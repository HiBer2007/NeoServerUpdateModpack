#include "build_engine.h"
#include "branch_merger.h"
#include "pointer_downloader.h"
#include "modpack_exporter.h"
#include "serverconfig_sync.h"
#include "sync_policy_executor.h"
#include "workspace_manager.h"
#include <git_operations.h>
#include <sync_engine.h>
#include <file_scanner.h>
#include <logger.h>
#include <error_codes.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

namespace NeoBuild {

namespace fs = std::filesystem;

BuildEngine::BuildEngine()
    : workspace_(nullptr)
    , git_(std::make_unique<NeoWorkspace::GitOperations>())
    , sync_(std::make_unique<NeoWorkspace::SyncEngine>())
    , scanner_(std::make_unique<NeoWorkspace::FileScanner>())
    , merger_(std::make_unique<BranchMerger>())
    , downloader_(std::make_unique<PointerDownloader>())
    , exporter_(std::make_unique<ModpackExporter>())
{
}

BuildEngine::~BuildEngine() = default;

void BuildEngine::setGitPath(const std::string& path)
{
    git_ = std::make_unique<NeoWorkspace::GitOperations>(path);
}

bool BuildEngine::init(const std::string& workspacePath,
    const std::string& cacheDir,
    const std::string& outputBaseDir,
    const std::string& exportersDir)
{
    try {
        // 兼容传入 workspace.json 文件路径（取其所在目录）
        fs::path wsPath(workspacePath);
        if (fs::is_regular_file(wsPath)) {
            workspacePath_ = wsPath.parent_path().string();
        } else {
            workspacePath_ = workspacePath;
        }

        if (cacheDir.empty()) {
            cacheDir_ = (fs::path(workspacePath) / ".cache").string();
        } else {
            cacheDir_ = cacheDir;
        }

        if (outputBaseDir.empty()) {
            outputDir_ = (fs::path(workspacePath) / "output").string();
        } else {
            outputDir_ = outputBaseDir;
        }

        std::error_code ec;
        fs::create_directories(cacheDir_, ec);
        if (ec) {
            CLogger::Error("BuildEngine::init failed to create cache dir: {}", ec.message());
            return false;
        }

        fs::create_directories(outputDir_, ec);
        if (ec) {
            CLogger::Error("BuildEngine::init failed to create output dir: {}", ec.message());
            return false;
        }

        workspace_ = std::make_unique<NeoWorkspace::WorkspaceManager>();
        if (!workspace_->loadFromFile(workspacePath_ + "/workspace.json")) {
            CLogger::Error("BuildEngine::init failed to load workspace config: {}",
                workspacePath_ + "/workspace.json");
            return false;
        }

        CLogger::Info("BuildEngine initialized: workspace={} cache={} output={}",
            workspacePath_, cacheDir_, outputDir_);

        // 预览/导出所需: 加载 exporters 插件 (可选目录, 不传则保持空壳)
        if (!exportersDir.empty()) {
            exporter_->scanExporters(exportersDir);
        }

        return true;
    } catch (const std::exception& e) {
        CLogger::Error("BuildEngine::init exception: {}", e.what());
        return false;
    }
}

bool BuildEngine::loadBranchConfig(const std::string& branchName)
{
    mergedFileManifest_.clear();
    mergedPointerFiles_.clear();

    try {
        auto topManifest = workspace_->fileManifest();
        for (const auto& [rel, sha] : topManifest) {
            mergedFileManifest_[rel] = sha;
        }
        auto topPointers = workspace_->pointerFiles();
        for (const auto& [sha, info] : topPointers) {
            mergedPointerFiles_[sha] = info;
        }

        const std::string bcFile = workspacePath_ + "/branch_config/" + branchName + ".json";
        if (!fs::exists(bcFile)) {
            CLogger::Info("BuildEngine: no branch_config for '{}', using top-level "
                "file_manifest/pointer_files ({} entries)", branchName,
                mergedFileManifest_.size());
            return true;
        }

        std::ifstream f(bcFile);
        if (!f.is_open()) {
            CLogger::Warn("BuildEngine: cannot open branch_config file: {}", bcFile);
            return false;
        }
        nlohmann::json j = nlohmann::json::parse(f);
        f.close();

        if (j.contains("file_manifest") && j["file_manifest"].is_object()) {
            for (const auto& [rel, val] : j["file_manifest"].items()) {
                if (val.is_string()) {
                    mergedFileManifest_[rel] = val.get<std::string>();
                } else if (val.is_object() && val.contains("sha256") && val["sha256"].is_string()) {
                    mergedFileManifest_[rel] = val["sha256"].get<std::string>();
                }
            }
        }

        if (j.contains("pointer_files") && j["pointer_files"].is_object()) {
            for (const auto& [sha, ptrObj] : j["pointer_files"].items()) {
                NeoCore::PointerInfo info;
                info.sha256 = sha;
                info.resolver = ptrObj.value("resolver", "");
                info.metadata = ptrObj.contains("metadata")
                    ? ptrObj["metadata"] : nlohmann::json::object();
                if (info.resolver.empty() || sha.empty()) continue;
                mergedPointerFiles_[sha] = std::move(info);
            }
        }

        CLogger::Info("BuildEngine: branch_config loaded for '{}': {} manifest entries, "
            "{} pointer files", branchName, mergedFileManifest_.size(),
            mergedPointerFiles_.size());
        return true;
    } catch (const std::exception& e) {
        CLogger::Error("BuildEngine::loadBranchConfig exception: {}", e.what());
        return false;
    }
}

void BuildEngine::reportProgress(const std::string& stage, int percent,
    const std::string& message, NeoCore::IBuildProgress* progress)
{
    progress_.stage = stage;
    progress_.percent = percent;
    progress_.message = message;

    CLogger::Info("[{} {}%] {}", stage, percent, message);

    if (!progress) {
        return;
    }

    progress->set_main_stage(stage);
    progress->set_main_progress(percent);
    progress->set_main_message(message);
    progress->log("[" + stage + " " + std::to_string(percent) + "%] " + message);
}

bool BuildEngine::checkCancelled(NeoCore::CancelToken* token) const
{
    return token && token->is_cancelled();
}

BuildResult BuildEngine::build(const std::string& branchName,
    NeoCore::IBuildProgress* progress,
    NeoCore::CancelToken* cancelToken,
    const std::string& gitBranch)
{
    BuildResult result;
    result.outputDir = outputDir_;
    result.success = false;

    currentBranch_ = branchName;
    loadBranchConfig(branchName);

    auto cancelled = [&]() -> bool {
        return checkCancelled(cancelToken)
            || (progress && progress->is_cancelled());
    };

    try {
        reportProgress("init", 0, "Start building branch: " + branchName, progress);
        if (cancelled()) {
            result.errorMessage = "构建已取消";
            return result;
        }

        reportProgress("clone", 5, "Cloning/updating repository...", progress);
        if (!stepCloneOrFetch()) {
            result.errorMessage = "仓库操作失败";
            result.warnings.push_back("Git clone/fetch failed");
            return result;
        }
        if (cancelled()) {
            result.errorMessage = "构建已取消";
            return result;
        }

        const std::string checkBranch = gitBranch.empty() ? branchName : gitBranch;
        reportProgress("checkout", 15, "Switching to branch: " + checkBranch, progress);
        if (!stepCheckout(checkBranch)) {
            result.errorMessage = "分支切换失败: " + checkBranch;
            result.warnings.push_back("Could not checkout branch: " + checkBranch);
            return result;
        }
        if (cancelled()) {
            result.errorMessage = "构建已取消";
            return result;
        }

        reportProgress("merge", 25, "Merging parent branches...", progress);
        if (!stepMergeBranches(branchName)) {
            result.warnings.push_back("Branch merge had issues");
        }
        if (cancelled()) {
            result.errorMessage = "构建已取消";
            return result;
        }

        reportProgress("download", 40, "Downloading files...", progress);
        if (!stepProcessFiles(branchName)) {
            result.errorMessage = "文件处理失败";
            result.warnings.push_back("File processing failed");
            return result;
        }
        if (cancelled()) {
            result.errorMessage = "构建已取消";
            return result;
        }

        reportProgress("sync_config", 60, "Syncing config files...", progress);
        if (cancelled()) {
            result.errorMessage = "构建已取消";
            return result;
        }

        reportProgress("custom_mods", 75, "Merging custom mods...", progress);
        if (!stepMergeCustomMods(branchName)) {
            result.warnings.push_back("Custom mod merge had issues");
        }
        if (cancelled()) {
            result.errorMessage = "构建已取消";
            return result;
        }

        reportProgress("server_config", 85, "Syncing server configs...", progress);
        if (!stepSyncServerConfigs()) {
            result.warnings.push_back("Server config sync had issues");
        }
        if (cancelled()) {
            result.errorMessage = "构建已取消";
            return result;
        }

        reportProgress("finalize", 95, "Generating version files...", progress);
        if (!stepFinalize()) {
            result.warnings.push_back("Finalization had issues");
        }
        if (cancelled()) {
            result.errorMessage = "构建已取消";
            return result;
        }

        if (!targetDir_.empty()) {
        reportProgress("sync_target", 97, "Syncing target working dir...", progress);
            if (!stepSyncTarget(progress, cancelToken, result)) {
                result.warnings.push_back("Sync target had issues");
            }
        }
        if (cancelled()) {
            result.errorMessage = "构建已取消";
            return result;
        }

        result.success = true;
        result.outputDir = targetDir_.empty() ? outputDir_ : targetDir_;
        result.totalFiles = result.syncedFiles + result.failedFiles;

        reportProgress("done", 100, "Build complete", progress);
        CLogger::Info("Build complete for branch: {}", branchName);
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = std::string("构建异常: ") + e.what();
        CLogger::Error("BuildEngine::build exception: {}", e.what());
    }

    return result;
}

bool BuildEngine::stepCloneOrFetch()
{
    try {
        if (!fs::exists(workspacePath_)) {
            fs::create_directories(workspacePath_);
            CLogger::Info("Created workspace directory: {}", workspacePath_);
            return true;
        }

        if (git_->isGitRepository(workspacePath_)) {
            // 仅远程仓库才同步; 本地仓库 (无 remote) 跳过 fetch, 直接使用本地工作区
            if (!git_->hasRemote(workspacePath_)) {
                CLogger::Info("No git remote configured, skipping fetch: {}",
                    workspacePath_);
                return true;
            }
            auto fetchResult = git_->fetch(workspacePath_);
            if (fetchResult.exitCode != 0) {
                CLogger::Warn("Git fetch failed (exit {}), using local workspace: {}",
                    fetchResult.exitCode, workspacePath_);
                return true;
            }
            CLogger::Info("Git fetch completed for workspace");
        }

        return true;
    } catch (const std::exception& e) {
        CLogger::Error("stepCloneOrFetch exception: {}", e.what());
        return false;
    }
}

bool BuildEngine::stepCheckout(const std::string& branch)
{
    try {
        if (!git_->isGitRepository(workspacePath_)) {
            CLogger::Warn("Not a git repository, skipping checkout");
            return true;
        }

        auto result = git_->checkout(workspacePath_, branch);
        if (result.exitCode != 0) {
            CLogger::Error("Checkout failed for branch {}: {}", branch, result.stderrOutput);
            return false;
        }

        CLogger::Info("Checked out branch: {}", branch);
        return true;
    } catch (const std::exception& e) {
        CLogger::Error("stepCheckout exception: {}", e.what());
        return false;
    }
}

bool BuildEngine::stepMergeBranches(const std::string& targetBranch)
{
    try {
        std::vector<BranchLayer> layers;

        auto chain = workspace_->branchInheritanceChain(targetBranch);
        if (chain.empty()) {
            chain.push_back(targetBranch);
        }
        for (const auto& name : chain) {
            const std::string baseDir = (fs::path(workspacePath_)
                / "branches" / name).string();
            if (!fs::is_directory(baseDir)) {
                CLogger::Warn("BranchMerger: branch dir missing, skipping layer: {}",
                    baseDir);
                continue;
            }
            BranchLayer layer;
            layer.name = name;
            layer.baseDir = baseDir;
            layer.overridesDir = baseDir + "/.overrides";
            layer.manifest = BranchMerger::loadManifest(baseDir);
            layers.push_back(std::move(layer));
        }

        if (layers.empty()) {
            CLogger::Warn("Branch merge skipped: no layers for branch '{}'",
                targetBranch);
            return true;
        }

        auto mergeResult = merger_->merge(layers, outputDir_, true, nullptr);
        if (!mergeResult.success) {
            CLogger::Warn("Branch merge: {}", mergeResult.message);
        }

        CLogger::Info("Merged {} files, {} overridden, {} deleted",
            mergeResult.mergedFiles.size(), mergeResult.overriddenFiles.size(),
            mergeResult.deletedFiles.size());

        return true;
    } catch (const std::exception& e) {
        CLogger::Error("stepMergeBranches exception: {}", e.what());
        return false;
    }
}

bool BuildEngine::stepProcessFiles(const std::string& branchName)
{
    try {
        auto entries = scanner_->scanDirectory(outputDir_,
            {".git", "branch_config(/.*)?"});

        std::unordered_map<std::string, std::string> pointerRels;
        for (const auto& [rel, sha] : mergedFileManifest_) {
            if (mergedPointerFiles_.find(sha) != mergedPointerFiles_.end()) {
                pointerRels[rel] = sha;
            }
        }

        for (const auto& entry : entries) {
            auto pointerIt = pointerRels.find(entry.relativePath);
            if (pointerIt != pointerRels.end()) {
                continue;
            }

            if (entry.relativePath.find(".pointer") != std::string::npos) {
                auto pfile = fs::path(outputDir_) / entry.relativePath;
                QFile qf(QString::fromStdString(pfile.string()));
                if (!qf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    CLogger::Warn("Cannot read pointer file: {}", entry.relativePath);
                    continue;
                }

                QByteArray raw = qf.readAll();
                qf.close();

                try {
                    auto j = nlohmann::json::parse(raw.toStdString());
                    NeoCore::PointerInfo ptr;
                    ptr.sha256 = j.value("sha256", "");
                    ptr.resolver = j.value("resolver", "");
                    ptr.metadata = j.value("metadata", nlohmann::json::object());

                    if (ptr.sha256.empty() || ptr.resolver.empty()) {
                        CLogger::Warn("Invalid pointer file: {}", entry.relativePath);
                        continue;
                    }

                    std::string cachedFile = downloader_->cachePath(cacheDir_, ptr.sha256);
                    if (fs::exists(cachedFile)) {
                        QFile cfile(QString::fromStdString(cachedFile));
                        if (cfile.open(QIODevice::ReadOnly)) {
                            QCryptographicHash hasher(QCryptographicHash::Sha256);
                            hasher.addData(&cfile);
                            cfile.close();

                            if (hasher.result().toHex().toStdString() == ptr.sha256) {
                                std::string stem = entry.relativePath;
                                auto dotPos = stem.rfind(".pointer");
                                if (dotPos != std::string::npos) {
                                    stem = stem.substr(0, dotPos);
                                }

                                std::string destPath = (fs::path(outputDir_) / stem).string();
                                fs::create_directories(fs::path(destPath).parent_path());
                                std::error_code ec;
                                fs::copy_file(cachedFile, destPath,
                                    fs::copy_options::overwrite_existing, ec);
                                if (!ec) {
                                    CLogger::Info("Cached copy: {} -> {}", ptr.sha256, stem);
                                }
                                continue;
                            } else {
                                CLogger::Warn("Cache hash mismatch for {}, re-downloading", ptr.sha256);
                            }
                        }
                    }

                    auto dlResult = downloader_->download(ptr, cacheDir_, nullptr, nullptr);
                    if (dlResult.success) {
                        std::string stem = entry.relativePath;
                        auto dotPos = stem.rfind(".pointer");
                        if (dotPos != std::string::npos) {
                            stem = stem.substr(0, dotPos);
                        }

                        std::string destPath = (fs::path(outputDir_) / stem).string();
                        fs::create_directories(fs::path(destPath).parent_path());
                        std::error_code ec;
                        fs::copy_file(dlResult.cachedPath, destPath,
                            fs::copy_options::overwrite_existing, ec);
                        if (!ec) {
                            CLogger::Info("Downloaded and placed: {}", stem);
                        }
                    } else {
                        CLogger::Error("Download failed for {}: {}", ptr.sha256, dlResult.errorMessage);
                    }
                } catch (const nlohmann::json::exception& je) {
                    CLogger::Warn("Invalid JSON in pointer file {}: {}", entry.relativePath, je.what());
                }
            }
        }

        if (!pointerRels.empty()) {
            CLogger::Info("Process {} branch_config pointer file(s)", pointerRels.size());
        }
        for (const auto& [rel, sha] : pointerRels) {
            const auto& ptr = mergedPointerFiles_.at(sha);
            auto dlResult = downloader_->download(ptr, cacheDir_, nullptr, nullptr);
            if (!dlResult.success) {
                CLogger::Error("Download failed for {} -> {}: {}",
                    sha, rel, dlResult.errorMessage);
                continue;
            }

            std::string destPath = (fs::path(outputDir_) / rel).string();
            fs::create_directories(fs::path(destPath).parent_path());
            std::error_code ec;
            fs::copy_file(dlResult.cachedPath, destPath,
                fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                CLogger::Info("Pointer placed: {} -> {}", sha, rel);
            } else {
                CLogger::Error("Failed to place {}: {}", rel, ec.message());
            }
        }

        return true;
    } catch (const std::exception& e) {
        CLogger::Error("stepProcessFiles exception: {}", e.what());
        return false;
    }
}

bool BuildEngine::stepMergeCustomMods(const std::string& branchName)
{
    try {
        fs::path customModsDir = fs::path(workspacePath_) / "custom" / "mods";

        if (!fs::exists(customModsDir) || !fs::is_directory(customModsDir)) {
            CLogger::Info("No custom mods directory found, skipping");
            return true;
        }

        fs::path outputModsDir = fs::path(outputDir_) / "mods";
        fs::create_directories(outputModsDir);

        for (const auto& entry : fs::directory_iterator(customModsDir,
            fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            fs::path dest = outputModsDir / entry.path().filename();
            std::error_code ec;
            fs::copy_file(entry.path(), dest,
                fs::copy_options::overwrite_existing, ec);

            if (ec) {
                CLogger::Warn("Failed to copy custom mod {}: {}",
                    entry.path().filename().string(), ec.message());
            } else {
                CLogger::Info("Copied custom mod: {}",
                    entry.path().filename().string());
            }
        }

        CLogger::Info("Custom mods merge complete");
        return true;
    } catch (const std::exception& e) {
        CLogger::Error("stepMergeCustomMods exception: {}", e.what());
        return false;
    }
}

bool BuildEngine::stepSyncServerConfigs()
{
    try {
        std::string savesRoot = (fs::path(targetDir_.empty() ? outputDir_ : targetDir_)
            / "saves").string();

        if (!fs::exists(savesRoot) || !fs::is_directory(savesRoot)) {
            CLogger::Info("No saves directory, skipping server config sync");
            return true;
        }

        ServerConfigSync scs;
        if (!scs.init(savesRoot, workspacePath_, currentBranch_)) {
            CLogger::Warn("ServerConfigSync init failed");
            return false;
        }

        if (!scs.hasRules()) {
            CLogger::Info("No serverconfig rules for branch '{}', skipping",
                currentBranch_);
            return true;
        }

        if (!scs.syncAll(nullptr)) {
            CLogger::Warn("ServerConfigSync completed with failures");
        }

        CLogger::Info("ServerConfigSync done: {} synced, {} skipped, {} failed",
            scs.syncedCount(), scs.skippedCount(), scs.failedCount());

        return scs.failedCount() == 0;
    } catch (const std::exception& e) {
        CLogger::Error("stepSyncServerConfigs exception: {}", e.what());
        return false;
    }
}

bool BuildEngine::stepFinalize()
{
    try {
        std::string versionJson = generateVersionJson();
        fs::path versionPath = fs::path(outputDir_) / "version.json";
        QFile vf(QString::fromStdString(versionPath.string()));
        if (vf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            vf.write(versionJson.c_str(), static_cast<qint64>(versionJson.size()));
            vf.close();
            CLogger::Info("Generated version.json");
        } else {
            CLogger::Warn("Failed to write version.json");
        }

        std::string hmclCfg = generateHMCLVersionCfg();
        fs::path hmclPath = fs::path(outputDir_) / "hmclversion.cfg";
        QFile hf(QString::fromStdString(hmclPath.string()));
        if (hf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            hf.write(hmclCfg.c_str(), static_cast<qint64>(hmclCfg.size()));
            hf.close();
            CLogger::Info("Generated hmclversion.cfg");
        } else {
            CLogger::Warn("Failed to write hmclversion.cfg");
        }

        CLogger::Info("Build finalization complete for branch: {}", currentBranch_);
        return true;
    } catch (const std::exception& e) {
        CLogger::Error("stepFinalize exception: {}", e.what());
        return false;
    }
}

bool BuildEngine::stepSyncTarget(NeoCore::IBuildProgress* progress,
    NeoCore::CancelToken* cancelToken, NeoCore::BuildResult& result)
{
    try {
        if (targetDir_.empty()) return true;

        NeoWorkspace::SyncPolicy policy = workspace_->syncPolicy(currentBranch_);

        SyncPolicyExecutor executor;
        SyncPolicyExecutor::Result r = executor.execute(
            outputDir_, targetDir_, policy, progress, cancelToken);

        for (const auto& w : r.warnings) {
            result.warnings.push_back(w);
        }

        result.syncedFiles += r.copiedFiles + r.mergedFiles;
        result.totalFiles = result.syncedFiles + result.failedFiles;

        CLogger::Info("Target sync complete: copied={} merged={} "
            "skipped={} deleted={} harvested={} restored={}",
            r.copiedFiles, r.mergedFiles, r.skippedFiles, r.deletedFiles,
            r.customHarvested, r.customRestored);

        return r.success;
    } catch (const std::exception& e) {
        CLogger::Error("stepSyncTarget exception: {}", e.what());
        return false;
    }
}

bool BuildEngine::exportModpack(const std::string& format,
    const std::string& outputPath,
    const NeoCore::ExportMetadata& metadata)
{
    try {
        if (!fs::exists(outputDir_)) {
            CLogger::Error("Output directory does not exist: {}", outputDir_);
            return false;
        }

        return exporter_->exportModpack(format, outputDir_, outputPath, metadata, nullptr);
    } catch (const std::exception& e) {
        CLogger::Error("exportModpack exception: {}", e.what());
        return false;
    }
}

nlohmann::json BuildEngine::previewStructure(const std::string& format,
    const NeoCore::ExportMetadata& metadata,
    const std::string& targetDir)
{
    try {
        return exporter_->previewStructure(format, outputDir_, metadata, targetDir);
    } catch (const std::exception& e) {
        CLogger::Error("previewStructure exception: {}", e.what());
        return nlohmann::json::array();
    }
}

std::string BuildEngine::generateVersionJson() const
{
    nlohmann::json j;

    j["name"] = currentBranch_;
    j["mc_version"] = "1.21";
    j["modloader"] = "forge";
    j["modloader_version"] = "";

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    j["build_time"] = oss.str();
    j["generated_by"] = "NeoServerUpdateModpack/1.0";

    return j.dump(2);
}

std::string BuildEngine::generateHMCLVersionCfg() const
{
    std::ostringstream oss;
    oss << "# HMCL Version Configuration\n";
    oss << "# Generated by NeoServerUpdateModpack\n";
    oss << "branch=" << currentBranch_ << "\n";
    oss << "mc_version=1.21\n";

    return oss.str();
}

} // namespace NeoBuild




