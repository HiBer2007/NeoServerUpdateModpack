#include <IModpackExporter.h>
#include <plugin_log_sink.h>
#include <logger.h>
#include <build_engine.h>
#include <nlohmann/json.hpp>
#include <libzippp/libzippp.h>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>

#include <algorithm>
#include <filesystem>

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace {

std::string relativePath(const std::string& base, const std::string& full) {
    if (full.size() <= base.size()) return "";
    std::string rel = full.substr(base.size());
    while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) {
        rel.erase(0, 1);
    }
    return rel;
}

bool pathExists(const std::string& path) {
    return QFileInfo::exists(QString::fromStdString(path));
}

bool isDirectory(const std::string& path) {
    return QFileInfo(QString::fromStdString(path)).isDir();
}

void addFileToZip(libzippp::ZipArchive& zf,
                  const std::string& localPath,
                  const std::string& entryName) {
    if (!zf.addFile(entryName, localPath)) {
        throw std::runtime_error("Failed to add file '" + entryName + "' to zip");
    }
}

void addEmptyDirToZip(libzippp::ZipArchive& zf, const std::string& entryName) {
    std::string e = entryName;
    if (e.empty()) return;
    if (e.back() != '/') e += '/';
    if (!zf.addEntry(e)) {
        throw std::runtime_error("Failed to add directory entry '" + e + "' to zip");
    }
}

std::string fullPathForRel(const std::string& base, const std::string& rel) {
    std::string path = base;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') path += '/';
    path += rel;
    return path;
}

void scanAndPackDirectory(libzippp::ZipArchive& zf,
                          const std::string& buildDir,
                          const std::string& overridePrefix) {
    std::string normBuildDir = buildDir;
    while (!normBuildDir.empty() &&
           (normBuildDir.back() == '/' || normBuildDir.back() == '\\')) {
        normBuildDir.pop_back();
    }
    if (normBuildDir.empty()) return;

    QDirIterator it(QString::fromStdString(normBuildDir),
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);

    std::vector<std::string> emptyDirs;

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        std::string fullPath = fi.absoluteFilePath().toStdString();
        std::string rel = relativePath(normBuildDir, fullPath);
        if (rel.empty()) continue;

        // save/ 是 L3 存档同步的规则与源，不属于整合包内容
        if (rel == "save" || rel.rfind("save/", 0) == 0
            || rel.rfind("save\\", 0) == 0) {
            continue;
        }

        if (fi.isDir()) {
            emptyDirs.push_back(rel);
            continue;
        }
        if (fi.isFile()) {
            std::string entryName = overridePrefix + "/" + rel;
            std::replace(entryName.begin(), entryName.end(), '\\', '/');
            addFileToZip(zf, fullPath, entryName);
        }
    }

    for (const auto& d : emptyDirs) {
        std::string checkPath = fullPathForRel(normBuildDir, d);
        QDirIterator childCheck(QString::fromStdString(checkPath),
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        if (!childCheck.hasNext()) {
            std::string entryName = overridePrefix + "/" + d;
            std::replace(entryName.begin(), entryName.end(), '\\', '/');
            addEmptyDirToZip(zf, entryName);
        }
    }
}

std::string extraString(const json& extra, const char* key, const std::string& fallback = "") {
    if (extra.is_object() && extra.contains(key) && extra[key].is_string()) {
        return extra[key].get<std::string>();
    }
    return fallback;
}

json buildManifest(const NeoCore::ExportMetadata& meta) {
    json j;
    j["schema"] = 1;
    j["type"] = "nsum";

    j["name"] = meta.name.empty() ? "NSUM" : meta.name;
    j["author"] = meta.author;
    j["version"] = meta.version;
    j["gameVersion"] = meta.game_version;
    j["modloader"] = meta.modloader;
    j["modloaderVersion"] = meta.modloader_version;
    j["description"] = meta.description.empty() ? meta.summary : meta.description;

    const json& extra = meta.extra;
    j["instance"] = {
        {"repo", extraString(extra, "repo")},
        {"branch", extraString(extra, "branch", "base")},
        {"gitBranch", extraString(extra, "git_branch")}
    };
    j["runtime"] = {
        {"minVersion", extraString(extra, "runtime_min_version", "1.0.0")}
    };
    return j;
}

} // namespace

