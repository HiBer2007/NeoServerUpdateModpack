#include "workspace_manager.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <set>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <logger.h>

namespace NeoWorkspace {

namespace fs = std::filesystem;

WorkspaceManager::WorkspaceManager()
    : loaded_(false)
    , validated_(false)
{
}

WorkspaceManager::~WorkspaceManager()
{
}

bool WorkspaceManager::loadFromFile(const std::string& configPath)
{
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            CLogger::Error("WorkspaceManager: Cannot open config file: {}", configPath);
            return false;
        }

        config_ = nlohmann::json::parse(file);
        file.close();

        loaded_ = true;
        validated_ = false;

        parseBranches();
        parseFileManifest();
        parsePointerFiles();
        parseSyncPolicies();

        CLogger::Info("WorkspaceManager: Loaded config from {}", configPath);
        return true;
    }
    catch (const nlohmann::json::parse_error& e) {
        CLogger::Error("WorkspaceManager: JSON parse error in {}: {}", configPath, e.what());
        loaded_ = false;
        return false;
    }
    catch (const std::exception& e) {
        CLogger::Error("WorkspaceManager: Error loading config: {}", e.what());
        loaded_ = false;
        return false;
    }
}

bool WorkspaceManager::loadFromJson(const nlohmann::json& config)
{
    try {
        if (config.is_null()) {
            CLogger::Error("WorkspaceManager: loadFromJson received null config");
            loaded_ = false;
            return false;
        }

        config_ = config;
        loaded_ = true;
        validated_ = false;

        parseBranches();
        parseFileManifest();
        parsePointerFiles();
        parseSyncPolicies();

        CLogger::Info("WorkspaceManager: Loaded config from JSON object");
        return true;
    }
    catch (const std::exception& e) {
        CLogger::Error("WorkspaceManager: Error loading config from JSON: {}", e.what());
        loaded_ = false;
        return false;
    }
}

bool WorkspaceManager::validate()
{
    if (!loaded_) {
        CLogger::Error("WorkspaceManager: Cannot validate - config not loaded");
        validated_ = false;
        return false;
    }

    try {
        if (!config_.contains("workspace") || !config_["workspace"].is_object()) {
            CLogger::Error("WorkspaceManager: Missing 'workspace' section");
            validated_ = false;
            return false;
        }

        const auto& ws = config_["workspace"];
        if (!ws.contains("name") || ws["name"].get<std::string>().empty()) {
            CLogger::Error("WorkspaceManager: Missing or empty 'workspace.name'");
            validated_ = false;
            return false;
        }

        if (!config_.contains("git") || !config_["git"].is_object()) {
            CLogger::Error("WorkspaceManager: Missing 'git' section");
            validated_ = false;
            return false;
        }

        const auto& git = config_["git"];
        if (!git.contains("remote") || git["remote"].get<std::string>().empty()) {
            CLogger::Error("WorkspaceManager: Missing or empty 'git.remote'");
            validated_ = false;
            return false;
        }

        if (!config_.contains("branches") || !config_["branches"].is_array() ||
            config_["branches"].empty()) {
            CLogger::Error("WorkspaceManager: Missing or empty 'branches' array");
            validated_ = false;
            return false;
        }

        validated_ = true;
        CLogger::Info("WorkspaceManager: Validation passed for '{}'", workspaceName());
        return true;
    }
    catch (const std::exception& e) {
        CLogger::Error("WorkspaceManager: Validation error: {}", e.what());
        validated_ = false;
        return false;
    }
}

std::string WorkspaceManager::workspaceName() const
{
    try {
        return config_.value("workspace", nlohmann::json::object())
            .value("name", "");
    }
    catch (...) {
        return "";
    }
}

std::string WorkspaceManager::minecraftVersion() const
{
    try {
        return config_.value("workspace", nlohmann::json::object())
            .value("minecraft_version", "");
    }
    catch (...) {
        return "";
    }
}

std::string WorkspaceManager::modloader() const
{
    try {
        return config_.value("workspace", nlohmann::json::object())
            .value("modloader", "");
    }
    catch (...) {
        return "";
    }
}

