#pragma once

#include <string>

namespace NeoInstaller {

class GitChecker {
public:
    GitChecker();

    bool isGitInstalled() const;
    std::string findGitPath();
    std::string getGitVersion();
    std::string getDownloadUrl() const;
    bool isValidGitVersion();

private:
    std::string gitPath_;
    std::string gitVersion_;
    bool checked_;
    bool found_;

    bool searchPath();
    bool runGitVersion();
};

} // namespace NeoInstaller
