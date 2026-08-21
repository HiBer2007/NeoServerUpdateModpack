#include "sync_policy_executor.h"
#include "mod_metadata.h"
#include <sync_policy.h>
#include <logger.h>
#include <PluginLoader.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>

namespace NeoBuild {

namespace fs = std::filesystem;

namespace {

constexpr const char* kNsumDir = ".NSUM";
constexpr const char* kCustomModsSub = "custom/mod";
constexpr const char* kHashesFile = ".NSUM/hashes.json";
constexpr const char* kHmclDir = ".hmcl";

std::string normalizeRel(const std::string& path)
{
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}

std::string relOf(const fs::path& base, const fs::path& p)
{
    fs::path rel = fs::relative(p, base);
    if (rel.empty()) return "";
    return normalizeRel(rel.string());
}

bool isProtectedRoot(const std::string& rel)
{
    return rel == kNsumDir || rel.rfind(".NSUM/", 0) == 0
        || rel == kHmclDir || rel.rfind(".hmcl/", 0) == 0;
}

std::string computeSha256(const std::string& filepath)
{
    QFile f(QString::fromStdString(filepath));
    if (!f.open(QIODevice::ReadOnly)) {
        return "";
    }

    QCryptographicHash hasher(QCryptographicHash::Sha256);
    char buffer[1024 * 1024];
    qint64 read = 0;
    while ((read = f.read(buffer, sizeof(buffer))) > 0) {
        hasher.addData(buffer, static_cast<int>(read));
    }
    f.close();
    return hasher.result().toHex().toStdString();
}

std::string readFile(const std::string& filepath)
{
    QFile f(QString::fromStdString(filepath));
    if (!f.open(QIODevice::ReadOnly)) {
        return "";
    }
    std::string content = f.readAll().toStdString();
    f.close();
    return content;
}

bool writeFile(const std::string& filepath, const std::string& content)
{
    fs::create_directories(fs::path(filepath).parent_path());
    QFile f(QString::fromStdString(filepath));
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    f.write(content.data(), static_cast<qint64>(content.size()));
    f.close();
    return true;
}

bool copyFile(const std::string& src, const std::string& dst)
{
    std::error_code ec;
    fs::create_directories(fs::path(dst).parent_path(), ec);
    if (ec) return false;
    ec.clear();
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

bool moveFile(const std::string& src, const std::string& dst)
{
    std::error_code ec;
    fs::create_directories(fs::path(dst).parent_path(), ec);
    if (ec) return false;
    ec.clear();
    fs::rename(src, dst, ec);
    if (ec) {
        ec.clear();
        if (!copyFile(src, dst)) return false;
        fs::remove(src, ec);
    }
    return true;
}

} // namespace

struct ExecutorImpl {
    using Result = SyncPolicyExecutor::Result;
    std::string sourceDir;
    std::string targetDir;
    NeoWorkspace::SyncPolicy policy;
    NeoCore::IBuildProgress* progress;
    NeoCore::CancelToken* cancel;
    Result result;

    std::unordered_map<std::string, std::string> hashes;
    std::unordered_set<std::string> sourceFiles;
    std::vector<std::string> mirrorScopes;
    std::string modsScope;
    bool hasModsMirror = false;

    std::vector<std::string> parsersDir;

    bool cancelled() const
    {
        if (cancel && cancel->is_cancelled()) return true;
        if (progress && progress->is_cancelled()) return true;
        return false;
    }

    // L2: 配置文件特化，返回匹配条目或 nullptr
    const NeoWorkspace::SyncPolicyFile* matchFilePolicy(const std::string& rel) const
    {
        for (const auto& f : policy.files) {
            if (f.path == rel) return &f;
            if (!f.path.empty() && f.path.back() == '*') {
                std::string prefix = f.path.substr(0, f.path.size() - 1);
                if (rel.rfind(prefix, 0) == 0) return &f;
            }
        }
        return nullptr;
    }

