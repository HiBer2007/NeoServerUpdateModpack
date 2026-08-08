#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <cancel_token.h>
#include <IBuildProgress.h>

namespace NeoCore {

struct ExportMetadata {
    std::string name;
    std::string version;
    std::string author;
    std::string game_version;
    std::string modloader;
    std::string modloader_version;
    std::string summary;
    std::string description;
    std::vector<std::string> language_files;
    nlohmann::json extra;
};

// 构建目标：主程序构造后交给导出插件内的构建引擎。
struct BuildTarget {
    std::string workspace_path;   // 仓库工作目录（已 clone/fetch 的缓存仓库）
    std::string workspace_json;   // workspace.json 路径，为空则由插件自行查找
    std::string cache_dir;        // 哈希缓存目录（.NSUM/cache 复用）
    std::string output_path;      // sync_to_directory=false: 中间构建目录；true: 目标工作目录（同步目标）
    std::string staging_dir;      // sync_to_directory=true: 中间构建目录（source），空 = 插件默认
    std::string branch;           // 整合包分支
    bool sync_to_directory = false; // true = 工作区同步（HMCL，输出即目录）；false = 打包格式
    ExportMetadata metadata;
};

class IModpackExporter {
public:
    virtual ~IModpackExporter() = default;

    virtual std::string format_name() const = 0;

    virtual std::string file_extension() const = 0;

    virtual std::string format_description() const = 0;

    // 插件内构建引擎入口：执行完整构建流程（clone/fetch → checkout → merge
    // → files → configs → custom mods → serverconfig → finalize）。
    // progress/cancel 可为空指针，此时静默降级为无 UI 构建。
    virtual BuildResult build_modpack(
        const BuildTarget& target,
        IBuildProgress* progress,
        CancelToken* cancel)
    {
        (void)target;
        (void)progress;
        (void)cancel;
        BuildResult result;
        result.success = false;
        result.errorMessage = "build_modpack not implemented";
        return result;
    }

    virtual bool export_modpack(
        const std::string& build_dir,
        const std::string& output_path,
        const ExportMetadata& metadata) = 0;

    // Simulate the final archive structure without writing any file.
    // Returns a JSON array of entries: [{"path": "...", "dir": true|false, "umd": ""|"U"|"M"|"D"}, ...]
    // target_dir: 目标工作目录（hmcl 真实比对基准，生成 U/M/D）；空 = 不做比对（全部未更改）
    virtual nlohmann::json preview_structure(
        const std::string& build_dir,
        const ExportMetadata& metadata,
        const std::string& target_dir = "")
    {
        (void)build_dir;
        (void)metadata;
        (void)target_dir;
        return nlohmann::json::array();
    }
};

using CreateExporterFunc = IModpackExporter* (*)();

} // namespace NeoCore
