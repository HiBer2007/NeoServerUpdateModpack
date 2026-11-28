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

#include <algorithm>
#include <filesystem>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

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

std::string fullPathForRel(const std::string& base, const std::string& rel) {
    std::string path = base;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') path += '/';
    path += rel;
    return path;
}

std::string extraString(const json& extra, const char* key, const std::string& fallback = "") {
    if (extra.is_object() && extra.contains(key) && extra[key].is_string()) {
        return extra[key].get<std::string>();
    }
    return fallback;
}

std::vector<std::string> splitPatterns(const std::string& raw) {
    std::vector<std::string> patterns;
    std::string current;
    for (char c : raw) {
        if (c == '\n' || c == '\r' || c == ';') {
            const size_t begin = current.find_first_not_of(" \t");
            if (begin != std::string::npos) {
                const size_t end = current.find_last_not_of(" \t");
                patterns.push_back(current.substr(begin, end - begin + 1));
            }
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    const size_t begin = current.find_first_not_of(" \t");
    if (begin != std::string::npos) {
        const size_t end = current.find_last_not_of(" \t");
        patterns.push_back(current.substr(begin, end - begin + 1));
    }
    return patterns;
}

void validatePatterns(const std::vector<std::string>& patterns) {
    for (const auto& pattern : patterns) {
        try {
            std::regex re(pattern, std::regex::ECMAScript);
            (void)re;
        } catch (const std::regex_error& error) {
            throw std::runtime_error("invalid embed pattern '" + pattern + "': "
                + error.what());
        }
    }
}

bool matchPattern(const std::vector<std::string>& patterns, const std::string& path,
                  std::string& matched) {
    for (const auto& pattern : patterns) {
        try {
            if (std::regex_search(path, std::regex(pattern, std::regex::ECMAScript))) {
                matched = pattern;
                return true;
            }
        } catch (const std::regex_error&) {
            return false;
        }
    }
    return false;
}

json collectFileIndex(const std::string& buildDir, const std::string& commit,
                      const std::string& gitBranch) {
    json files = json::array();
    std::string root = buildDir;
    while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
    if (root.empty()) return files;
    QDirIterator it(QString::fromStdString(root), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        std::string rel = relativePath(root, fi.absoluteFilePath().toStdString());
        if (rel.empty()) continue;
        std::replace(rel.begin(), rel.end(), '\\', '/');
        if (rel == "save" || rel.rfind("save/", 0) == 0) continue;
        std::string hash;
        QFile fh(fi.absoluteFilePath());
        if (fh.open(QIODevice::ReadOnly)) {
            QCryptographicHash h(QCryptographicHash::Sha256);
            h.addData(&fh);
            hash = h.result().toHex().toStdString();
            fh.close();
        }
        files.push_back({{"path", rel},
                         {"size", static_cast<long long>(fi.size())},
                         {"sha256", hash},
                         {"commit", commit},
                         {"git_branch", gitBranch}});
    }
    return files;
}

json repositoryUrls(const json& extra, const std::string& repo) {
    json urls = json::array();
    if (extra.is_object()) {
        urls = extra.value("repo_urls", json::array());
        if (urls.is_string()) {
            try {
                urls = json::parse(urls.get<std::string>());
            } catch (...) {
                urls = json::array();
            }
        }
    }
    if (!urls.is_array()) urls = json::array();
    if (urls.empty() && !repo.empty()) {
        std::string type = "ssh";
        if (repo.rfind("https://", 0) == 0) type = "https";
        else if (repo.rfind("http://", 0) == 0) type = "http";
        else if (repo.rfind("git@", 0) != 0 && repo.rfind("ssh://", 0) != 0) type = "local";
        urls.push_back({{"type", type}, {"url", repo}});
    }
    return urls;
}

json buildManifest(const NeoCore::ExportMetadata& meta, const json& files,
                   const json& embedded, const std::vector<std::string>& patterns) {
    const json& extra = meta.extra;
    const std::string commit = extraString(extra, "git_commit");
    const std::string gitBranch = extraString(extra, "git_branch");
    const std::string defaultBranch = extraString(extra, "default_branch");
    const std::string repo = extraString(extra, "repo");

    json j;
    j["pack_format"] = 2;
    j["schema"] = 2;
    j["type"] = "nsum";

    j["name"] = meta.name.empty() ? "NSUM" : meta.name;
    j["author"] = meta.author;
    j["version"] = meta.version;
    j["gameVersion"] = meta.game_version;
    j["modloader"] = meta.modloader;
    j["modloaderVersion"] = meta.modloader_version;
    j["core"] = extraString(extra, "core");
    j["description"] = meta.description.empty() ? meta.summary : meta.description;

    j["repository"] = {
        {"url", repo},
        {"urls", repositoryUrls(extra, repo)},
        {"default_branch", defaultBranch},
        {"branch", extraString(extra, "branch", "base")},
        {"git_branch", gitBranch},
        {"commit", commit}
    };
    j["instance"] = {
        {"repo", repo},
        {"branch", extraString(extra, "branch", "base")},
        {"gitBranch", gitBranch},
        {"branchType", extraString(extra, "branch_type", "universal")}
    };
    j["files"] = files;
    j["embedded"] = embedded;
    j["embed_patterns"] = patterns;
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
    std::string file_extension() const override { return ".nsumpack"; }
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
            const std::string commit = extraString(metadata.extra, "git_commit");
            const std::string gitBranch = extraString(metadata.extra, "git_branch");
            const std::vector<std::string> patterns =
                splitPatterns(extraString(metadata.extra, "embed_patterns"));
            validatePatterns(patterns);

            const json files = collectFileIndex(build_dir, commit, gitBranch);
            json embedded = json::array();
            if (!patterns.empty()) {
                for (const auto& file : files) {
                    const std::string rel = file.value("path", "");
                    std::string matched;
                    if (rel.empty() || !matchPattern(patterns, rel, matched)) continue;
                    addFileToZip(zf, fullPathForRel(build_dir, rel), "overrides/" + rel);
                    embedded.push_back({{"path", rel},
                                        {"size", file.value("size", 0LL)},
                                        {"sha256", file.value("sha256", "")},
                                        {"commit", commit},
                                        {"git_branch", gitBranch},
                                        {"pattern", matched}});
                }
            }

            json manifest = buildManifest(metadata, files, embedded, patterns);
            std::string manifestStr = manifest.dump(2);
            zf.addData("nsum.index.json", manifestStr.data(), manifestStr.size());

            if (zf.close() != LIBZIPPP_OK) {
                CLogger::Error("NSUM export: failed to close zip archive");
                return false;
            }
            CLogger::Info("NSUM export: embedded {} file(s) of {}, patterns={}",
                embedded.size(), files.size(), patterns.size());
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
        (void)target_dir;
        nlohmann::json entries = nlohmann::json::array();
        entries.push_back({{"path", "nsum.index.json"}, {"dir", false}, {"umd", ""}});

        const std::vector<std::string> patterns =
            splitPatterns(extraString(metadata.extra, "embed_patterns"));
        if (patterns.empty()) return entries;

        std::string normBuildDir = build_dir;
        while (!normBuildDir.empty()
               && (normBuildDir.back() == '/' || normBuildDir.back() == '\\')) {
            normBuildDir.pop_back();
        }
        if (!normBuildDir.empty()
            && QFileInfo::exists(QString::fromStdString(normBuildDir))) {
            QDirIterator it(QString::fromStdString(normBuildDir),
                QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QFileInfo fi = it.fileInfo();
                std::string rel = relativePath(normBuildDir,
                    fi.absoluteFilePath().toStdString());
                if (rel.empty()) continue;
                std::replace(rel.begin(), rel.end(), '\\', '/');
                std::string matched;
                if (!matchPattern(patterns, rel, matched)) continue;
                entries.push_back({
                    {"path", "overrides/" + rel},
                    {"dir", false},
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
