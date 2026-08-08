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
#include <QDateTime>

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
        throw std::runtime_error(
            "Failed to add file '" + entryName + "' to zip");
    }
}

void addEmptyDirToZip(libzippp::ZipArchive& zf,
                      const std::string& entryName) {
    std::string e = entryName;
    if (e.empty()) return;
    if (e.back() != '/') e += '/';
    if (!zf.addEntry(e)) {
        throw std::runtime_error(
            "Failed to add directory entry '" + e + "' to zip");
    }
}

std::string fullPathForRel(const std::string& base, const std::string& rel) {
    std::string path = base;
    if (path.back() != '/' && path.back() != '\\') path += '/';
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

    QDirIterator it(
        QString::fromStdString(normBuildDir),
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);

    std::vector<std::string> emptyDirs;

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        std::string fullPath = fi.absoluteFilePath().toStdString();
        std::string rel = relativePath(normBuildDir, fullPath);

        if (rel.empty()) continue;

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
        QDirIterator childCheck(
            QString::fromStdString(checkPath),
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        if (!childCheck.hasNext()) {
            std::string entryName = overridePrefix + "/" + d;
            std::replace(entryName.begin(), entryName.end(), '\\', '/');
            addEmptyDirToZip(zf, entryName);
        }
    }
}

json buildManifest(const NeoCore::ExportMetadata& meta) {
    json j;
    j["manifestType"] = "minecraftModpack";
    j["manifestVersion"] = 1;
    j["name"] = meta.name;
    j["version"] = meta.version;
    j["author"] = meta.author;
    j["overrides"] = "overrides";

    json mc;
    mc["version"] = meta.game_version;
    json ml;
    ml["id"] = meta.modloader + "-" + meta.modloader_version;
    ml["primary"] = true;
    mc["modLoaders"] = json::array({ml});
    j["minecraft"] = mc;

    j["files"] = json::array();

    if (!meta.summary.empty()) {
        j["description"] = meta.summary;
    }
    if (!meta.description.empty()) {
        j["description"] = meta.description;
    }

    return j;
}

json buildPackMeta(const NeoCore::ExportMetadata& meta) {
    json pm;
    pm["pack"] = meta.name;
    pm["version"] = meta.version;
    pm["author"] = meta.author;
    pm["export_time"] = QDateTime::currentDateTime()
        .toString(Qt::ISODate).toStdString();
    return pm;
}

std::string jsonDump(const json& j) {
    return j.dump(2);
}

class McbbsExporter : public NeoCore::IModpackExporter {
public:
    std::string format_name() const override { return "mcbbs"; }
    std::string file_extension() const override { return ".zip"; }
    std::string format_description() const override {
        return "MCBBS / PCL / HMCL 通用整合包格式";
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
        return engine.build(target.branch, progress, cancel);
    }

    bool export_modpack(
        const std::string& build_dir,
        const std::string& output_path,
        const NeoCore::ExportMetadata& metadata) override
    {
        try {
            if (build_dir.empty()) {
                CLogger::Error(
                    "MCBBS export: build_dir is empty");
                return false;
            }
            if (output_path.empty()) {
                CLogger::Error(
                    "MCBBS export: output_path is empty");
                return false;
            }
            if (!pathExists(build_dir)) {
                CLogger::Error(
                    "MCBBS export: build_dir does not exist '{}'",
                    build_dir.c_str());
                return false;
            }
            if (!isDirectory(build_dir)) {
                CLogger::Error(
                    "MCBBS export: build_dir is not a directory '{}'",
                    build_dir.c_str());
                return false;
            }

            QFileInfo outFi(QString::fromStdString(output_path));
            QDir outDir = outFi.absoluteDir();
            if (!outDir.exists()) {
                if (!outDir.mkpath(".")) {
                    CLogger::Error(
                        "MCBBS export: cannot create output directory '{}'",
                        outDir.absolutePath().toStdString().c_str());
                    return false;
                }
            }

            if (QFile::exists(QString::fromStdString(output_path))) {
                if (!QFile::remove(QString::fromStdString(output_path))) {
                    CLogger::Error(
                        "MCBBS export: cannot remove existing output file '{}'",
                        output_path.c_str());
                    return false;
                }
            }

            libzippp::ZipArchive zf(output_path);
            if (!zf.open(libzippp::ZipArchive::New)) {
                CLogger::Error(
                    "MCBBS export: cannot create zip archive '{}'",
                    output_path.c_str());
                return false;
            }

            try {
                {
                    json manifest = buildManifest(metadata);
                    std::string manifestStr = jsonDump(manifest);
                    zf.addData("manifest.json", manifestStr.data(), manifestStr.size());
                }

                {
                    json packmeta = buildPackMeta(metadata);
                    std::string packmetaStr = jsonDump(packmeta);
                    zf.addData("mcbbs.packmeta", packmetaStr.data(), packmetaStr.size());
                }

                scanAndPackDirectory(zf, build_dir, "overrides");

                if (zf.close() != LIBZIPPP_OK) {
                    CLogger::Error(
                        "MCBBS export: failed to close zip archive");
                    return false;
                }
            } catch (const std::exception& e) {
                zf.close();
                CLogger::Error(
                    "MCBBS export: exception during packaging: {}",
                    e.what());
                return false;
            } catch (...) {
                zf.close();
                CLogger::Error(
                    "MCBBS export: unknown exception during packaging");
                return false;
            }

            CLogger::Info(
                "MCBBS export: successfully created '{}'", output_path.c_str());
            return true;

        } catch (const std::exception& e) {
            CLogger::Error(
                "MCBBS export: unexpected exception: {}", e.what());
            return false;
        } catch (...) {
            CLogger::Error(
                "MCBBS export: unknown unexpected exception");
            return false;
        }
    }

    nlohmann::json preview_structure(
        const std::string& build_dir,
        const NeoCore::ExportMetadata& metadata,
        const std::string& target_dir = "") override
    {
        (void)target_dir;
        nlohmann::json entries = nlohmann::json::array();
        entries.push_back({ {"path", "manifest.json"}, {"dir", false}, {"umd", ""} });
        entries.push_back({ {"path", "mcbbs.packmeta"}, {"dir", false}, {"umd", ""} });

        std::string normBuildDir = build_dir;
        while (!normBuildDir.empty() &&
               (normBuildDir.back() == '/' || normBuildDir.back() == '\\')) {
            normBuildDir.pop_back();
        }

        if (!normBuildDir.empty() && QFileInfo::exists(
                QString::fromStdString(normBuildDir))) {
            QDirIterator it(
                QString::fromStdString(normBuildDir),
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

} // anonymous namespace

extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter() {
    return new McbbsExporter();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoExporter_MCBBS")

