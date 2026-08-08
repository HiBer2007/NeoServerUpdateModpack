#include "platform_api.h"
#include <logger.h>
#include <filesystem>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QStorageInfo>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <fileapi.h>
#else
#include <sys/statvfs.h>
#include <unistd.h>
#include <pwd.h>
#endif

namespace NeoBuild {

namespace fs = std::filesystem;

std::string getAppDataDir()
{
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
        SHGFP_TYPE_CURRENT, path))) {
        return (fs::path(path) / "NeoServerUpdateModpack").string();
    }
    return (fs::path(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation).toStdString())).string();
#else
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    if (xdgData && xdgData[0] != '\0') {
        return (fs::path(xdgData) / "NeoServerUpdateModpack").string();
    }
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return (fs::path(home) / ".local" / "share" / "NeoServerUpdateModpack").string();
    }
    return (fs::path(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation).toStdString())).string();
#endif
}

std::string getCacheDir()
{
    return (fs::path(getAppDataDir()) / "cache").string();
}

std::string getConfigDir()
{
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr,
        SHGFP_TYPE_CURRENT, path))) {
        return (fs::path(path) / "NeoServerUpdateModpack").string();
    }
    return (fs::path(QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation).toStdString())).string();
#else
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig && xdgConfig[0] != '\0') {
        return (fs::path(xdgConfig) / "NeoServerUpdateModpack").string();
    }
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return (fs::path(home) / ".config" / "NeoServerUpdateModpack").string();
    }
    return (fs::path(QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation).toStdString())).string();
#endif
}

std::string getTempDir()
{
    return QStandardPaths::writableLocation(
        QStandardPaths::TempLocation).toStdString();
}

std::string getDefaultWorkspaceDir()
{
#ifdef _WIN32
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile && userProfile[0] != '\0') {
        return (fs::path(userProfile) / "NeoServerWorkspace").string();
    }
#else
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return (fs::path(home) / "NeoServerWorkspace").string();
    }
#endif
    return (fs::path(QDir::homePath().toStdString()) /
        "NeoServerWorkspace").string();
}

bool isWindows()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

bool isLinux()
{
#ifdef __linux__
    return true;
#else
    return false;
#endif
}

bool isMacOS()
{
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
}

std::string platformName()
{
#ifdef _WIN32
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#elif defined(__APPLE__)
    return "macOS";
#else
    return "Unknown";
#endif
}

std::string findGitExecutable()
{
    QStringList searchCommands;

#ifdef _WIN32
    searchCommands << "where" << "git";
#else
    searchCommands << "which" << "git";
#endif

    for (const auto& cmd : searchCommands) {
        QProcess proc;
        proc.setProgram(cmd);

        if (cmd == "where") {
            proc.setArguments({"git"});
        } else if (cmd == "which") {
            proc.setArguments({"git"});
        } else {
            proc.setProgram(cmd);
        }

        proc.start();
        if (!proc.waitForFinished(5000)) {
            continue;
        }

        QString output = QString::fromLocal8Bit(
            proc.readAllStandardOutput()).trimmed();
        QString errorOutput = QString::fromLocal8Bit(
            proc.readAllStandardError()).trimmed();

        QStringList lines;
        if (!output.isEmpty()) {
            lines = output.split('\n', Qt::SkipEmptyParts);
        }

        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;

            fs::path candidate(trimmed.toStdString());
            if (fs::exists(candidate)) {
                CLogger::Info("platform_api: found git at {}", candidate.string());
                return candidate.string();
            }
        }

        if (!errorOutput.isEmpty()) {
            CLogger::Debug("platform_api: {} error: {}",
                cmd.toStdString(), errorOutput.toStdString());
        }
    }

    QStringList commonPaths;
#ifdef _WIN32
    commonPaths << "C:\\Program Files\\Git\\bin\\git.exe"
                << "C:\\Program Files (x86)\\Git\\bin\\git.exe"
                << (QDir::homePath() + "\\scoop\\apps\\git\\current\\bin\\git.exe");
#else
    commonPaths << "/usr/bin/git"
                << "/usr/local/bin/git"
                << "/opt/homebrew/bin/git";
#endif

    for (const auto& p : commonPaths) {
        if (fs::exists(fs::path(p.toStdString()))) {
            CLogger::Info("platform_api: found git at {}", p.toStdString());
            return p.toStdString();
        }
    }

    CLogger::Warn("platform_api: git executable not found");
    return "";
}

bool isGitAvailable()
{
    std::string gitPath = findGitExecutable();
    if (gitPath.empty()) return false;

    QProcess proc;
    proc.setProgram(QString::fromStdString(gitPath));
    proc.setArguments({"--version"});
    proc.start();

    if (!proc.waitForFinished(5000)) {
        return false;
    }

    int exitCode = proc.exitCode();
    if (exitCode != 0) {
        return false;
    }

    QString output = QString::fromLocal8Bit(proc.readAllStandardOutput());
    return output.contains("git version", Qt::CaseInsensitive);
}

std::string getFreeDiskSpace(const std::string& path)
{
    uint64_t bytes = getFreeDiskBytes(path);
    if (bytes == 0) return "Unknown";

    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unitIdx = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIdx < 5) {
        size /= 1024.0;
        ++unitIdx;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIdx];
    return oss.str();
}

uint64_t getFreeDiskBytes(const std::string& path)
{
    try {
        QStorageInfo storage(QString::fromStdString(path));
        if (storage.isValid() && storage.bytesAvailable() > 0) {
            return static_cast<uint64_t>(storage.bytesAvailable());
        }

#ifdef _WIN32
        fs::path fsPath(path);
        std::string rootPath;

        if (fsPath.has_root_path()) {
            rootPath = fsPath.root_path().string();
        } else {
            rootPath = fs::current_path().root_path().string();
        }

        if (rootPath.empty()) {
            rootPath = "C:\\";
        }

        ULARGE_INTEGER freeBytesAvailable;
        ULARGE_INTEGER totalNumberOfBytes;
        ULARGE_INTEGER totalNumberOfFreeBytes;

        if (GetDiskFreeSpaceExA(rootPath.c_str(),
            &freeBytesAvailable,
            &totalNumberOfBytes,
            &totalNumberOfFreeBytes)) {
            return freeBytesAvailable.QuadPart;
        }

        DWORD err = GetLastError();
        CLogger::Warn("platform_api: GetDiskFreeSpaceEx failed: error {}", err);
        return 0;
#else
        struct statvfs stat;
        if (statvfs(path.c_str(), &stat) == 0) {
            return static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;
        }

        CLogger::Warn("platform_api: statvfs failed for {}", path);
        return 0;
#endif
    } catch (const std::exception& e) {
        CLogger::Error("platform_api::getFreeDiskBytes exception: {}", e.what());
        return 0;
    }
}

} // namespace NeoBuild

