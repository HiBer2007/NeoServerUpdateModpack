#include "branch_merger.h"
#include <logger.h>
#include <workspace_manager.h>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <QFile>
#include <QDir>

namespace NeoBuild {

namespace fs = std::filesystem;

BranchMerger::BranchMerger() = default;

NeoWorkspace::BranchManifest BranchMerger::loadManifest(const std::string& dir)
{
    std::string path = dir + "/branch_manifest.json";
    std::ifstream f(path);
    if (!f.is_open()) return {};
    try {
        auto j = nlohmann::json::parse(f);
        return NeoWorkspace::BranchManifest::fromJson(j);
    } catch (...) {}
    return {};
}

void BranchMerger::saveManifest(const std::string& dir,
    const NeoWorkspace::BranchManifest& manifest)
{
    std::string path = dir + "/branch_manifest.json";
    std::ofstream f(path);
    f << manifest.toJson().dump(2) << std::endl;
}

MergeResult BranchMerger::merge(const std::vector<BranchLayer>& layers,
    const std::string& outputDir, bool overwriteChild,
    NeoCore::CancelToken* cancelToken)
{
    MergeResult result;
    if (layers.empty()) {
        result.success = false;
        result.message = "No branch layers to merge";
        return result;
    }

    try {
        std::error_code ec;
        fs::create_directories(outputDir, ec);

        for (size_t i = 0; i < layers.size(); ++i) {
            if (cancelToken && cancelToken->is_cancelled()) {
                result.success = false;
                result.message = "Merge cancelled";
                return result;
            }
            const auto& layer = layers[i];
            CLogger::Info("BranchMerger: layer {} ({})", i, layer.name);
            bool isRoot = (i == layers.size() - 1);
            copyDirectoryWithManifest(layer.baseDir, outputDir,
                layer.overridesDir, layer.manifest,
                isRoot, result.mergedFiles,
                result.overriddenFiles, result.deletedFiles, cancelToken);
        }

        result.success = true;
        result.message = "Merged " + std::to_string(result.mergedFiles.size()) +
            " added, " + std::to_string(result.overriddenFiles.size()) +
            " overridden, " + std::to_string(result.deletedFiles.size()) + " deleted";
        CLogger::Info("BranchMerger: {}", result.message);
    } catch (const std::exception& e) {
        result.success = false;
        result.message = std::string("Merge exception: ") + e.what();
    }
    return result;
}

MergeResult BranchMerger::mergeDirectories(const std::string& parentDir,
    const std::string& childDir, const std::string& outputDir,
    const std::string& manifestPath, NeoCore::CancelToken* cancelToken)
{
    MergeResult result;
    try {
        std::error_code ec;
        fs::create_directories(outputDir, ec);

        std::string overridesDir = childDir + "/.overrides";
        auto manifest = loadManifest(childDir);

        if (fs::exists(parentDir) && fs::is_directory(parentDir)) {
            copyDirectoryWithManifest(parentDir, outputDir, "",
                {}, false, result.mergedFiles,
                result.overriddenFiles, result.deletedFiles, cancelToken);
        }

        if (cancelToken && cancelToken->is_cancelled()) {
            result.success = false;
            result.message = "Merge cancelled";
            return result;
        }

        if (fs::exists(childDir) && fs::is_directory(childDir)) {
            std::vector<std::string> copied;
            copyDirectoryWithManifest(childDir, outputDir,
                overridesDir, manifest, true,
                copied, result.overriddenFiles,
                result.deletedFiles, cancelToken);
            for (auto& f : copied)
                if (std::find(result.mergedFiles.begin(), result.mergedFiles.end(), f) == result.mergedFiles.end())
                    result.mergedFiles.push_back(f);
        }

        result.success = true;
        result.message = "Merged " + std::to_string(result.mergedFiles.size()) +
            " files, " + std::to_string(result.overriddenFiles.size()) + " overridden";
    } catch (const std::exception& e) {
        result.success = false;
        result.message = std::string("Merge exception: ") + e.what();
    }
    return result;
}

bool BranchMerger::copyFileSilent(const std::string& src, const std::string& dst, bool overwrite)
{
    try {
        fs::copy_options opts = overwrite
            ? fs::copy_options::overwrite_existing : fs::copy_options::none;
        std::error_code ec;
        fs::copy_file(src, dst, opts, ec);
        if (ec) {
            CLogger::Warn("BranchMerger: copy failed {} -> {}: {}", src, dst, ec.message());
            return false;
        }
        QFile::setPermissions(QString::fromStdString(dst),
            QFileDevice::ReadOwner | QFileDevice::WriteOwner |
            QFileDevice::ReadGroup | QFileDevice::ReadOther);
        return true;
    } catch (const std::exception& e) {
        CLogger::Error("BranchMerger::copyFileSilent: {}", e.what());
        return false;
    }
}

void BranchMerger::copyDirectoryWithManifest(const std::string& srcDir,
    const std::string& dstDir, const std::string& overridesDir,
    const NeoWorkspace::BranchManifest& manifest,
    bool isRoot, std::vector<std::string>& copiedFiles,
    std::vector<std::string>& overridden,
    std::vector<std::string>& deletedFiles,
    NeoCore::CancelToken* cancelToken)
{
    std::error_code ec;
    fs::create_directories(dstDir, ec);

    for (const auto& entry : fs::recursive_directory_iterator(srcDir,
        fs::directory_options::skip_permission_denied)) {
        if (cancelToken && cancelToken->is_cancelled()) return;
        if (!entry.is_regular_file()) continue;

        fs::path rel = fs::relative(entry.path(), srcDir);
        std::string relStr = rel.generic_string();

        if (entry.path().filename() == "branch_manifest.json") continue;
        if (relStr.find(".overrides/") == 0) continue;

        auto it = manifest.markers.find(relStr);
        NeoWorkspace::FileMarker marker = (it != manifest.markers.end())
            ? it->second : NeoWorkspace::FileMarker::None;

        fs::path dst = fs::path(dstDir) / relStr;

        if (marker == NeoWorkspace::FileMarker::Delete) {
            if (fs::exists(dst)) {
                fs::remove(dst, ec);
                deletedFiles.push_back(relStr);
            }
            continue;
        }

        std::string srcPath;
        if (marker == NeoWorkspace::FileMarker::Override &&
            !overridesDir.empty() && fs::exists(overridesDir)) {
            srcPath = overridesDir + "/" + relStr;
            if (!fs::exists(srcPath)) srcPath = entry.path().string();
        } else {
            srcPath = entry.path().string();
        }

        fs::create_directories(dst.parent_path(), ec);
        bool existed = fs::exists(dst);
        bool forceOver = (marker == NeoWorkspace::FileMarker::Override)
            || (isRoot && existed);

        if (copyFileSilent(srcPath, dst.string(), forceOver)) {
            if (existed) overridden.push_back(relStr);
            else copiedFiles.push_back(relStr);
        }
    }
}

} // namespace NeoBuild
