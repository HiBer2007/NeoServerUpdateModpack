#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QProcess>

struct InstallConfig {
    std::string installPath;
    bool installEditor = false;
    bool useSystemGit = true;
    std::string gitPath = "git";
    bool valid = false;
    std::string errorMsg;

    static InstallConfig load()
    {
        InstallConfig cfg;
        QString exeDir = QCoreApplication::applicationDirPath();

        // 1. Try install.conf next to exe or one level up
        QStringList searchPaths = {
            exeDir + "/install.conf",
            exeDir + "/../install.conf"
        };
        std::string confPath;
        for (auto& p : searchPaths) {
            if (QFileInfo::exists(p)) { confPath = p.toStdString(); break; }
        }

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
            cfg.useSystemGit = (kv["use_system_git"] == "true");
            if (!kv["git_path"].empty()) cfg.gitPath = kv["git_path"];
        }

        // 2. If using system git, check it actually works
        //    (Git for Windows first launch can exceed a few seconds;
        //     give it a generous window before declaring "not found").
        if (cfg.useSystemGit || cfg.gitPath == "git") {
            QProcess test;
            test.start("git", {"--version"});
            test.waitForFinished(15000);
            if (test.exitCode() == 0) {
                cfg.valid = true;
                cfg.useSystemGit = true;
                cfg.gitPath = "git";
                return cfg;
            }
        }

        // 3. Try bundled git path from install.conf
        if (!cfg.gitPath.empty() && cfg.gitPath != "git") {
            if (QFileInfo::exists(QString::fromStdString(cfg.gitPath))) {
                QProcess test;
                test.start(QString::fromStdString(cfg.gitPath), {"--version"});
                test.waitForFinished(3000);
                if (test.exitCode() == 0) {
                    cfg.valid = true;
                    cfg.useSystemGit = false;
                    return cfg;
                }
            }
        }

        // 4. Search tools/git/ relative to exe (portable layout)
        QString portableGit = QDir(exeDir).absoluteFilePath("tools/git/bin/git.exe");
        if (QFileInfo::exists(portableGit)) {
            QProcess test;
            test.start(portableGit, {"--version"});
            test.waitForFinished(3000);
            if (test.exitCode() == 0) {
                cfg.valid = true;
                cfg.useSystemGit = false;
                cfg.gitPath = portableGit.toStdString();
                return cfg;
            }
        }
        // Also try one level up (dev layout: build/deploy/tools/git/...)
        portableGit = QDir(exeDir).absoluteFilePath("../tools/git/bin/git.exe");
        if (QFileInfo::exists(portableGit)) {
            QProcess test;
            test.start(portableGit, {"--version"});
            test.waitForFinished(3000);
            if (test.exitCode() == 0) {
                cfg.valid = true;
                cfg.useSystemGit = false;
                cfg.gitPath = portableGit.toStdString();
                return cfg;
            }
        }

        // 5. Fatal: no git found anywhere
        cfg.valid = false;
        cfg.errorMsg =
            "FATAL: Git is required but was not found.\n\n"
            "Checked:\n"
            "  - System PATH (git --version)\n"
            "  - " + cfg.gitPath + "\n"
            "  - " + portableGit.toStdString() + "\n\n"
            "Solutions:\n"
            "  1. Install Git from https://git-scm.com/download/win\n"
            "  2. Re-run NSUM_Installer_1.0.0.exe (install/repair) to bundle Git\n"
            "  3. Set use_system_git=false and git_path=<path> in install.conf\n";
        return cfg;
    }
};