std::string WorkspaceManager::gitRemote() const
{
    try {
        return config_.value("git", nlohmann::json::object())
            .value("remote", "");
    }
    catch (...) {
        return "";
    }
}

std::string WorkspaceManager::defaultBranch() const
{
    try {
        return config_.value("git", nlohmann::json::object())
            .value("default_branch", "main");
    }
    catch (...) {
        return "main";
    }
}

const nlohmann::json& WorkspaceManager::config() const
{
    return config_;
}

void WorkspaceManager::parseBranches()
{
    branches_.clear();

    if (!config_.contains("branches") || !config_["branches"].is_array()) {
        CLogger::Warn("WorkspaceManager: No branches array in config");
        return;
    }

    try {
        for (const auto& b : config_["branches"]) {
            BranchConfig bc;
            bc.name = b.value("name", "");
            bc.parent = b.value("parent", "");
            bc.gameVersion = b.value("game_version", "");
            bc.modloader = b.value("modloader", "");
            bc.modloaderVersion = b.value("modloader_version", "");
            bc.description = b.value("description", "");
            bc.hidden = b.value("hidden", false);

            if (bc.name.empty()) {
                CLogger::Warn("WorkspaceManager: Skipping branch with empty name");
                continue;
            }

            branches_.push_back(std::move(bc));
        }

        CLogger::Info("WorkspaceManager: Parsed {} branches", branches_.size());
    }
    catch (const std::exception& e) {
        CLogger::Error("WorkspaceManager: Error parsing branches: {}", e.what());
    }
}

void WorkspaceManager::parseFileManifest()
{
    fileManifest_.clear();

    if (!config_.contains("file_manifest") || !config_["file_manifest"].is_object()) {
        CLogger::Warn("WorkspaceManager: No file_manifest in config");
        return;
    }

    try {
        for (const auto& [key, value] : config_["file_manifest"].items()) {
            if (value.is_string()) {
                fileManifest_[key] = value.get<std::string>();
            }
            else {
                CLogger::Warn("WorkspaceManager: Non-string value in file_manifest for key '{}'", key);
            }
        }

        CLogger::Info("WorkspaceManager: Parsed {} file manifest entries", fileManifest_.size());
    }
    catch (const std::exception& e) {
        CLogger::Error("WorkspaceManager: Error parsing file_manifest: {}", e.what());
    }
}

void WorkspaceManager::parsePointerFiles()
{
    pointerFiles_.clear();

    if (!config_.contains("pointer_files") || !config_["pointer_files"].is_object()) {
        CLogger::Info("WorkspaceManager: No pointer_files in config");
        return;
    }

    try {
        for (const auto& [sha256, ptrObj] : config_["pointer_files"].items()) {
            NeoCore::PointerInfo info;
            info.sha256 = sha256;
            info.resolver = ptrObj.value("resolver", "");

            if (ptrObj.contains("metadata")) {
                info.metadata = ptrObj["metadata"];
            }

            pointerFiles_[sha256] = std::move(info);
        }

        CLogger::Info("WorkspaceManager: Parsed {} pointer files", pointerFiles_.size());
    }
    catch (const std::exception& e) {
        CLogger::Error("WorkspaceManager: Error parsing pointer_files: {}", e.what());
    }
}

namespace {

SyncPolicy parsePolicyObject(const nlohmann::json& obj)
{
    SyncPolicy policy;

    if (obj.contains("default_folder_policy") && obj["default_folder_policy"].is_string()) {
        policy.defaultFolderPolicy = obj["default_folder_policy"].get<std::string>();
    }

    if (obj.contains("folders") && obj["folders"].is_object()) {
        for (const auto& [path, val] : obj["folders"].items()) {
            if (!val.is_string()) continue;
            SyncPolicyFolder f;
            f.path = path;
            f.policy = val.get<std::string>();
            policy.folders.push_back(std::move(f));
        }
    }

    if (obj.contains("files") && obj["files"].is_object()) {
        for (const auto& [path, val] : obj["files"].items()) {
            if (!val.is_object()) continue;
            SyncPolicyFile f;
            f.path = path;
            f.mode = val.value("mode", "full");

            if (val.contains("tracked_keys") && val["tracked_keys"].is_array()) {
                for (const auto& k : val["tracked_keys"]) {
                    if (k.is_string()) {
                        f.trackedKeys.push_back(k.get<std::string>());
                    }
                }
            }

            if (val.contains("tracked_lines") && val["tracked_lines"].is_array()) {
                for (const auto& l : val["tracked_lines"]) {
                    if (l.is_number_integer()) {
                        f.trackedLines.push_back(l.get<int>());
                    }
                }
            }

            policy.files.push_back(std::move(f));
        }
    }

    return policy;
}

} // namespace

