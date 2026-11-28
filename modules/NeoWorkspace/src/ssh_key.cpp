#include <ssh_key.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace NeoWorkspace {
namespace {

std::string envValue(const char* name)
{
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

std::string homeDir()
{
    std::string home = envValue("USERPROFILE");
    if (home.empty()) home = envValue("HOME");
    return home;
}

std::string quote(const std::string& path)
{
    if (path.empty()) return path;
    return "\"" + path + "\"";
}

std::string findInCandidates(const std::vector<std::string>& candidates)
{
    for (const auto& candidate : candidates) {
        if (candidate.empty()) continue;
        if (QFileInfo::exists(QString::fromStdString(candidate))) return candidate;
    }
    return {};
}

std::string findOnPath(const char* name)
{
    const QString found = QStandardPaths::findExecutable(QString::fromLatin1(name));
    return found.isEmpty() ? std::string() : found.toStdString();
}

std::string installGitRoot()
{
    return envValue("ProgramFiles") + "\\Git";
}

std::vector<std::string> sshCandidates(const std::string& configured)
{
    std::vector<std::string> list;
    if (!configured.empty()) list.push_back(configured);
    const std::string home = homeDir();
    list.push_back(installGitRoot() + "\\usr\\bin\\ssh.exe");
    list.push_back("C:\\Program Files\\Git\\usr\\bin\\ssh.exe");
    list.push_back("C:\\Program Files (x86)\\Git\\usr\\bin\\ssh.exe");
    list.push_back("C:\\Windows\\System32\\OpenSSH\\ssh.exe");
    return list;
}

std::vector<std::string> keygenCandidates()
{
    std::vector<std::string> list;
    list.push_back(installGitRoot() + "\\usr\\bin\\ssh-keygen.exe");
    list.push_back("C:\\Program Files\\Git\\usr\\bin\\ssh-keygen.exe");
    list.push_back("C:\\Program Files (x86)\\Git\\usr\\bin\\ssh-keygen.exe");
    list.push_back("C:\\Windows\\System32\\OpenSSH\\ssh-keygen.exe");
    return list;
}

bool runProcess(const std::string& program, const QStringList& args,
    std::string& output, std::string& error, int timeoutMs = 30000)
{
    QProcess process;
    process.setProgram(QString::fromStdString(program));
    process.setArguments(args);
    process.start();
    if (!process.waitForStarted(5000)) {
        error = "failed to start " + program;
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        error = "process timed out: " + program;
        return false;
    }
    output = QString::fromUtf8(process.readAllStandardOutput()).toStdString();
    const std::string stderrText = QString::fromUtf8(process.readAllStandardError()).toStdString();
    if (process.exitCode() != 0) {
        error = stderrText.empty() ? ("exit code " + std::to_string(process.exitCode())) : stderrText;
        return false;
    }
    if (!stderrText.empty()) output += stderrText;
    return true;
}

} // namespace

std::string sshConfigPath()
{
    const std::string home = homeDir();
    if (home.empty()) return {};
    return home + "\\.nsum\\git_ssh.json";
}

SshConfig loadSshConfig()
{
    SshConfig config;
    const std::string path = sshConfigPath();
    if (!path.empty() && QFileInfo::exists(QString::fromStdString(path))) {
        std::ifstream input(path);
        try {
            nlohmann::json json;
            input >> json;
            config.sshPath = json.value("ssh_path", "");
            config.keyPath = json.value("key_path", "");
            config.host = json.value("host", "github.com");
            config.gitSshCommand = json.value("git_ssh_command", "");
        } catch (...) {
        }
    }
    if (config.host.empty()) config.host = "github.com";
    if (config.keyPath.empty()) config.keyPath = defaultSshKeyPath();
    return config;
}

bool saveSshConfig(const SshConfig& config, std::string& error)
{
    const std::string path = sshConfigPath();
    if (path.empty()) {
        error = "cannot resolve user home";
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    if (ec) {
        error = "cannot create config directory: " + ec.message();
        return false;
    }
    nlohmann::json json;
    json["ssh_path"] = config.sshPath;
    json["key_path"] = config.keyPath;
    json["host"] = config.host;
    json["git_ssh_command"] = config.gitSshCommand;
    std::ofstream output(path);
    if (!output) {
        error = "cannot write " + path;
        return false;
    }
    output << json.dump(2);
    return true;
}

std::string defaultSshPath()
{
    return findInCandidates(sshCandidates(loadSshConfig().sshPath));
}

std::string defaultSshKeygenPath()
{
    return findInCandidates(keygenCandidates());
}

std::string defaultSshKeyPath()
{
    const std::string home = homeDir();
    if (home.empty()) return {};
    return home + "\\.ssh\\nsum_ed25519";
}

std::string effectiveSshCommand(const SshConfig& config)
{
    if (!config.sshPath.empty() && QFileInfo::exists(QString::fromStdString(config.sshPath))) {
        return quote(config.sshPath);
    }
    const std::string detected = defaultSshPath();
    return detected.empty() ? std::string() : quote(detected);
}

std::vector<SshKeyInfo> listSshKeys()
{
    std::vector<SshKeyInfo> keys;
    const std::string home = homeDir();
    if (home.empty()) return keys;
    const QStringList entries = QDir(QString::fromStdString(home + "\\.ssh"))
        .entryList({ "*.pub" }, QDir::Files);
    const std::string keygen = defaultSshKeygenPath();
    for (const auto& name : entries) {
        const std::string path = home + "\\.ssh\\" + name.toStdString();
        SshKeyInfo info;
        info.path = path;
        std::ifstream input(path);
        std::string line;
        std::getline(input, line);
        info.publicKey = line;
        const size_t lastSpace = line.rfind(' ');
        if (lastSpace != std::string::npos) info.comment = line.substr(lastSpace + 1);
        if (!keygen.empty()) {
            std::string output;
            std::string ignored;
            if (runProcess(keygen, { "-lf", QString::fromStdString(path) }, output, ignored, 10000)) {
                info.fingerprint = output;
            }
        }
        keys.push_back(info);
    }
    return keys;
}

bool generateSshKey(const std::string& keyPath, const std::string& comment,
    std::string& error)
{
    const std::string keygen = defaultSshKeygenPath();
    if (keygen.empty()) {
        error = "ssh-keygen.exe not found (install Git or OpenSSH)";
        return false;
    }
    if (QFileInfo::exists(QString::fromStdString(keyPath))
        || QFileInfo::exists(QString::fromStdString(keyPath + ".pub"))) {
        error = "key already exists: " + keyPath;
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(keyPath).parent_path(), ec);
    if (ec) {
        error = "cannot create key directory: " + ec.message();
        return false;
    }
    std::string output;
    const QStringList args = { "-t", "ed25519", "-f", QString::fromStdString(keyPath),
        "-N", "", "-C", QString::fromStdString(comment.empty() ? "nsum" : comment), "-q" };
    if (!runProcess(keygen, args, output, error, 60000)) return false;

    SshConfig config = loadSshConfig();
    config.keyPath = keyPath;
    if (config.sshPath.empty()) config.sshPath = defaultSshPath();
    config.gitSshCommand = effectiveSshCommand(config);
    std::string saveError;
    if (!saveSshConfig(config, saveError)) {
        error = saveError;
        return false;
    }
    return true;
}

std::string readSshPublicKey(const std::string& keyPath)
{
    std::string pubPath = keyPath;
    if (pubPath.size() < 4 || pubPath.substr(pubPath.size() - 4) != ".pub") pubPath += ".pub";
    std::ifstream input(pubPath);
    if (!input) {
        const std::string keygen = defaultSshKeygenPath();
        if (keygen.empty()) return {};
        std::string output;
        std::string error;
        if (runProcess(keygen, { "-y", "-f", QString::fromStdString(keyPath) }, output, error, 10000)) {
            return output;
        }
        return {};
    }
    std::string line;
    std::getline(input, line);
    return line;
}

bool configureSshHost(const std::string& host, const std::string& keyPath,
    std::string& error)
{
    const std::string home = homeDir();
    if (home.empty()) {
        error = "cannot resolve user home";
        return false;
    }
    const std::string sshDir = home + "\\.ssh";
    std::error_code ec;
    std::filesystem::create_directories(sshDir, ec);
    std::filesystem::create_directories(std::filesystem::path(keyPath).parent_path(), ec);

    const std::string configPath = sshDir + "\\config";
    std::string content;
    std::ifstream input(configPath);
    if (input) {
        std::string line;
        while (std::getline(input, line)) {
            content += line;
            content += "\n";
        }
    }
    const std::string begin = "# >>> NSUM SSH";
    const std::string end = "# <<< NSUM SSH";
    const size_t beginPos = content.find(begin);
    if (beginPos != std::string::npos) {
        const size_t endPos = content.find(end, beginPos);
        if (endPos != std::string::npos) {
            content.erase(beginPos, endPos + end.size() - beginPos);
            while (!content.empty() && (content.back() == '\n' || content.back() == '\r')) content.pop_back();
            content += "\n";
        }
    }
    const std::string block = begin + "\n"
        + "Host " + host + "\n"
        + "    IdentityFile " + quote(keyPath) + "\n"
        + "    IdentitiesOnly yes\n"
        + "    StrictHostKeyChecking accept-new\n"
        + end + "\n";
    content += block;

    std::ofstream output(configPath, std::ios::trunc);
    if (!output) {
        error = "cannot write " + configPath;
        return false;
    }
    output << content;
    return true;
}

bool testSshConnection(const std::string& host, const std::string& sshPath,
    const std::string& keyPath, std::string& output, std::string& error)
{
    std::string ssh = sshPath;
    if (ssh.empty() || !QFileInfo::exists(QString::fromStdString(ssh))) ssh = defaultSshPath();
    if (ssh.empty()) {
        error = "ssh.exe not found";
        return false;
    }
    QStringList args = { "-T", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=accept-new",
        "-o", "ConnectTimeout=10" };
    if (!keyPath.empty()) {
        args << "-i" << QString::fromStdString(keyPath);
    }
    args << QString::fromStdString("git@" + host);
    QProcess process;
    process.setProgram(QString::fromStdString(ssh));
    process.setArguments(args);
    process.start();
    if (!process.waitForStarted(5000)) {
        error = "failed to start ssh";
        return false;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        error = "ssh connection timed out (30s)";
        return false;
    }
    output = QString::fromUtf8(process.readAllStandardOutput()).toStdString()
        + QString::fromUtf8(process.readAllStandardError()).toStdString();
    const bool authenticated = output.find("successfully authenticated") != std::string::npos;
    if (process.exitCode() != 0 && !authenticated) {
        error = output.empty() ? ("ssh exit code " + std::to_string(process.exitCode())) : output;
        return false;
    }
    return true;
}

void applySshEnvironment()
{
    if (!envValue("GIT_SSH_COMMAND").empty()) return;
    const SshConfig config = loadSshConfig();
    const std::string command = effectiveSshCommand(config);
    if (!command.empty()) {
        qputenv("GIT_SSH_COMMAND", QByteArray::fromStdString(command));
    }
}

} // namespace NeoWorkspace
