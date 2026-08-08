#include "modpack_exporter.h"
#include <logger.h>
#include <plugin_log_sink.h>
#include <filesystem>
#include <fstream>
#include <QFile>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace NeoBuild {

namespace fs = std::filesystem;

ModpackExporter::ModpackExporter() = default;

ModpackExporter::~ModpackExporter()
{
    unloadAll();
}

void ModpackExporter::unloadAll()
{
    for (auto& exp : exporters_) {
        exp.instance.reset();
        if (exp.handle) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(exp.handle));
#else
            dlclose(exp.handle);
#endif
            exp.handle = nullptr;
        }
    }
    exporters_.clear();
}

void ModpackExporter::scanExporters(const std::string& exportersDir)
{
    if (!fs::exists(exportersDir) || !fs::is_directory(exportersDir)) {
        CLogger::Info("ModpackExporter: exporters dir not found: {}", exportersDir);
        return;
    }

    unloadAll();

    for (const auto& entry : fs::directory_iterator(exportersDir,
        fs::directory_options::skip_permission_denied)) {

if (!entry.is_regular_file()) continue;

const std::string fname = entry.path().filename().string();
        const std::string metaSuffix = ".meta.json";
        if (fname.size() < metaSuffix.size()
            || fname.compare(fname.size() - metaSuffix.size(),
                metaSuffix.size(), metaSuffix) != 0) continue;

        fs::path dllPath = entry.path().parent_path()
            / (fname.substr(0, fname.size() - metaSuffix.size()) + ".dll");

#if defined(__linux__) || defined(__APPLE__)
        fs::path soPath = entry.path();
        soPath.replace_extension(".so");
        if (!fs::exists(dllPath) && fs::exists(soPath)) {
            dllPath = soPath;
        }

        fs::path dylibPath = entry.path();
        dylibPath.replace_extension(".dylib");
        if (!fs::exists(dllPath) && fs::exists(dylibPath)) {
            dllPath = dylibPath;
        }
#endif

        if (!fs::exists(dllPath)) {
            CLogger::Warn("ModpackExporter: DLL not found for meta: {}",
                entry.path().filename().string());
            continue;
        }

        loadExporter(dllPath.string(), entry.path().string());
    }

    CLogger::Info("ModpackExporter: loaded {} exporters", exporters_.size());
}

void ModpackExporter::loadExporter(const std::string& dllPath,
    const std::string& metaPath)
{
    try {
        nlohmann::json meta;
        QFile mf(QString::fromStdString(metaPath));
        if (!mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            CLogger::Warn("ModpackExporter: cannot read meta: {}", metaPath);
            return;
        }
        QByteArray raw = mf.readAll();
        mf.close();

        try {
            meta = nlohmann::json::parse(raw.toStdString());
        } catch (const nlohmann::json::exception& e) {
            CLogger::Error("ModpackExporter: invalid meta JSON {}: {}", metaPath, e.what());
            return;
        }

        void* handle = nullptr;
#ifdef _WIN32
        std::wstring widePath(dllPath.begin(), dllPath.end());
        handle = LoadLibraryW(widePath.c_str());
        if (!handle) {
            DWORD err = GetLastError();
            CLogger::Error("ModpackExporter: LoadLibrary failed for {}: error {}",
                dllPath, err);
            return;
        }
#else
        handle = dlopen(dllPath.c_str(), RTLD_NOW);
        if (!handle) {
            CLogger::Error("ModpackExporter: dlopen failed for {}: {}",
                dllPath, dlerror());
            return;
        }
#endif

#ifdef _WIN32
        auto createFunc = reinterpret_cast<NeoCore::CreateExporterFunc>(
            GetProcAddress(static_cast<HMODULE>(handle), "CreateExporter"));
#else
        auto createFunc = reinterpret_cast<NeoCore::CreateExporterFunc>(
            dlsym(handle, "CreateExporter"));
#endif

        if (!createFunc) {
            CLogger::Error("ModpackExporter: CreateExporter not found in {}", dllPath);
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            return;
        }

        // 插件日志注入 (可选): 插件导出 SetPluginLogSink 则注入宿主 sink
        static LoggerLogSink exporterLogSink;
#ifdef _WIN32
        if (auto setSink = reinterpret_cast<void (*)(ILogSink*)>(
                GetProcAddress(static_cast<HMODULE>(handle), "SetPluginLogSink"))) {
            setSink(&exporterLogSink);
        }
#else
        if (auto setSink = reinterpret_cast<void (*)(ILogSink*)>(
                dlsym(handle, "SetPluginLogSink"))) {
            setSink(&exporterLogSink);
        }
#endif

        NeoCore::IModpackExporter* instance = createFunc();
        if (!instance) {
            CLogger::Error("ModpackExporter: CreateExporter returned null from {}", dllPath);
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            return;
        }

        LoadedExporter loaded;
        loaded.instance.reset(instance);
        loaded.handle = handle;
        loaded.format = instance->format_name();
        loaded.extension = instance->file_extension();
        loaded.description = instance->format_description();

        if (meta.contains("format_name")) {
            loaded.format = meta["format_name"].get<std::string>();
        }
        if (meta.contains("file_extension")) {
            loaded.extension = meta["file_extension"].get<std::string>();
        }
        if (meta.contains("description")) {
            loaded.description = meta["description"].get<std::string>();
        }

        exporters_.push_back(std::move(loaded));

        CLogger::Info("ModpackExporter: loaded {} ({})", loaded.format, dllPath);
    } catch (const std::exception& e) {
        CLogger::Error("ModpackExporter::loadExporter exception: {}", e.what());
    }
}