namespace {

class NsumExporter : public NeoCore::IModpackExporter {
public:
    std::string format_name() const override { return "nsum"; }
    std::string file_extension() const override { return ".zip"; }
    std::string format_description() const override {
        return "NSUM \u6574\u5408\u5305\uff08HMCL \u7ecf NSUM Bridge \u63a5\u5165\u670d\u52a1\u5668\u540c\u6b65\uff09";
    }

    NeoCore::BuildResult build_modpack(
        const NeoCore::BuildTarget& target,
        NeoCore::IBuildProgress* progress,
        NeoCore::CancelToken* cancel) override
    {
        NeoBuild::BuildEngine engine;
        std::string wsJson = target.workspace_json;
        if (wsJson.empty()) {
            wsJson = (fs::path(target.workspace_path) / "workspace.json").string();
        }
        if (!engine.init(wsJson, target.cache_dir, target.output_path)) {
            NeoCore::BuildResult r;
            r.success = false;
            r.errorMessage = "Failed to initialize build engine";
            return r;
        }
        return engine.build(target.branch, progress, cancel, target.git_branch);
    }

    bool export_modpack(const std::string& build_dir,
                        const std::string& output_path,
                        const NeoCore::ExportMetadata& metadata) override
    {
        if (build_dir.empty() || output_path.empty()) {
            CLogger::Error("NSUM export: build_dir/output_path is empty");
            return false;
        }
        if (!pathExists(build_dir) || !isDirectory(build_dir)) {
            CLogger::Error("NSUM export: build_dir is not a directory '{}'", build_dir.c_str());
            return false;
        }

        QFileInfo outFi(QString::fromStdString(output_path));
        QDir outDir = outFi.absoluteDir();
        if (!outDir.exists() && !outDir.mkpath(".")) {
            CLogger::Error("NSUM export: cannot create output directory '{}'",
                outDir.absolutePath().toStdString().c_str());
            return false;
        }
        if (QFile::exists(QString::fromStdString(output_path))
            && !QFile::remove(QString::fromStdString(output_path))) {
            CLogger::Error("NSUM export: cannot remove existing output file '{}'",
                output_path.c_str());
            return false;
        }

        libzippp::ZipArchive zf(output_path);
        if (!zf.open(libzippp::ZipArchive::New)) {
            CLogger::Error("NSUM export: cannot create zip archive '{}'", output_path.c_str());
            return false;
        }

        try {
            json manifest = buildManifest(metadata);
            std::string manifestStr = manifest.dump(2);
            zf.addData("nsum.modpack.json", manifestStr.data(), manifestStr.size());
            scanAndPackDirectory(zf, build_dir, "overrides");

            if (zf.close() != LIBZIPPP_OK) {
                CLogger::Error("NSUM export: failed to close zip archive");
                return false;
            }
        } catch (const std::exception& e) {
            zf.close();
            CLogger::Error("NSUM export: exception during packaging: {}", e.what());
            return false;
        }

        CLogger::Info("NSUM export: created '{}'", output_path.c_str());
        return true;
    }

    nlohmann::json preview_structure(const std::string& build_dir,
                                     const NeoCore::ExportMetadata& metadata,
                                     const std::string& target_dir = "") override
    {
        (void)metadata;
        (void)target_dir;
        nlohmann::json entries = nlohmann::json::array();
        entries.push_back({{"path", "nsum.modpack.json"}, {"dir", false}, {"umd", ""}});

        std::string normBuildDir = build_dir;
        while (!normBuildDir.empty()
               && (normBuildDir.back() == '/' || normBuildDir.back() == '\\')) {
            normBuildDir.pop_back();
        }
        if (!normBuildDir.empty()
            && QFileInfo::exists(QString::fromStdString(normBuildDir))) {
            QDirIterator it(QString::fromStdString(normBuildDir),
                QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QFileInfo fi = it.fileInfo();
                std::string rel = relativePath(normBuildDir,
                    fi.absoluteFilePath().toStdString());
                if (rel.empty()) continue;
                std::replace(rel.begin(), rel.end(), '\\', '/');
                entries.push_back({
                    {"path", "overrides/" + rel},
                    {"dir", fi.isDir()},
                    {"umd", ""}
                });
            }
        }
        return entries;
    }
};

} // namespace

extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter() {
    return new NsumExporter();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoExporter_NSUM")