    // L1: 文件夹策略最长前缀匹配，返回 policy 字符串（可能为 default）
    std::string matchFolderPolicy(const std::string& rel) const
    {
        std::string best;
        size_t bestLen = 0;

        for (const auto& f : policy.folders) {
            if (f.path.empty()) {
                if (bestLen == 0) best = f.policy;
                continue;
            }
            if (rel == f.path || rel.rfind(f.path + "/", 0) == 0) {
                if (f.path.size() > bestLen) {
                    bestLen = f.path.size();
                    best = f.policy;
                }
            }
        }

        if (!best.empty()) return best;
        return policy.defaultFolderPolicy;
    }

    std::string resolveDefault(const std::string& policyName) const
    {
        if (policyName == "default") {
            std::string d = policy.defaultFolderPolicy;
            return (d == "default" || d.empty()) ? "incremental_add" : d;
        }
        return policyName;
    }

    bool isModsScope(const std::string& scope) const
    {
        return scope == "mods" || (scope.size() > 5 && scope.rfind("/mods") == scope.size() - 5);
    }

    void loadHashes()
    {
        hashes.clear();
        std::string hashPath = (fs::path(targetDir) / kHashesFile).string();
        try {
            std::ifstream in(hashPath);
            if (!in.is_open()) return;
            auto j = nlohmann::json::parse(in);
            if (j.is_object()) {
                for (auto& [k, v] : j.items()) {
                    if (v.is_string()) {
                        hashes[k] = v.get<std::string>();
                    }
                }
            }
        }
        catch (const std::exception& e) {
            CLogger::Warn("SyncPolicyExecutor: failed to load {}: {}",
                kHashesFile, e.what());
        }
    }

    void saveHashes()
    {
        nlohmann::json j = nlohmann::json::object();
        std::vector<std::string> keys;
        keys.reserve(hashes.size());
        for (const auto& [k, v] : hashes) {
            (void)v;
            keys.push_back(k);
        }
        std::sort(keys.begin(), keys.end());
        for (const auto& k : keys) {
            j[k] = hashes[k];
        }
        writeFile((fs::path(targetDir) / kHashesFile).string(), j.dump(2) + "\n");
    }

    // 目标是否存在（且是文件）
    bool targetExists(const std::string& rel) const
    {
        return fs::is_regular_file(fs::path(targetDir) / rel);
    }

    bool copyIfNeeded(const std::string& rel, bool overwrite)
    {
        std::string srcPath = (fs::path(sourceDir) / rel).string();
        std::string dstPath = (fs::path(targetDir) / rel).string();

        if (!overwrite && targetExists(rel)) {
            ++result.skippedFiles;
            return true;
        }

        std::string srcHash = computeSha256(srcPath);
        if (srcHash.empty()) {
            result.warnings.push_back("Sync failed (unreadable source): " + rel);
            return false;
        }

        if (overwrite && targetExists(rel)) {
            std::string dstHash = computeSha256(dstPath);
            if (dstHash == srcHash) {
                ++result.skippedFiles;
                hashes[rel] = srcHash;
                return true;
            }
        }

        if (!copyFile(srcPath, dstPath)) {
            result.warnings.push_back("Sync failed (copy error): " + rel);
            return false;
        }

        hashes[rel] = srcHash;
        ++result.copiedFiles;
        return true;
    }

    void mergeFile(const std::string& rel, const NeoWorkspace::SyncPolicyFile& filePolicy)
    {
        std::string srcPath = (fs::path(sourceDir) / rel).string();
        std::string dstPath = (fs::path(targetDir) / rel).string();

        std::string localContent;
        if (targetExists(rel)) {
            localContent = readFile(dstPath);
        }

        NeoCore::PluginLoader loader;
        std::string parsers = (fs::path(
            QCoreApplication::applicationDirPath().toStdString()) / "parsers").string();
        if (fs::exists(parsers)) {
            loader.ScanDirectory(parsers);
        }

        NeoCore::IConfigParser* parser = loader.FindParser(srcPath);
        if (!parser) {
            CLogger::Warn(
                "SyncPolicyExecutor: no parser for '{}', falling back to full copy", rel);
            if (!copyIfNeeded(rel, true)) {
                result.warnings.push_back("Sync failed (no parser): " + rel);
            }
            return;
        }

        std::vector<std::string> trackedKeys = filePolicy.trackedKeys;
        if (trackedKeys.empty()) {
            trackedKeys = parser->list_keys(srcPath);
        }

        std::string remoteContent = readFile(srcPath);
        std::string merged = parser->merge_entries(
            srcPath, trackedKeys, remoteContent, localContent);

        if (merged == localContent) {
            ++result.skippedFiles;
            return;
        }

        if (!writeFile(dstPath, merged)) {
            result.warnings.push_back("Sync failed (merge write error): " + rel);
            return;
        }

        hashes[rel] = computeSha256(srcPath);
        ++result.mergedFiles;
    }

