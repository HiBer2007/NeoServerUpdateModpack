#include <IModpackExporter.h>
#include <plugin_log_sink.h>
#include <logger.h>
#include <build_engine.h>
#include <umd_generator.h>

#include <filesystem>

namespace fs = std::filesystem;

class HmclExporter : public NeoCore::IModpackExporter {
public:
    std::string format_name() const override { return "hmcl"; }

    std::string file_extension() const override { return ""; }

    std::string format_description() const override {
        return "HMCL 工作区同步（同步到游戏工作目录）";
    }

    NeoCore::BuildResult build_modpack(
        const NeoCore::BuildTarget& target,
        NeoCore::IBuildProgress* progress,
        NeoCore::CancelToken* cancel) override
    {
        if (target.output_path.empty()) {
            NeoCore::BuildResult r;
            r.success = false;
            r.errorMessage = "sync target directory is empty";
            return r;
        }

        NeoBuild::BuildEngine engine;
        std::string wsJson = target.workspace_json;
        if (wsJson.empty()) {
            wsJson = (fs::path(target.workspace_path) / "workspace.json").string();
        }

        std::string staging = target.staging_dir;
        if (staging.empty()) {
            staging = (fs::path(target.cache_dir).parent_path()
                / "staging" / target.branch).string();
        }

        if (!engine.init(wsJson, target.cache_dir, staging)) {
            NeoCore::BuildResult r;
            r.success = false;
            r.errorMessage = "Failed to initialize build engine";
            return r;
        }

        engine.setTargetDir(target.output_path);
        return engine.build(target.branch, progress, cancel);
    }

    nlohmann::json preview_structure(
        const std::string& build_dir,
        const NeoCore::ExportMetadata& metadata,
        const std::string& target_dir = "") override
    {
        (void)metadata;
        return NeoBuild::generateUmdStructure(build_dir, target_dir, nullptr, nullptr);
    }

    bool export_modpack(
        const std::string& build_dir,
        const std::string& output_path,
        const NeoCore::ExportMetadata& metadata) override
    {
        (void)build_dir;
        (void)output_path;
        (void)metadata;
        CLogger::Error(
            "HMCL is workspace sync mode: build writes directly to the target "
            "directory, zip export is not supported");
        return false;
    }
};

extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter()
{
    return new HmclExporter();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoExporter_HMCL")

