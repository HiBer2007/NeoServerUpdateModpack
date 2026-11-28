#pragma once

#include <string>
#include <vector>

namespace NeoWorkspace {

struct SshKeyInfo {
    std::string path;
    std::string publicKey;
    std::string comment;
    std::string fingerprint;
};

struct SshConfig {
    std::string sshPath;
    std::string keyPath;
    std::string host = "github.com";
    std::string gitSshCommand;
};

std::string sshConfigPath();
SshConfig loadSshConfig();
bool saveSshConfig(const SshConfig& config, std::string& error);

std::string defaultSshPath();
std::string defaultSshKeygenPath();
std::string defaultSshKeyPath();
std::string effectiveSshCommand(const SshConfig& config);

std::vector<SshKeyInfo> listSshKeys();
bool generateSshKey(const std::string& keyPath, const std::string& comment,
    std::string& error);
std::string readSshPublicKey(const std::string& keyPath);
bool configureSshHost(const std::string& host, const std::string& keyPath,
    std::string& error);
bool testSshConnection(const std::string& host, const std::string& sshPath,
    const std::string& keyPath, std::string& output, std::string& error);

void applySshEnvironment();

} // namespace NeoWorkspace