std::vector<std::string> ModpackExporter::availableFormats() const
{
    std::vector<std::string> formats;
    formats.reserve(exporters_.size());
    for (const auto& exp : exporters_) {
        formats.push_back(exp.format);
    }
    return formats;
}

std::string ModpackExporter::formatDescription(const std::string& format) const
{
    const auto* exp = findExporter(format);
    if (exp) {
        return exp->description;
    }
    return "";
}

bool ModpackExporter::exportModpack(const std::string& format,
    const std::string& buildDir,
    const std::string& outputPath,
    const NeoCore::ExportMetadata& metadata,
    NeoCore::CancelToken* cancelToken)
{
    if (cancelToken && cancelToken->is_cancelled()) {
        CLogger::Info("ModpackExporter: export cancelled");
        return false;
    }

    const auto* exp = findExporter(format);
    if (!exp) {
        CLogger::Error("ModpackExporter: format not available: {}", format);
        return false;
    }

    if (!fs::exists(buildDir)) {
        CLogger::Error("ModpackExporter: build dir not found: {}", buildDir);
        return false;
    }

    try {
        fs::path outPath(outputPath);
        fs::create_directories(outPath.parent_path());

        CLogger::Info("ModpackExporter: exporting to {} using format {}",
            outputPath, format);

        if (cancelToken && cancelToken->is_cancelled()) {
            CLogger::Info("ModpackExporter: export cancelled before calling exporter");
            return false;
        }

        bool ok = exp->instance->export_modpack(buildDir, outputPath, metadata);

        if (cancelToken && cancelToken->is_cancelled()) {
            CLogger::Info("ModpackExporter: export cancelled after exporter call");
            return false;
        }

        if (ok) {
            CLogger::Info("ModpackExporter: export successful: {}", outputPath);
        } else {
            CLogger::Error("ModpackExporter: export failed for format {}", format);
        }

        return ok;
    } catch (const std::exception& e) {
        CLogger::Error("ModpackExporter::exportModpack exception: {}", e.what());
        return false;
    }
}

const ModpackExporter::LoadedExporter* ModpackExporter::findExporter(
    const std::string& format) const
{
    for (const auto& exp : exporters_) {
        if (exp.format == format) {
            return &exp;
        }
    }
    return nullptr;
}

NeoCore::IModpackExporter* ModpackExporter::exporterForFormat(
    const std::string& format)
{
    return const_cast<NeoCore::IModpackExporter*>(
        static_cast<const ModpackExporter*>(this)->exporterForFormat(format));
}

const NeoCore::IModpackExporter* ModpackExporter::exporterForFormat(
    const std::string& format) const
{
    const auto* exp = findExporter(format);
    if (exp) {
        return exp->instance.get();
    }
    return nullptr;
}

nlohmann::json ModpackExporter::previewStructure(const std::string& format,
    const std::string& buildDir,
    const NeoCore::ExportMetadata& metadata,
    const std::string& targetDir)
{
    const auto* exp = findExporter(format);
    if (!exp) {
        CLogger::Error("ModpackExporter: format not available for preview: {}", format);
        return nlohmann::json::array();
    }
    try {
        return exp->instance->preview_structure(buildDir, metadata, targetDir);
    } catch (const std::exception& e) {
        CLogger::Error("ModpackExporter::previewStructure exception: {}", e.what());
        return nlohmann::json::array();
    }
}

} // namespace NeoBuild