void WorkspaceManager::parseSyncPolicies()
{
    policy_ = SyncPolicy{};
    branchPolicies_.clear();

    if (config_.contains("sync_policies") && config_["sync_policies"].is_object()) {
        policy_ = parsePolicyObject(config_["sync_policies"]);
        CLogger::Info("WorkspaceManager: Parsed {} folder policies, {} file policies",
            policy_.folders.size(), policy_.files.size());
    }

    if (!config_.contains("branches") || !config_["branches"].is_array()) {
        return;
    }

    for (const auto& branchObj : config_["branches"]) {
        if (!branchObj.is_object()) continue;
        std::string name = branchObj.value("name", "");
        if (name.empty()) continue;

        if (branchObj.contains("sync_policies") && branchObj["sync_policies"].is_object()) {
            branchPolicies_[name] = parsePolicyObject(branchObj["sync_policies"]);
        }
    }

    CLogger::Info("WorkspaceManager: Parsed sync policies for {} branches",
        branchPolicies_.size());
}

SyncPolicy WorkspaceManager::syncPolicy(const std::string& branch) const
{
    if (branch.empty() || !branchPolicies_.count(branch)) {
        return policy_;
    }

    SyncPolicy effective = policy_;
    const SyncPolicy& override = branchPolicies_.at(branch);

    if (!override.defaultFolderPolicy.empty()) {
        effective.defaultFolderPolicy = override.defaultFolderPolicy;
    }

    for (const auto& f : override.folders) {
        effective.folders.erase(
            std::remove_if(effective.folders.begin(), effective.folders.end(),
                [&f](const SyncPolicyFolder& x) { return x.path == f.path; }),
            effective.folders.end());
        effective.folders.push_back(f);
    }

    for (const auto& f : override.files) {
        effective.files.erase(
            std::remove_if(effective.files.begin(), effective.files.end(),
                [&f](const SyncPolicyFile& x) { return x.path == f.path; }),
            effective.files.end());
        effective.files.push_back(f);
    }

    return effective;
}

std::vector<WorkspaceManager::BranchConfig> WorkspaceManager::branches() const
{
    return branches_;
}

WorkspaceManager::BranchConfig WorkspaceManager::findBranch(const std::string& name) const
{
    for (const auto& b : branches_) {
        if (b.name == name) {
            return b;
        }
    }

    CLogger::Warn("WorkspaceManager: Branch '{}' not found", name);
    return BranchConfig{};
}

std::vector<std::string> WorkspaceManager::branchInheritanceChain(const std::string& branchName) const
{
    std::vector<std::string> chain;
    std::string current = branchName;

    constexpr size_t maxDepth = 100;

    while (!current.empty() && chain.size() < maxDepth) {
        if (std::find(chain.begin(), chain.end(), current) != chain.end()) {
            CLogger::Error(
                "WorkspaceManager: Circular parent reference detected at branch '{}'",
                current);
            chain.clear();
            return chain;
        }

        chain.push_back(current);

        const BranchConfig* cfg = nullptr;
        for (const auto& b : branches_) {
            if (b.name == current) {
                cfg = &b;
                break;
            }
        }

        if (!cfg) {
            CLogger::Error(
                "WorkspaceManager: Branch '{}' not found in inheritance chain lookup", current);
            chain.clear();
            return chain;
        }

        current = cfg->parent;
        if (current.empty()) {
            break;
        }

        bool parentExists = false;
        for (const auto& b : branches_) {
            if (b.name == current) {
                parentExists = true;
                break;
            }
        }
        if (!parentExists) {
            CLogger::Error(
                "WorkspaceManager: Parent branch '{}' for branch '{}' not found",
                current, cfg->name);
            chain.clear();
            return chain;
        }
    }

    if (chain.size() >= maxDepth) {
        CLogger::Error(
            "WorkspaceManager: Branch inheritance chain exceeded max depth for '{}'",
            branchName);
        chain.clear();
        return chain;
    }

    std::reverse(chain.begin(), chain.end());
    return chain;
}

