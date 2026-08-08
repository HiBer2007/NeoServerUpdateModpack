#pragma once

#include <string>
#include <cstdint>

namespace NeoBuild {

std::string getAppDataDir();
std::string getCacheDir();
std::string getConfigDir();
std::string getTempDir();
std::string getDefaultWorkspaceDir();

bool isWindows();
bool isLinux();
bool isMacOS();
std::string platformName();

std::string findGitExecutable();
bool isGitAvailable();

std::string getFreeDiskSpace(const std::string& path);
uint64_t getFreeDiskBytes(const std::string& path);

} // namespace NeoBuild