    void collectSourceFiles()
    {
        sourceFiles.clear();

        if (!fs::is_directory(sourceDir)) return;

        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(
                 sourceDir, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            const fs::directory_entry& entry = *it;
            if (!entry.is_regular_file()) continue;

            std::string rel = relOf(sourceDir, entry.path());
            if (rel.empty() || isProtectedRoot(rel)) continue;

            sourceFiles.insert(rel);
        }
    }

    void collectScopes()
    {
        mirrorScopes.clear();
        hasModsMirror = false;
        modsScope.clear();

        for (const auto& f : policy.folders) {
            std::string p = resolveDefault(f.policy);
            if (p == "mirror") {
                mirrorScopes.push_back(f.path);
            }
            if (isModsScope(f.path) && p == "mirror") {
                hasModsMirror = true;
                modsScope = f.path;
            }
        }
    }

    bool inScope(const std::string& scope, const std::string& rel) const
    {
        if (scope.empty()) return true;
        return rel == scope || rel.rfind(scope + "/", 0) == 0;
    }

    // 通用 mirror 删除多余项（mods 作用域交给 runModsMirror）
    void deleteExtras()
    {
        if (!fs::is_directory(targetDir)) return;

        for (const auto& scope : mirrorScopes) {
            if (isModsScope(scope) && hasModsMirror) continue;

            std::error_code ec;
            fs::path scopePath = fs::path(targetDir) / scope;
            if (!fs::exists(scopePath)) continue;

            for (auto it = fs::recursive_directory_iterator(
                     scopePath, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                if (cancelled()) return;

                const fs::directory_entry& entry = *it;
                if (!entry.is_regular_file()) continue;

                std::string rel = relOf(targetDir, entry.path());
                if (rel.empty() || isProtectedRoot(rel)) continue;
                if (sourceFiles.count(rel)) continue;

                std::error_code r;
                fs::remove(entry.path(), r);
                if (!r) {
                    result.warnings.push_back("Sync failed (delete error): " + rel);
                    continue;
                }
                hashes.erase(rel);
                ++result.deletedFiles;
                CLogger::Debug("SyncPolicyExecutor: deleted extra {}", rel);
            }
        }
    }

    // mods 严格镜像：harvest custom → 清除 .disabled → 复制库回 mods（冲突检测）
    void runModsMirror()
    {
        fs::path sourceMods = fs::path(sourceDir) / modsScope;
        fs::path targetMods = fs::path(targetDir) / modsScope;
        fs::path customLib = fs::path(targetDir) / kNsumDir / kCustomModsSub;

        std::error_code ec;
        fs::create_directories(customLib, ec);

        std::unordered_set<std::string> sourceJars;
        std::vector<std::string> sourceJarPaths;
        if (fs::is_directory(sourceMods)) {
            for (const auto& entry : fs::directory_iterator(
                     sourceMods, fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                std::string name = entry.path().filename().string();
                if (name.size() < 4 ||
                    name.compare(name.size() - 4, 4, ".jar") != 0) {
                    continue;
                }
                sourceJars.insert(name);
                sourceJarPaths.push_back(entry.path().string());
            }
        }

        std::unordered_set<std::string> sourceModIds;
        for (const auto& jar : sourceJarPaths) {
            auto ids = extractModIds(jar);
            sourceModIds.insert(ids.begin(), ids.end());
        }

        // harvest + .disabled 清除
        if (fs::is_directory(targetMods)) {
            for (const auto& entry : fs::directory_iterator(
                     targetMods, fs::directory_options::skip_permission_denied)) {
                if (cancelled()) return;
                if (!entry.is_regular_file()) continue;

                std::string name = entry.path().filename().string();
                if (sourceJars.count(name)) continue;

                std::string relInTarget = modsScope.empty()
                    ? name
                    : modsScope + "/" + name;

                std::string base = name;
                bool isDisabled = false;
                if (name.size() > 12 &&
                    name.compare(name.size() - 12, 12, ".jar.disabled") == 0) {
                    isDisabled = true;
                    base = name.substr(0, name.size() - 12);
                }
                if (isDisabled && sourceJars.count(base + ".jar")) {
                    std::error_code r;
                    fs::remove(entry.path(), r);
                    if (!r) {
                        result.warnings.push_back(
                            "Sync failed (disabled mod cleanup): " + name);
                    } else {
                        ++result.deletedFiles;
                        CLogger::Info(
                            "SyncPolicyExecutor: cleared disabled repo mod {}", name);
                    }
                    continue;
                }

                // 上次同步写入的仓库内容（有 hash 记录）已从仓库移除 -> 删除
                if (hashes.count(relInTarget)) {
                    std::error_code r;
                    fs::remove(entry.path(), r);
                    if (!r) {
                        result.warnings.push_back(
                            "Sync failed (removed repo mod cleanup): " + name);
                    } else {
                        hashes.erase(relInTarget);
                        ++result.deletedFiles;
                        CLogger::Info(
                            "SyncPolicyExecutor: removed repo mod {} (no longer in repo)",
                            name);
                    }
                    continue;
                }

                std::string libPath = (customLib / name).string();
                if (fs::exists(libPath)) {
                    std::error_code r;
                    fs::remove(entry.path(), r);
                    if (!r) {
                        result.warnings.push_back(
                            "Sync failed (custom mod cleanup): " + name);
                    }
                    continue;
                }

                std::error_code r;
                fs::rename(entry.path(), libPath, r);
                if (r) {
                    r.clear();
                    if (!copyFile(entry.path().string(), libPath)) {
                        result.warnings.push_back(
                            "Sync failed (custom mod harvest): " + name);
                        continue;
                    }
                    fs::remove(entry.path(), r);
                }
                ++result.customHarvested;
                CLogger::Info(
                    "SyncPolicyExecutor: harvested custom mod {} -> {}/",
                    name, kNsumDir + std::string("/") + kCustomModsSub);
            }
        }

        // restore：库中模组复制回 mods（modId 冲突则保留在库中 + 警告）
        if (fs::is_directory(customLib)) {
            for (const auto& entry : fs::directory_iterator(
                     customLib, fs::directory_options::skip_permission_denied)) {
                if (cancelled()) return;
                if (!entry.is_regular_file()) continue;

                std::string name = entry.path().filename().string();
                if (name.size() < 4) continue;
                bool isJar = name.compare(name.size() - 4, 4, ".jar") == 0;
                bool isDisabledJar = name.size() >= 12 &&
                    name.compare(name.size() - 12, 12, ".jar.disabled") == 0;
                if (!isJar && !isDisabledJar) continue;

                std::string dstPath = (targetMods / name).string();
                if (fs::exists(dstPath)) continue;

                bool conflict = false;
                auto ids = extractModIds(entry.path().string());
                for (const auto& id : ids) {
                    if (sourceModIds.count(id)) {
                        conflict = true;
                        break;
                    }
                }

                if (conflict) {
                    result.warnings.push_back(
                        "Custom mod '" + name
                        + "' conflicts with a repo mod, kept in " + kNsumDir
                        + "/" + kCustomModsSub);
                    continue;
                }

                std::error_code r;
                fs::create_directories(targetMods, r);
                r.clear();
                fs::copy_file(entry.path(), dstPath,
                    fs::copy_options::overwrite_existing, r);
                if (r) {
                    result.warnings.push_back(
                        "Sync failed (custom mod restore): " + name);
                    continue;
                }

                ++result.customRestored;
                CLogger::Info(
                    "SyncPolicyExecutor: restored custom mod {}", name);
            }
        }
    }

    void run()
    {
        fs::path src = fs::path(sourceDir).lexically_normal();
        fs::path dst = fs::path(targetDir).lexically_normal();
        if (src == dst) {
            result.warnings.push_back(
                "Sync target equals build directory, skipping sync");
            result.success = true;
            return;
        }

        std::error_code ec;
        fs::create_directories(targetDir, ec);
        if (ec) {
            result.success = false;
            result.warnings.push_back("Sync failed (cannot create target dir): "
                + ec.message());
            return;
        }

        fs::create_directories(fs::path(targetDir) / kNsumDir / "cache", ec);
        if (ec) {
            result.success = false;
            result.warnings.push_back("Sync failed (cannot create .NSUM dir): "
                + ec.message());
            return;
        }

        loadHashes();
        collectSourceFiles();
        collectScopes();

        std::vector<std::string> sortedFiles(sourceFiles.begin(), sourceFiles.end());
        std::sort(sortedFiles.begin(), sortedFiles.end());

        const size_t total = sortedFiles.size();
        size_t done = 0;
        int subHandle = -1;
        if (progress) {
            subHandle = progress->add_sub_bar("同步目标工作目录");
        }

        for (const auto& rel : sortedFiles) {
            if (cancelled()) break;

            ++done;
            if (subHandle > 0 && (done % 32 == 0 || done == total)) {
                int pct = total > 0
                    ? static_cast<int>(done * 100 / total)
                    : 100;
                progress->set_sub_progress(subHandle, pct);
                progress->set_sub_info(subHandle, std::to_string(done)
                    + "/" + std::to_string(total));
            }

            const NeoWorkspace::SyncPolicyFile* filePolicy = matchFilePolicy(rel);
            if (filePolicy) {
                if (filePolicy->mode == "ignore") {
                    ++result.skippedFiles;
                    continue;
                }
                if (filePolicy->mode == "partial") {
                    mergeFile(rel, *filePolicy);
                    continue;
                }
                if (filePolicy->mode == "force") {
                    copyIfNeeded(rel, true);
                    continue;
                }
                // mode == "full": 遵守文件夹策略 (回落, 与无文件策略一致)
            } else if (std::find(policy.configFiles.begin(),
                policy.configFiles.end(), rel) != policy.configFiles.end()) {
                // 用户标记为配置文件的路径: 无解析器时按全量覆盖处理 (full 语义)
                copyIfNeeded(rel, true);
                continue;
            }

            std::string folderPolicy = resolveDefault(matchFolderPolicy(rel));
            if (folderPolicy == "skip") {
                ++result.skippedFiles;
                continue;
            }
            if (folderPolicy == "mirror") {
                copyIfNeeded(rel, true);
                continue;
            }
            if (folderPolicy == "incremental_add") {
                copyIfNeeded(rel, false);
                continue;
            }
            // incremental_overwrite
            copyIfNeeded(rel, true);
        }

        if (subHandle > 0) {
            progress->remove_sub_bar(subHandle);
        }

        if (cancelled()) {
            result.warnings.push_back("Sync to target cancelled");
            saveHashes();
            return;
        }

        deleteExtras();
        if (hasModsMirror) {
            runModsMirror();
        }

        saveHashes();
    }
};

SyncPolicyExecutor::Result SyncPolicyExecutor::execute(
    const std::string& sourceDir,
    const std::string& targetDir,
    const NeoWorkspace::SyncPolicy& policy,
    NeoCore::IBuildProgress* progress,
    NeoCore::CancelToken* cancel)
{
    ExecutorImpl impl;
    impl.sourceDir = sourceDir;
    impl.targetDir = targetDir;
    impl.policy = policy;
    impl.progress = progress;
    impl.cancel = cancel;

    try {
        impl.run();
    }
    catch (const std::exception& e) {
        impl.result.success = false;
        impl.result.warnings.push_back(
            std::string("Sync to target failed: ") + e.what());
    }

    return impl.result;
}

} // namespace NeoBuild



