#pragma once

#include <string>
#include <vector>
#include <memory>
#include <IModpackExporter.h>
#include <cancel_token.h>

namespace NeoBuild {

class ModpackExporter {
public:
    ModpackExporter();
    ~ModpackExporter();

    void scanExporters(const std::string& exportersDir);

    std::vector<std::string> availableFormats() const;

    std::string formatDescription(const std::string& format) const;

    bool exportModpack(const std::string& format,
        const std::string& buildDir,
        const std::string& outputPath,
        const NeoCore::ExportMetadata& metadata,
        NeoCore::CancelToken* cancelToken = nullptr);

    // 直接取插件实例（构建引擎入口 build_modpack 需经此调用）。
    // 返回裸指针，生命周期归 ModpackExporter 所有，scanExporters 之后稳定。
    NeoCore::IModpackExporter* exporterForFormat(const std::string& format);
    const NeoCore::IModpackExporter* exporterForFormat(const std::string& format) const;

    nlohmann::json previewStructure(const std::string& format,
        const std::string& buildDir,
        const NeoCore::ExportMetadata& metadata,
        const std::string& targetDir = "");

private:
    struct LoadedExporter {
        std::unique_ptr<NeoCore::IModpackExporter> instance;
        std::string format;
        std::string extension;
        std::string description;
        void* handle;
    };

    std::vector<LoadedExporter> exporters_;

    void loadExporter(const std::string& dllPath, const std::string& metaPath);
    void unloadAll();
    const LoadedExporter* findExporter(const std::string& format) const;
};

} // namespace NeoBuild

