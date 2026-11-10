#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QStringList>

/// 运行时 Git 配置。
///
/// 规则（2026-11-09 用户裁定）：**必须使用内置 Git，不依赖系统 Git**。
/// 解析顺序：
///   1. install.conf 的 git_path（内置安装时写入，MinGit/PortableGit 均可）；
///   2. <exeDir>/tools/git 下的可执行文件（MinGit: mingw64/bin 或 cmd；PortableGit: bin）；
///   3. <exeDir>/../tools/git 同款（开发布局 build/deploy + build/tools/git）；
///   4. 仅当前三步都找不到时才回退系统 PATH 的 git（并标记 useSystemGit）。
struct InstallConfig {
    std::string installPath;
    bool installEditor = false;
    bool useSystemGit = false;
    std::string gitPath = "git";
    std::string gitVersion;
    bool valid = false;
    std::string errorMsg;

    /// 在 gitDir 下按 MinGit / PortableGit 布局找可运行的 git.exe。
    static std::string findRunnableGit(const QString& gitDir)
    {
        const QStringList candidates = {
            QDir(gitDir).absoluteFilePath("bin/git.exe"),
            QDir(gitDir).absoluteFilePath("mingw64/bin/git.exe"),
            QDir(gitDir).absoluteFilePath("ucrt64/bin/git.exe"),
            QDir(gitDir).absoluteFilePath("cmd/git.exe"),
        };
        for (const auto& candidate : candidates) {
            if (!QFileInfo::exists(candidate)) continue;
            QProcess test;
            test.start(candidate, {"--version"});
            test.waitForFinished(8000);
            if (test.exitCode() == 0) {
                return candidate.toStdString();
            }
        }
        return {};
    }

    static std::string probeVersion(const std::string& exe)
    {
        if (exe.empty()) return {};
        QProcess test;
        test.start(QString::fromStdString(exe), {"--version"});
        test.waitForFinished(15000);
        if (test.exitCode() != 0) return {};
        return QString::fromUtf8(test.readAllStandardOutput()).trimmed().toStdString();
    }

    static InstallConfig load()
    {
        InstallConfig cfg;
        QString exeDir = QCoreApplication::applicationDirPath();

        QStringList searchPaths = { exeDir + "/install.conf", exeDir + "/../install.conf" };
        std::string confPath;
        for (auto& p : searchPaths) {
            if (QFileInfo::exists(p)) { confPath = p.toStdString(); break; }
        }

        std::string confGitPath;
        bool confUseSystemGit = false;
        if (!confPath.empty()) {
            std::ifstream f(confPath);
            std::string line;
            std::unordered_map<std::string, std::string> kv;
            while (std::getline(f, line)) {
                if (line.empty() || line[0] == '#') continue;
                size_t eq = line.find('=');
                if (eq == std::string::npos) continue;
                kv[line.substr(0, eq)] = line.substr(eq + 1);
            }
            cfg.installPath = kv["install_path"];
            cfg.installEditor = (kv["install_editor"] == "true");
            confUseSystemGit = (kv["use_system_git"] == "true");
            confGitPath = kv["git_path"];
        }

        // 1. install.conf 里记录的内置 git
        if (!confGitPath.empty() && confGitPath != "git"
            && QFileInfo::exists(QString::fromStdString(confGitPath))) {
            std::string version = probeVersion(confGitPath);
            if (!version.empty()) {
                cfg.gitPath = confGitPath;
                cfg.gitVersion = version;
                cfg.useSystemGit = false;
                cfg.valid = true;
                return cfg;
            }
        }

        // 2/3. exe 旁或上一级的 tools/git（内置）
        const QStringList bundledDirs = {
            QDir(exeDir).absoluteFilePath("tools/git"),
            QDir(exeDir).absoluteFilePath("../tools/git"),
        };
        for (const auto& dir : bundledDirs) {
            if (!QFileInfo::exists(dir)) continue;
            std::string exe = findRunnableGit(dir);
            if (exe.empty()) continue;
            cfg.gitPath = exe;
            cfg.gitVersion = probeVersion(exe);
            cfg.useSystemGit = false;
            cfg.valid = true;
            return cfg;
        }

        // 4. 回退系统 Git（仅当确实没有内置 Git，且用户没有明确要求内置）
        if (!confUseSystemGit) {
            QProcess test;
            test.start("git", {"--version"});
            test.waitForFinished(15000);
            if (test.exitCode() == 0) {
                cfg.gitPath = "git";
                cfg.gitVersion = QString::fromUtf8(test.readAllStandardOutput()).trimmed().toStdString();
                cfg.useSystemGit = true;
                cfg.valid = true;
                return cfg;
            }
        }

        cfg.valid = false;
        cfg.errorMsg =
            "FATAL: 未找到可用的 Git。\n\n"
            "NSUM 内置 Git 应位于 <安装目录>/tools/git（MinGit 或 PortableGit）。\n"
            "已检查：\n"
            "  - install.conf 的 git_path\n"
            "  - " + QDir(exeDir).absoluteFilePath("tools/git").toStdString() + "\n"
            "  - " + QDir(exeDir).absoluteFilePath("../tools/git").toStdString() + "\n\n"
            "修复：运行 NeoServerUpdateModpack.exe exec git-update 下载/更新内置 Git，"
            "或重跑安装器的「修复」。";
        return cfg;
    }
};
