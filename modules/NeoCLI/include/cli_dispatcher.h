#pragma once

#include <string>
#include <cancel_token.h>
#include "arg_parser.h"

namespace NeoCLI {

class CliDispatcher {
public:
    CliDispatcher();

    int dispatch(const CliCommand& cmd);

    void setGitConfig(const std::string& gitPath, bool useSystemGit);
    bool isCancelled() const;
    void cancel();

private:
    NeoCore::CancelToken cancelToken_;
    std::string gitPath_;
    bool useSystemGit_ = false;

    int dispatchInfo(const CliCommand& cmd);
    int dispatchFlow(const CliCommand& cmd);
    int dispatchExec(const CliCommand& cmd);

    int cmdFlowConsole(const CliCommand& cmd);

    int cmdInfoVersion(const CliCommand& cmd);
    int cmdInfoSystem(const CliCommand& cmd);
    int cmdInfoGit(const CliCommand& cmd);
    int cmdInfoWorkspace(const CliCommand& cmd);
    int cmdInfoPreview(const CliCommand& cmd);
    int cmdInfoPlugins(const CliCommand& cmd);
    int cmdInfoExporters(const CliCommand& cmd);
    int cmdInfoPointers(const CliCommand& cmd);
    int cmdInfoHistory(const CliCommand& cmd);
    int cmdInfoDebug(const CliCommand& cmd);
    int cmdBuild(const CliCommand& cmd);
    int cmdExport(const CliCommand& cmd);
    int cmdListBranches(const CliCommand& cmd);
    int cmdListModpacks(const CliCommand& cmd);
    int cmdStatus(const CliCommand& cmd);
    int cmdSyncServerConfig(const CliCommand& cmd);
    int cmdVerifyRepo(const CliCommand& cmd);
    int cmdResolvePointer(const CliCommand& cmd);
    int cmdCrashTest(const CliCommand& cmd);
    int cmdGitUpdate(const CliCommand& cmd);
    int cmdRepoTrust(const CliCommand& cmd);
    int cmdRepoTrustCheck(const CliCommand& cmd);

    int notImplemented(const CliCommand& cmd);

    std::string gitVersion() const;
    std::string resolveWorkDir(const std::string& repoUrl) const;
    std::string resolveRepoPath(const std::string& repoUrl) const;
    std::string ensureRepoCloned(const std::string& repoUrl,
        const std::string& gitBranch);
    std::string findWorkspaceJson(const std::string& workDir) const;
    std::string findBuildOutputDir(const std::string& workDir,
        const std::string& modpackBranch) const;
};

} // namespace NeoCLI
