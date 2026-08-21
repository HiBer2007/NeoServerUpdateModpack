#include "git_operations.h"
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <logger.h>
#include <git_analyzer.h>

namespace NeoWorkspace {

std::string GitOperations::defaultGitPath_;

namespace {
// git 的 safe.directory 按正斜杠规范化路径匹配, 反斜杠条目不生效 (2026-08-20 实测)
std::string toForwardSlashes(std::string p)
{
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}
} // namespace

GitOperations::GitOperations(const std::string& gitPath)
    : gitPath_(gitPath)
    , lastError_()
{
    if (gitPath_.empty()) {
        gitPath_ = "git";
    }
}

GitResult GitOperations::clone(const std::string& url, const std::string& targetDir, int timeoutMs)
{
    CLogger::Info("GitOperations: Cloning {} to {}", url, targetDir);
    GitResult r = execute({"clone", url, targetDir}, "", timeoutMs);
    if (r.errorCode != NeoCore::ErrorCode::Success) {
        lastError_ = NeoCore::AnalyzeGitError(r.stderrOutput);
        CLogger::Error("GitOperations: Clone failed: {}", lastError_);
    }
    else {
        CLogger::Info("GitOperations: Clone successful");
    }
    return r;
}

GitResult GitOperations::pull(const std::string& repoDir, int timeoutMs)
{
    CLogger::Info("GitOperations: Pulling in {}", repoDir);
    GitResult r = execute({"pull"}, repoDir, timeoutMs);
    if (r.errorCode != NeoCore::ErrorCode::Success) {
        lastError_ = NeoCore::AnalyzeGitError(r.stderrOutput);
        CLogger::Error("GitOperations: Pull failed: {}", lastError_);
    }
    else {
        CLogger::Info("GitOperations: Pull successful");
    }
    return r;
}

GitResult GitOperations::fetch(const std::string& repoDir, const std::string& remote)
{
    CLogger::Info("GitOperations: Fetching {} in {}", remote, repoDir);
    GitResult r = execute({"fetch", remote}, repoDir, 60000);
    if (r.errorCode != NeoCore::ErrorCode::Success) {
        lastError_ = NeoCore::AnalyzeGitError(r.stderrOutput);
        CLogger::Error("GitOperations: Fetch failed: {}", lastError_);
    }
    else {
        CLogger::Info("GitOperations: Fetch successful");
    }
    return r;
}

GitResult GitOperations::checkout(const std::string& repoDir, const std::string& branch)
{
    CLogger::Info("GitOperations: Checking out '{}' in {}", branch, repoDir);
    GitResult r = execute({"checkout", branch}, repoDir, 30000);
    if (r.errorCode != NeoCore::ErrorCode::Success) {
        lastError_ = NeoCore::AnalyzeGitError(r.stderrOutput);
        CLogger::Error("GitOperations: Checkout failed: {}", lastError_);
    }
    else {
        CLogger::Info("GitOperations: Checkout '{}' successful", branch);
    }
    return r;
}

GitResult GitOperations::listBranches(const std::string& repoDir)
{
    return execute({"branch"}, repoDir, 15000);
}

GitResult GitOperations::createBranch(const std::string& repoDir,
    const std::string& branch, const std::string& baseBranch)
{
    CLogger::Info("GitOperations: Creating branch '{}' in {}", branch, repoDir);
    std::vector<std::string> args = { "branch", branch };
    if (!baseBranch.empty()) {
        args.push_back(baseBranch);
    }
    GitResult r = execute(args, repoDir, 30000);
    if (r.errorCode != NeoCore::ErrorCode::Success) {
        lastError_ = NeoCore::AnalyzeGitError(r.stderrOutput);
        CLogger::Error("GitOperations: Create branch failed: {}", lastError_);
    }
    return r;
}

GitResult GitOperations::currentBranch(const std::string& repoDir)
{
    return execute({"rev-parse", "--abbrev-ref", "HEAD"}, repoDir, 10000);
}

GitResult GitOperations::listRemoteBranches(const std::string& repoDir)
{
    return execute({"branch", "-r"}, repoDir, 15000);
}

GitResult GitOperations::status(const std::string& repoDir)
{
    return execute({"status", "--porcelain"}, repoDir, 15000);
}

GitResult GitOperations::revParse(const std::string& repoDir, const std::string& ref)
{
    return execute({"rev-parse", ref}, repoDir, 10000);
}

GitResult GitOperations::log(const std::string& repoDir, const std::string& format,
    int maxCount)
{
    return execute({"log", "--format=" + format, "-n", std::to_string(maxCount)},
        repoDir, 15000);
}

GitResult GitOperations::lsFiles(const std::string& repoDir)
{
    return execute({"ls-files"}, repoDir, 15000);
}

bool GitOperations::isGitRepository(const std::string& dir)
{
    GitResult r = execute({"rev-parse", "--git-dir"}, dir, 5000);
    return r.exitCode == 0 && r.errorCode == NeoCore::ErrorCode::Success;
}

bool GitOperations::hasRemote(const std::string& dir)
{
    GitResult r = execute({"remote"}, dir, 5000);
    if (r.exitCode != 0 || r.errorCode != NeoCore::ErrorCode::Success)
        return false;
    // git remote 输出每个远程名一行; 空白则无远程
    for (const char c : r.stdoutOutput) {
        if (!std::isspace(static_cast<unsigned char>(c)))
            return true;
    }
    return false;
}

std::string GitOperations::lastError() const
{
    return lastError_;
}

bool GitOperations::isDubiousOwnership(const std::string& dir)
{
    GitResult r = execute({"rev-parse", "--git-dir"}, dir, 5000);
    return r.stderrOutput.find("detected dubious ownership") != std::string::npos;
}

bool GitOperations::isTrustedRepository(const std::string& dir)
{
    GitResult r = execute({"config", "--get-all", "safe.directory"}, "", 5000);
    if (r.exitCode != 0 && r.exitCode != 1) return false;
    const QString target = QString::fromStdString(toForwardSlashes(dir));
    std::istringstream stream(r.stdoutOutput);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "*") return true;
        const QString entry = QString::fromStdString(toForwardSlashes(line));
        if (entry.compare(target, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

GitResult GitOperations::trustRepository(const std::string& dir)
{
    return execute({"config", "--global", "--add", "safe.directory",
        toForwardSlashes(dir)}, "", 10000);
}

GitResult GitOperations::execute(const std::vector<std::string>& args,
    const std::string& workingDir, int timeoutMs)
{
    GitResult result;
    result.exitCode = -1;
    result.errorCode = NeoCore::ErrorCode::Success;

    QProcess process;

    if (!workingDir.empty()) {
        process.setWorkingDirectory(QString::fromStdString(workingDir));
    }
    else {
        process.setWorkingDirectory(QDir::currentPath());
    }

    process.setProgram(QString::fromStdString(gitPath_));

    QStringList qArgs;
    for (const auto& arg : args) {
        qArgs << QString::fromStdString(arg);
    }
    process.setArguments(qArgs);

    process.setProcessChannelMode(QProcess::SeparateChannels);

    try {
        process.start();

        if (!process.waitForStarted(5000)) {
            result.errorCode = NeoCore::ErrorCode::GitNotFound;
            result.stderrOutput = "Failed to start git process: " +
                process.errorString().toStdString();
            result.stdoutOutput = "";
            result.exitCode = -1;
            CLogger::Error("GitOperations: Failed to start git: {}",
                process.errorString().toStdString());
            lastError_ = result.stderrOutput;
            return result;
        }

        if (!process.waitForFinished(timeoutMs)) {
            CLogger::Warn("GitOperations: Git process timed out after {} ms, killing", timeoutMs);
            process.kill();
            if (!process.waitForFinished(5000)) {
                process.terminate();
                process.waitForFinished(3000);
            }
            result.errorCode = NeoCore::ErrorCode::GitTimeout;
            result.stderrOutput = "Git process timed out";
            result.stdoutOutput = QString::fromUtf8(process.readAllStandardOutput()).toStdString();
            result.exitCode = -1;
            lastError_ = result.stderrOutput;
            return result;
        }

        result.exitCode = process.exitCode();
        result.stdoutOutput = QString::fromUtf8(process.readAllStandardOutput()).toStdString();
        result.stderrOutput = QString::fromUtf8(process.readAllStandardError()).toStdString();

        // git 输出实时挂日志/终端 (不再静默)
        if (!result.stdoutOutput.empty()) {
            CLogger::Info("Git out: {}", result.stdoutOutput);
        }
        if (!result.stderrOutput.empty()) {
            CLogger::Warn("Git err: {}", result.stderrOutput);
        }

        if (process.exitStatus() == QProcess::CrashExit) {
            result.errorCode = NeoCore::ErrorCode::GitCrash;
            result.stderrOutput += " (process crashed)";
            CLogger::Error("GitOperations: Git process crashed, exitCode={}", result.exitCode);
            lastError_ = result.stderrOutput;
        }
        else if (result.exitCode != 0) {
            result.errorCode = NeoCore::ErrorCode::Unknown;
            if (!result.stderrOutput.empty()) {
                lastError_ = NeoCore::AnalyzeGitError(result.stderrOutput);
            }
        }

        CLogger::Debug("GitOperations: git {} -> exitCode={}", args[0], result.exitCode);
    }
    catch (const std::exception& e) {
        result.errorCode = NeoCore::ErrorCode::GitCrash;
        result.stderrOutput = std::string("Exception during git execution: ") + e.what();
        result.exitCode = -1;
        CLogger::Error("GitOperations: Exception: {}", e.what());
        lastError_ = result.stderrOutput;
    }

    return result;
}

GitResult GitOperations::init(const std::string& dir) {
    return execute({"init"}, dir, 10000);
}

GitResult GitOperations::addRemote(const std::string& dir, const std::string& name, const std::string& url) {
    return execute({"remote", "add", name, url}, dir, 10000);
}

GitResult GitOperations::addAll(const std::string& dir) {
    return execute({"add", "-A"}, dir, 30000);
}

GitResult GitOperations::commit(const std::string& dir, const std::string& message) {
    return execute({"commit", "-m", message}, dir, 10000);
}

GitResult GitOperations::push(const std::string& dir, const std::string& remote, const std::string& branch) {
    std::vector<std::string> args = {"push", "-u"};
    if (!remote.empty()) args.push_back(remote);
    if (!branch.empty()) args.push_back(branch);
    return execute(args, dir, 60000);
}

GitResult GitOperations::generateSshKey(const std::string& keyPath, const std::string& comment,
    const std::string& type)
{
    QFileInfo fi(QString::fromStdString(keyPath));
    QDir().mkpath(fi.absolutePath());

    std::vector<std::string> args = {"-t", type, "-f", keyPath, "-N", "", "-q"};
    if (!comment.empty()) args.insert(args.end(), {"-C", comment});
    return execute(args, "", 10000);
}

GitResult GitOperations::testSshConnection(const std::string& host)
{
    return execute({"-T", "git@" + host, "-o", "StrictHostKeyChecking=accept-new",
        "-o", "BatchMode=yes"}, "", 15000);
}

std::string GitOperations::defaultSshKeyPath()
{
    QString home = QDir::homePath();
    return (home + "/.ssh/id_ed25519").toStdString();
}

std::string GitOperations::readPublicKey(const std::string& keyPath)
{
    QFile f(QString::fromStdString(keyPath + ".pub"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return "";
    return f.readAll().trimmed().toStdString();
}

} // namespace NeoWorkspace

