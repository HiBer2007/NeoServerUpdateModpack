#pragma once

#include <string>
#include <vector>

namespace NeoInstaller {

struct InstallProgress {
    int percent;
    std::string currentFile;
    std::string stepDescription;
    bool finished;
    bool error;
    std::string errorMessage;
};

class SilentInstaller {
public:
    enum GitMode { Auto, UseSystem, UseBundled };

    SilentInstaller();

    void setInstallEditor(bool on) { installEditor_ = on; }
    void setGitMode(GitMode m) { gitMode_ = m; }
    bool run(const std::string& installPath = "");

private:
    std::string installPath_;
    bool installEditor_ = false;
    GitMode gitMode_ = Auto;

    bool createDirectories(const std::string& path);
    bool createShortcuts(const std::string& installPath);
    bool createDesktopShortcut(const std::string& targetPath, const std::string& installPath);
    bool createStartMenuShortcut(const std::string& targetPath, const std::string& installPath);
    void writeInstallConfig(const std::string& installPath);
    std::string getDefaultInstallPath();
};

} // namespace NeoInstaller
