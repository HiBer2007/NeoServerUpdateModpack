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
#include <QCryptographicHash>
#include <QIODevice>
#include <QDateTime>

#include <filesystem>

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace {

constexpr qint64 kHashBufferSize = 65536;

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

struct FileHashResult {
    std::string sha1;
    std::string sha512;
    qint64 fileSize;
    bool valid;
};

FileHashResult computeHashes(const std::string& filePath) {
    FileHashResult result;
    result.valid = false;
    result.fileSize = 0;

    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::ReadOnly)) return result;

    QCryptographicHash sha1Hasher(QCryptographicHash::Sha1);
    QCryptographicHash sha512Hasher(QCryptographicHash::Sha512);
    char buffer[kHashBufferSize];
    qint64 totalBytes = 0;
    qint64 bytesRead;
    while ((bytesRead = file.read(buffer, kHashBufferSize)) > 0) {
        sha1Hasher.addData(buffer, bytesRead);
        sha512Hasher.addData(buffer, bytesRead);
        totalBytes += bytesRead;
    }
    file.close();

    result.sha1 = QString::fromLatin1(
        sha1Hasher.result().toHex()).toLower().toStdString();
    result.sha512 = QString::fromLatin1(
        sha512Hasher.result().toHex()).toLower().toStdString();
    result.fileSize = totalBytes;
    result.valid = true;
    return result;
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

std::string normalizeSep(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

std::string mapModloaderId(const std::string& modloader) {
    if (modloader == "fabric") return "fabric-loader";
    if (modloader == "quilt") return "quilt-loader";
    if (modloader == "forge") return "forge";
    if (modloader == "neoforge") return "neoforge";
    return modloader;
}

json scanBuildDirForIndex(const std::string& buildDir,
                           const std::string& overridePrefix) {
    std::string normBuildDir = buildDir;
    while (!normBuildDir.empty() &&
           (normBuildDir.back() == '/' || normBuildDir.back() == '\\')) {
        normBuildDir.pop_back();
    }

    json filesArray = json::array();

    QDirIterator it(
        QString::fromStdString(normBuildDir),
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();

        if (fi.isDir()) continue;

        if (!fi.isFile()) continue;

        std::string fullPath = fi.absoluteFilePath().toStdString();
        std::string rel = relativePath(normBuildDir, fullPath);
        if (rel.empty()) continue;

        rel = normalizeSep(rel);

        FileHashResult hashes = computeHashes(fullPath);
        if (!hashes.valid) {
            CLogger::Warn(
                "Modrinth export: cannot hash file '{}', skipping",
                fullPath.c_str());
            continue;
        }

        json fileEntry;
        fileEntry["path"] = rel;

        json hashObj;
        hashObj["sha1"] = hashes.sha1;
        hashObj["sha512"] = hashes.sha512;
        fileEntry["hashes"] = hashObj;

        fileEntry["downloads"] = json::array();
        fileEntry["fileSize"] = hashes.fileSize;

        filesArray.push_back(fileEntry);
    }

    return filesArray;
}

void packOverridesToZip(libzippp::ZipArchive& zf,
                         const std::string& buildDir,
                         const std::string& overridePrefix) {
    std::string normBuildDir = buildDir;
    while (!normBuildDir.empty() &&
           (normBuildDir.back() == '/' || normBuildDir.back() == '\\')) {
        normBuildDir.pop_back();
    }

    QStringList dirsWithFiles;

    QDirIterator it(
        QString::fromStdString(normBuildDir),
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        std::string fullPath = fi.absoluteFilePath().toStdString();
        std::string rel = relativePath(normBuildDir, fullPath);
        if (rel.empty()) continue;
        rel = normalizeSep(rel);

        if (fi.isDir()) continue;
        if (fi.isFile()) {
            std::string entryName = overridePrefix + "/" + rel;
            addFileToZip(zf, fullPath, entryName);
        }
    }
}

json buildModrinthIndex(const NeoCore::ExportMetadata& meta,
                         json& filesArray) {
    json index;
    index["formatVersion"] = 1;
    index["game"] = "minecraft";
    index["versionId"] = meta.version;
    index["name"] = meta.name;
    index["summary"] = meta.summary.empty() ? meta.description : meta.summary;

    index["files"] = filesArray;

    json deps;
    deps["minecraft"] = meta.game_version;

    std::string loaderId = mapModloaderId(meta.modloader);
    if (!loaderId.empty() && !meta.modloader_version.empty()) {
        deps[loaderId] = meta.modloader_version;
    }

    index["dependencies"] = deps;

    return index;
}

class ModrinthExporter : public NeoCore::IModpackExporter {
public:
    std::string format_name() const override { return "modrinth"; }
    std::string file_extension() const override { return ".mrpack"; }
    std::string format_description() const override {
        return "Modrinth Modpack Format (.mrpack)";
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
                    "Modrinth export: build_dir is empty");
                return false;
            }
            if (output_path.empty()) {
                CLogger::Error(
                    "Modrinth export: output_path is empty");
                return false;
            }
            if (!pathExists(build_dir)) {
                CLogger::Error(
                    "Modrinth export: build_dir does not exist '{}'",
                    build_dir.c_str());
                return false;
            }
            if (!isDirectory(build_dir)) {
                CLogger::Error(
                    "Modrinth export: build_dir is not a directory '{}'",
                    build_dir.c_str());
                return false;
            }

            QFileInfo outFi(QString::fromStdString(output_path));
            QDir outDir = outFi.absoluteDir();
            if (!outDir.exists()) {
                if (!outDir.mkpath(".")) {
                    CLogger::Error(
                        "Modrinth export: cannot create output directory '{}'",
                        outDir.absolutePath().toStdString().c_str());
                    return false;
                }
            }

            if (QFile::exists(QString::fromStdString(output_path))) {
                if (!QFile::remove(QString::fromStdString(output_path))) {
                    CLogger::Error(
                        "Modrinth export: cannot remove existing output "
                        "file '{}'", output_path.c_str());
                    return false;
                }
            }

            json filesArray = scanBuildDirForIndex(build_dir, "overrides");

            libzippp::ZipArchive zf(output_path);
            if (!zf.open(libzippp::ZipArchive::New)) {
                CLogger::Error(
                    "Modrinth export: cannot create zip archive '{}'",
                    output_path.c_str());
                return false;
            }

            try {
                json index = buildModrinthIndex(metadata, filesArray);
                std::string indexStr = index.dump(2);
                zf.addData("modrinth.index.json", indexStr.data(), indexStr.size());

                packOverridesToZip(zf, build_dir, "overrides");

                if (zf.close() != LIBZIPPP_OK) {
                    CLogger::Error(
                        "Modrinth export: failed to close zip archive");
                    return false;
                }
            } catch (const std::exception& e) {
                zf.close();
                CLogger::Error(
                    "Modrinth export: exception during packaging: {}",
                    e.what());
                return false;
            } catch (...) {
                zf.close();
                CLogger::Error(
                    "Modrinth export: unknown exception during packaging");
                return false;
            }

            CLogger::Info(
                "Modrinth export: successfully created '{}'",
                output_path.c_str());
            return true;

        } catch (const std::exception& e) {
            CLogger::Error(
                "Modrinth export: unexpected exception: {}", e.what());
            return false;
        } catch (...) {
            CLogger::Error(
                "Modrinth export: unknown unexpected exception");
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
        entries.push_back({ {"path", "modrinth.index.json"}, {"dir", false}, {"umd", ""} });

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
                rel = normalizeSep(rel);
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
    return new ModrinthExporter();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoExporter_Modrinth")

