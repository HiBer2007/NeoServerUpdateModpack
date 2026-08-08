#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <cancel_token.h>
#include <IModpackExporter.h>
#include <IBuildProgress.h>
#include <PluginLoader.h>
#include <IPluginPointer.h>

namespace NeoWorkspace {
class WorkspaceManager;
class GitOperations;
class SyncEngine;
class FileScanner;
}

namespace NeoBuild {

class BranchMerger;
class PointerDownloader;
class ModpackExporter;

using BuildProgress = NeoCore::BuildProgress;
using BuildResult = NeoCore::BuildResult;

class BuildEngine {
public:
    BuildEngine();
    ~BuildEngine();

    bool init(const std::string& workspacePath,
        const std::string& cacheDir = "",
        const std::string& outputBaseDir = "",
        const std::string& exportersDir = "");

    BuildResult build(const std::string& branchName,
        NeoCore::IBuildProgress* progress = nullptr,
        NeoCore::CancelToken* cancelToken = nullptr,
        const std::string& gitBranch = "");

    bool stepCloneOrFetch();
    bool stepCheckout(const std::string& branch);
    bool stepMergeBranches(const std::string& targetBranch);
    bool stepProcessFiles(const std::string& branchName);
    bool stepMergeCustomMods(const std::string& branchName);
    bool stepSyncServerConfigs();
    bool stepFinalize();
    // hmcl 工作区同步：构建完成后按 sync_policies 同步到目标工作目录
    bool stepSyncTarget(NeoCore::IBuildProgress* progress,
        NeoCore::CancelToken* cancelToken, NeoCore::BuildResult& result);

    bool exportModpack(const std::string& format,
        const std::string& outputPath,
        const NeoCore::ExportMetadata& metadata);

    nlohmann::json previewStructure(const std::string& format,
        const NeoCore::ExportMetadata& metadata,
        const std::string& targetDir = "");

    std::string cacheDir() const { return cacheDir_; }
    std::string outputDir() const { return outputDir_; }
    const NeoWorkspace::WorkspaceManager* workspace() const { return workspace_.get(); }

    void setGitPath(const std::string& path);

    // hmcl 工作区同步目标目录（空 = 跳过同步，打包格式）
    void setTargetDir(const std::string& dir) { targetDir_ = dir; }
    const std::string& targetDir() const { return targetDir_; }

    // 分支级文件清单（file_manifest/pointer_files 顶层 + 分支合并结果，分支优先）
    const std::unordered_map<std::string, std::string>& mergedFileManifest() const
    { return mergedFileManifest_; }
    const std::unordered_map<std::string, NeoCore::PointerInfo>& mergedPointerFiles() const
    { return mergedPointerFiles_; }

private:
    std::unique_ptr<NeoWorkspace::WorkspaceManager> workspace_;
    std::unique_ptr<NeoWorkspace::GitOperations> git_;
    std::unique_ptr<NeoWorkspace::SyncEngine> sync_;
    std::unique_ptr<NeoWorkspace::FileScanner> scanner_;
    std::unique_ptr<BranchMerger> merger_;
    std::unique_ptr<PointerDownloader> downloader_;
    std::unique_ptr<ModpackExporter> exporter_;

    NeoCore::PluginLoader pluginLoader_;

    std::string workspacePath_;
    std::string cacheDir_;
    std::string outputDir_;
    std::string targetDir_;
    std::string currentBranch_;

    BuildProgress progress_;

    std::unordered_map<std::string, std::string> mergedFileManifest_;
    std::unordered_map<std::string, NeoCore::PointerInfo> mergedPointerFiles_;

    bool loadBranchConfig(const std::string& branchName);

    void reportProgress(const std::string& stage, int percent,
        const std::string& message, NeoCore::IBuildProgress* progress);
    bool checkCancelled(NeoCore::CancelToken* token) const;
    std::string generateVersionJson() const;
    std::string generateHMCLVersionCfg() const;
};

} // namespace NeoBuild