std::string WorkspaceManager::resolveDirectory(const std::string& key, const std::string& branch) const
{
    try {
        std::string dir;
        if (config_.contains("directories") && config_["directories"].is_object()) {
            dir = config_["directories"].value(key, "");
        }

        if (dir.empty()) {
            CLogger::Warn("WorkspaceManager: Directory key '{}' not found or empty", key);
            return "";
        }

        if (!branch.empty()) {
            size_t pos = dir.find("{branch}");
            while (pos != std::string::npos) {
                dir.replace(pos, 8, branch);
                pos = dir.find("{branch}", pos + branch.size());
            }
        }

        std::replace(dir.begin(), dir.end(), '\\', '/');
        return dir;
    }
    catch (const std::exception& e) {
        CLogger::Error("WorkspaceManager: Error resolving directory '{}': {}", key, e.what());
        return "";
    }
}

std::unordered_map<std::string, std::string> WorkspaceManager::fileManifest() const
{
    return fileManifest_;
}

bool WorkspaceManager::serverConfigSyncEnabled() const
{
    try {
        return config_.value("serverconfig_sync", nlohmann::json::object())
            .value("enabled", false);
    }
    catch (...) {
        return false;
    }
}

std::vector<std::string> WorkspaceManager::serverConfigScanPaths() const
{
    std::vector<std::string> paths;
    try {
        if (!config_.contains("serverconfig_sync") || !config_["serverconfig_sync"].is_object()) {
            return paths;
        }
        const auto& scs = config_["serverconfig_sync"];
        if (scs.contains("scan_paths") && scs["scan_paths"].is_array()) {
            for (const auto& p : scs["scan_paths"]) {
                if (p.is_string()) {
                    std::string path = p.get<std::string>();
                    std::replace(path.begin(), path.end(), '\\', '/');
                    paths.push_back(path);
                }
            }
        }
    }
    catch (const std::exception& e) {
        CLogger::Error("WorkspaceManager: Error parsing serverconfig scan paths: {}", e.what());
    }
    return paths;
}

std::unordered_map<std::string, NeoCore::PointerInfo> WorkspaceManager::pointerFiles() const
{
    return pointerFiles_;
}

bool WorkspaceManager::customModsEnabled() const
{
    try {
        return config_.value("custom_mods", nlohmann::json::object())
            .value("enabled", false);
    }
    catch (...) {
        return false;
    }
}

std::string WorkspaceManager::customModsPath(const std::string& branch) const
{
    try {
        const auto& cm = config_.value("custom_mods", nlohmann::json::object());
        std::string path = cm.value("path", "");

        if (path.empty()) {
            return "";
        }

        if (!branch.empty()) {
            size_t pos = path.find("{branch}");
            while (pos != std::string::npos) {
                path.replace(pos, 8, branch);
                pos = path.find("{branch}", pos + branch.size());
            }
        }

        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }
    catch (const std::exception& e) {
        CLogger::Error("WorkspaceManager: Error resolving custom mods path: {}", e.what());
        return "";
    }
}

BranchManifest WorkspaceManager::loadBranchManifest(const std::string& branchName) const
{
    std::string dir = branchStorageDir(branchName);
    if (dir.empty()) return {};

    std::string path = dir + "/branch_manifest.json";
    std::ifstream f(path);
    if (!f.is_open()) return {branchName, {}};

    try {
        auto j = nlohmann::json::parse(f);
        return BranchManifest::fromJson(j);
    } catch (...) {}
    return {branchName, {}};
}

void WorkspaceManager::saveBranchManifest(const std::string& branchName,
    const BranchManifest& manifest) const
{
    std::string dir = branchStorageDir(branchName);
    if (dir.empty()) return;
    QDir().mkpath(QString::fromStdString(dir));

    std::string path = dir + "/branch_manifest.json";
    std::ofstream f(path);
    f << manifest.toJson().dump(2) << std::endl;
}

