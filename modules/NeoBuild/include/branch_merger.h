#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cancel_token.h>
#include <workspace_manager.h>

namespace NeoBuild {

struct BranchLayer {
    std::string name;
    std::string baseDir;
    std::string overridesDir;               // .overrides/ subdirectory
    NeoWorkspace::BranchManifest manifest;  // branch_manifest.json
};

struct MergeResult {
    bool success = false;
    std::vector<std::string> mergedFiles;
    std::vector<std::string> overriddenFiles;
    std::vector<std::string> deletedFiles;
    std::string message;
};

class BranchMerger {
public:
    BranchMerger();

    MergeResult merge(const std::vector<BranchLayer>& layers,
        const std::string& outputDir,
        bool overwriteChild = true,
        NeoCore::CancelToken* cancelToken = nullptr);

    MergeResult mergeDirectories(const std::string& parentDir,
        const std::string& childDir,
        const std::string& outputDir,
        const std::string& manifestPath = "",
        NeoCore::CancelToken* cancelToken = nullptr);

    static NeoWorkspace::BranchManifest loadManifest(const std::string& dir);
    static void saveManifest(const std::string& dir,
        const NeoWorkspace::BranchManifest& manifest);

private:
    bool copyFileSilent(const std::string& src, const std::string& dst, bool overwrite);
    void copyDirectoryWithManifest(const std::string& srcDir,
        const std::string& dstDir, const std::string& overridesDir,
        const NeoWorkspace::BranchManifest& manifest,
        bool isRoot, std::vector<std::string>& copiedFiles,
        std::vector<std::string>& overridden,
        std::vector<std::string>& deletedFiles,
        NeoCore::CancelToken* cancelToken);
};

} // namespace NeoBuild
