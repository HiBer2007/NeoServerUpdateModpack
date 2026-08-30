#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <cancel_token.h>

namespace NeoBuild {

struct ServerConfigEntry {
    std::string worldName;
    std::string configPath;
    std::string relativePath;
    std::string content;
};

enum class ServerConfigMode {
    Full,      // full: 应用本层设置 (遵守 save/[save]/serverconfig 文件夹同步模式 folder_mode)
    Force,     // force: 强制覆盖 (与 config 同步逻辑统一)
    Partial,   // partial: 半同步 merge (IConfigParser 追踪键, tracked_keys)
    Ignore     // ignore: 不碰
};

enum class ServerConfigFolderMode {
    Skip,                // skip: 不处理
    Mirror,              // mirror: 严格覆盖
    IncrementalAdd,      // incremental_add: 只补缺失, 已存在不动
    IncrementalOverwrite // incremental_overwrite: 保留多余项, 被改过也写入
};

// serverconfig 特殊同步（L3）：
// 规则存储于仓库 branches/<branch>/save/[save]/serverconfig/
//   （save = 存档文件夹, [save] = 单个存档目录占位, 均为字面目录名）
//   <源文件本体>          -> 镜像内容，同步到目标每个存档的 serverconfig/
//   .rule/globle.json    -> { default_mode, folder_mode, version, description }
//                           folder_mode: 本层文件夹同步模式 (full 模式应用,
//                           skip|mirror|incremental_add|incremental_overwrite)
//   .rule/list.json      -> { files: { <rel>: {mode, tracked_keys} } } 逐文件同步模式
//                           (兼容旧字符串格式: { <rel>: "mode" })
//   .rule/<其他文件>      -> 规则文件组（预留，同步时忽略）
// 模式语义与 sync_policies.files 统一: full/force/partial/ignore;
// full = 应用本层 folder_mode; partial 的 tracked_keys 与 config 同步逻辑一致
// (为空时取 list_keys 全部键)。
class ServerConfigSync {
public:
    ServerConfigSync();

    bool init(const std::string& savesDir, const std::string& repoRoot,
        const std::string& branchName);

    bool hasRules() const;

    std::vector<ServerConfigEntry> scanServerConfigs();

    bool syncConfig(const ServerConfigEntry& entry,
        NeoCore::CancelToken* cancelToken = nullptr);

    bool syncAll(NeoCore::CancelToken* cancelToken = nullptr);

    int syncedCount() const { return synced_; }
    int skippedCount() const { return skipped_; }
    int failedCount() const { return failed_; }

private:
    std::string savesDir_;
    std::string repoRoot_;
    std::string branchName_;
    std::string ruleDir_;

    ServerConfigMode defaultMode_ = ServerConfigMode::Full;
    ServerConfigFolderMode folderMode_ = ServerConfigFolderMode::Mirror;
    std::map<std::string, ServerConfigMode> fileModes_;
    std::map<std::string, std::vector<std::string>> fileTrackedKeys_;

    std::vector<ServerConfigEntry> entries_;
    int synced_ = 0;
    int skipped_ = 0;
    int failed_ = 0;

    void loadRules();
    ServerConfigMode modeFor(const std::string& relPath) const;
    std::vector<std::string> trackedKeysFor(const std::string& relPath) const;
    std::string sourcePathFor(const std::string& relPath) const;
    bool syncOverwrite(const ServerConfigEntry& entry, const std::string& remoteContent);
    bool syncByFolderMode(const ServerConfigEntry& entry,
        const std::string& remoteContent);
    bool syncPartial(const ServerConfigEntry& entry, const std::string& remoteContent);

    std::string readFile(const std::string& filepath) const;
    bool writeFile(const std::string& filepath, const std::string& content) const;
};

} // namespace NeoBuild