void WorkspaceManager::setFileMarker(const std::string& branchName,
    const std::string& relPath, FileMarker marker)
{
    auto manifest = loadBranchManifest(branchName);
    manifest.branchName = branchName;

    if (marker == FileMarker::None) {
        manifest.markers.erase(relPath);
    } else {
        manifest.markers[relPath] = marker;
    }

    if (marker == FileMarker::Override) {
        std::string ovDir = branchOverridesDir(branchName);
        QDir().mkpath(QString::fromStdString(ovDir));
    }

    saveBranchManifest(branchName, manifest);
}

std::string WorkspaceManager::branchStorageDir(const std::string& branchName) const
{
    std::string dir = resolveDirectory("mods", branchName);
    if (dir.empty()) return "";
    size_t pos = dir.rfind('/');
    if (pos == std::string::npos) return dir;
    return dir.substr(0, pos);
}

std::string WorkspaceManager::branchOverridesDir(const std::string& branchName) const
{
    return branchStorageDir(branchName) + "/.overrides";
}

bool WorkspaceManager::fileExistsInParent(const std::string& branchName,
    const std::string& relPath) const
{
    auto chain = branchInheritanceChain(branchName);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (*it == branchName) continue;
        auto manifest = loadBranchManifest(*it);
        auto mi = manifest.markers.find(relPath);
        if (mi != manifest.markers.end() && mi->second == FileMarker::Delete)
            return false;

        std::string filePath = branchStorageDir(*it) + "/" + relPath;
        if (QFileInfo::exists(QString::fromStdString(filePath)))
            return true;

        std::string ovPath = branchOverridesDir(*it) + "/" + relPath;
        if (QFileInfo::exists(QString::fromStdString(ovPath)))
            return true;
    }
    return false;
}

std::vector<std::string> WorkspaceManager::branchFiles(const std::string& branchName) const
{
    std::vector<std::string> result;
    std::string dir = branchStorageDir(branchName);
    if (dir.empty()) return result;

    QDirIterator it(QString::fromStdString(dir), QDir::Files,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        std::string relPath = QDir(QString::fromStdString(dir))
            .relativeFilePath(it.filePath()).toStdString();
        std::replace(relPath.begin(), relPath.end(), '\\', '/');
        if (relPath.find(".overrides/") != std::string::npos) continue;
        if (relPath == "branch_manifest.json") continue;
        result.push_back(relPath);
    }
    return result;
}

std::vector<std::string> WorkspaceManager::listInheritedFiles(
    const std::string& branchName) const
{
    std::vector<std::string> result;
    auto chain = branchInheritanceChain(branchName);
    if (chain.size() < 2) return result;

    auto selfManifest = loadBranchManifest(branchName);
    std::set<std::string> selfFiles;
    for (const auto& f : branchFiles(branchName)) selfFiles.insert(f);

    std::set<std::string> collected;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (*it == branchName) continue;
        const std::string& parent = *it;
        auto manifest = loadBranchManifest(parent);

        std::vector<std::string> files = branchFiles(parent);
        const std::string ovDir = branchOverridesDir(parent);
        if (!ovDir.empty()) {
            QDirIterator it2(QString::fromStdString(ovDir), QDir::Files,
                QDirIterator::Subdirectories);
            while (it2.hasNext()) {
                it2.next();
                std::string relPath = QDir(QString::fromStdString(ovDir))
                    .relativeFilePath(it2.filePath()).toStdString();
                std::replace(relPath.begin(), relPath.end(), '\\', '/');
                files.push_back(relPath);
            }
        }

        for (const auto& rel : files) {
            if (collected.count(rel)) continue;
            collected.insert(rel);

            if (selfFiles.count(rel)) continue;
            auto mi = selfManifest.markers.find(rel);
            if (mi != selfManifest.markers.end()) continue;

            auto pi = manifest.markers.find(rel);
            if (pi != manifest.markers.end()
                && pi->second == FileMarker::Delete) continue;

            result.push_back(rel);
        }
    }
    return result;
}

} // namespace NeoWorkspace

