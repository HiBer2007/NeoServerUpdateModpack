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
    Overwrite,   // full: 整文件覆盖（镜像）
    Partial,     // partial: 半同步 merge（IConfigParser 追踪键）
    Ignore       // ignore: 不碰
};

// serverconfig 特殊同步（L3）：
// 规则存储于仓库 branches/<branch>/[save]/serverconfig/（[save] 为字面目录名）
//   <源文件本体>          -> 镜像内容，同步到目标每个存档的 serverconfig/
//   .rule/globle.json    -> { default_mode, version, description } 未清单文件的默认行为
//   .rule/list.json      -> { files: { <rel>: overwrite|partial|ignore } } 逐文件同步模式
//   .rule/<其他文件>      -> 规则文件组（预留，同步时忽略）
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

    ServerConfigMode defaultMode_ = ServerConfigMode::Overwrite;
    std::map<std::string, ServerConfigMode> fileModes_;

    std::vector<ServerConfigEntry> entries_;
    int synced_ = 0;
    int skipped_ = 0;
    int failed_ = 0;

    void loadRules();
    ServerConfigMode modeFor(const std::string& relPath) const;
    std::string sourcePathFor(const std::string& relPath) const;
    bool syncOverwrite(const ServerConfigEntry& entry, const std::string& remoteContent);
    bool syncPartial(const ServerConfigEntry& entry, const std::string& remoteContent);

    std::string readFile(const std::string& filepath) const;
    bool writeFile(const std::string& filepath, const std::string& content) const;
};

} // namespace NeoBuild
