#include "git_checker.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <algorithm>
#include <sstream>
#include <cstdlib>

namespace NeoInstaller {

GitChecker::GitChecker()
    : checked_(false)
    , found_(false)
{
}

bool GitChecker::searchPath()
{
#ifdef _WIN32
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return false;

    std::string pathStr(pathEnv);
    std::stringstream ss(pathStr);
    std::string dirPath;

    while (std::getline(ss, dirPath, ';')) {
        if (dirPath.empty()) continue;
        QString qdirPath = QString::fromStdString(dirPath);
        QString gitExe = QDir(qdirPath).filePath("git.exe");
        if (QFileInfo::exists(gitExe)) {
            gitPath_ = gitExe.toStdString();
            return true;
        }
    }

    const char* commonPaths[] = {
        "C:\\Program Files\\Git\\bin\\git.exe",
        "C:\\Program Files (x86)\\Git\\bin\\git.exe",
        "C:\\Git\\bin\\git.exe",
    };

    for (const char* cp : commonPaths) {
        if (QFileInfo::exists(cp)) {
            gitPath_ = cp;
            return true;
        }
    }
#else
    QProcess process;
    process.start("which", QStringList() << "git");
    process.waitForFinished(5000);
    if (process.exitCode() == 0) {
        QString path = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            gitPath_ = path.toStdString();
            return true;
        }
    }

    const char* commonPaths[] = {
        "/usr/bin/git",
        "/usr/local/bin/git",
        "/opt/homebrew/bin/git",
    };

    for (const char* cp : commonPaths) {
        if (QFileInfo::exists(cp)) {
            gitPath_ = cp;
            return true;
        }
    }
#endif
    return false;
}

bool GitChecker::runGitVersion()
{
    if (gitPath_.empty()) return false;

    QProcess process;
    process.start(QString::fromStdString(gitPath_), QStringList() << "--version");
    if (!process.waitForFinished(5000)) return false;

    if (process.exitCode() != 0) return false;

    gitVersion_ = QString::fromUtf8(process.readAllStandardOutput()).trimmed().toStdString();
    return !gitVersion_.empty();
}

bool GitChecker::isGitInstalled() const
{
    return const_cast<GitChecker*>(this)->findGitPath().empty() == false;
}

std::string GitChecker::findGitPath()
{
    if (checked_) return gitPath_;

    checked_ = true;
    found_ = searchPath();

    if (found_) {
        if (!runGitVersion()) {
            gitPath_.clear();
            gitVersion_.clear();
            found_ = false;
        }
    }

    return gitPath_;
}

std::string GitChecker::getGitVersion()
{
    if (gitVersion_.empty()) {
        findGitPath();
    }
    return gitVersion_;
}

std::string GitChecker::getDownloadUrl() const
{
    return "https://git-scm.com/download/win";
}

bool GitChecker::isValidGitVersion()
{
    std::string version = getGitVersion();
    if (version.empty()) return false;

    std::string prefix = "git version ";
    std::string verNum;
    if (version.size() > prefix.size() &&
        version.substr(0, prefix.size()) == prefix) {
        verNum = version.substr(prefix.size());
    } else {
        verNum = version;
    }

    std::stringstream ss(verNum);
    std::string segment;
    std::vector<int> parts;
    while (std::getline(ss, segment, '.')) {
        try {
            parts.push_back(std::stoi(segment));
        } catch (...) {
            break;
        }
        if (parts.size() >= 3) break;
    }

    if (parts.empty()) return false;

    if (parts[0] > 2) return true;
    if (parts[0] < 2) return false;
    if (parts.size() >= 2 && parts[1] >= 30) return true;
    return false;
}

} // namespace NeoInstaller
