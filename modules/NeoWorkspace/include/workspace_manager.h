#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <IPluginPointer.h>
#include "sync_policy.h"

namespace NeoWorkspace {

enum class FileMarker {
    None = 0,
    Delete = 1,
    Override = 2
};

struct BranchManifest {
    std::string branchName;
    std::unordered_map<std::string, FileMarker> markers;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["branch"] = branchName;
        nlohmann::json m = nlohmann::json::object();
        for (auto& [path, marker] : markers) {
            switch (marker) {
                case FileMarker::Delete:   m[path] = "delete"; break;
                case FileMarker::Override: m[path] = "override"; break;
                default: break;
            }
        }
        j["markers"] = m;
        return j;
    }

    static BranchManifest fromJson(const nlohmann::json& j) {
        BranchManifest bm;
        bm.branchName = j.value("branch", "");
        if (j.contains("markers") && j["markers"].is_object()) {
            for (auto& [path, val] : j["markers"].items()) {
                std::string m = val.get<std::string>();
                if (m == "delete") bm.markers[path] = FileMarker::Delete;
                else if (m == "override") bm.markers[path] = FileMarker::Override;
            }
        }
        return bm;
    }
};

class WorkspaceManager {
public:
    WorkspaceManager();
    ~WorkspaceManager();

    bool loadFromFile(const std::string& configPath);
    bool loadFromJson(const nlohmann::json& config);

    bool validate();

    std::string workspaceName() const;
    std::string minecraftVersion() const;
    std::string modloader() const;
    std::string gitRemote() const;
    std::string defaultBranch() const;
    const nlohmann::json& config() const;

    struct BranchConfig {
        std::string name;
        std::string parent;
        std::string gameVersion;
        std::string modloader;
        std::string modloaderVersion;
        std::string description;
        bool hidden = false;
    };
    std::vector<BranchConfig> branches() const;
    BranchConfig findBranch(const std::string& name) const;
    std::vector<std::string> branchInheritanceChain(const std::string& branchName) const;

    std::string resolveDirectory(const std::string& key, const std::string& branch = "") const;

    std::unordered_map<std::string, std::string> fileManifest() const;

    // 有效同步策略：顶层 sync_policies 与分支级覆盖合并
    SyncPolicy syncPolicy(const std::string& branch = "") const;

    bool serverConfigSyncEnabled() const;
    std::vector<std::string> serverConfigScanPaths() const;

    std::unordered_map<std::string, NeoCore::PointerInfo> pointerFiles() const;

    bool customModsEnabled() const;
    std::string customModsPath(const std::string& branch) const;

    BranchManifest loadBranchManifest(const std::string& branchName) const;
    void saveBranchManifest(const std::string& branchName,
        const BranchManifest& manifest) const;
    void setFileMarker(const std::string& branchName,
        const std::string& relPath, FileMarker marker);
    std::string branchStorageDir(const std::string& branchName) const;
    std::string branchOverridesDir(const std::string& branchName) const;
    // 父分支继承文件列表（完整相对路径；排除本分支已覆盖/已标记、父链 delete 标记）
    std::vector<std::string> listInheritedFiles(const std::string& branchName) const;
    bool fileExistsInParent(const std::string& branchName,
        const std::string& relPath) const;
    std::vector<std::string> branchFiles(const std::string& branchName) const;

private:
    nlohmann::json config_;
    bool loaded_;
    bool validated_;
    std::vector<BranchConfig> branches_;
    std::unordered_map<std::string, std::string> fileManifest_;
    std::unordered_map<std::string, NeoCore::PointerInfo> pointerFiles_;

    SyncPolicy policy_;
    std::unordered_map<std::string, SyncPolicy> branchPolicies_;

    void parseBranches();
    void parseFileManifest();
    void parsePointerFiles();
    void parseSyncPolicies();
};
} // namespace NeoWorkspace

