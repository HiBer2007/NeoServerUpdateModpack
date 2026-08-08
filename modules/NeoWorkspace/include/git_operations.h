#pragma once

#include <string>
#include <vector>
#include <error_codes.h>

namespace NeoWorkspace {

struct GitResult {
    int exitCode;
    std::string stdoutOutput;
    std::string stderrOutput;
    NeoCore::ErrorCode errorCode;
};

class GitOperations {
public:
    GitOperations(const std::string& gitPath = GetDefaultGitPath());

    static void SetDefaultGitPath(const std::string& path) { defaultGitPath_ = path; }
    static std::string GetDefaultGitPath() { return defaultGitPath_.empty() ? "git" : defaultGitPath_; }

    GitResult clone(const std::string& url, const std::string& targetDir, int timeoutMs = 120000);
    GitResult pull(const std::string& repoDir, int timeoutMs = 60000);
    GitResult fetch(const std::string& repoDir, const std::string& remote = "origin");
    GitResult checkout(const std::string& repoDir, const std::string& branch);
    GitResult createBranch(const std::string& repoDir, const std::string& branch,
        const std::string& baseBranch = "");
    GitResult currentBranch(const std::string& repoDir);
    GitResult listBranches(const std::string& repoDir);
    GitResult listRemoteBranches(const std::string& repoDir);
    GitResult status(const std::string& repoDir);
    GitResult revParse(const std::string& repoDir, const std::string& ref = "HEAD");
    GitResult log(const std::string& repoDir, const std::string& format = "%H %s", int maxCount = 10);
    GitResult lsFiles(const std::string& repoDir);

    GitResult init(const std::string& dir);
    GitResult addRemote(const std::string& dir, const std::string& name, const std::string& url);
    GitResult addAll(const std::string& dir);
    GitResult commit(const std::string& dir, const std::string& message);
    GitResult push(const std::string& dir, const std::string& remote = "origin", const std::string& branch = "");

    // SSH support
    GitResult generateSshKey(const std::string& keyPath, const std::string& comment = "",
        const std::string& type = "ed25519");
    GitResult testSshConnection(const std::string& host = "github.com");
    static std::string defaultSshKeyPath();
    static std::string readPublicKey(const std::string& keyPath);

    bool isGitRepository(const std::string& dir);
    // 仓库是否配置了远程 (git remote 非空); 本地仓库无 remote 时同步操作应跳过
    bool hasRemote(const std::string& dir);
    std::string lastError() const;

private:
    std::string gitPath_;
    std::string lastError_;
    static std::string defaultGitPath_;

    GitResult execute(const std::vector<std::string>& args,
        const std::string& workingDir = "", int timeoutMs = 30000);
};

} // namespace NeoWorkspace

