#include "umd_generator.h"
#include <logger.h>
#include <filesystem>
#include <map>
#include <set>
#include <QFile>
#include <QCryptographicHash>

namespace NeoBuild {

namespace fs = std::filesystem;

namespace {

void collectLayers(const std::vector<BranchLayer>& layers,
    std::map<std::string, std::pair<bool, std::string>>& buildSet)
{
    for (const auto& layer : layers) {
        std::error_code ec;
        if (!fs::is_directory(layer.baseDir, ec)) continue;

        for (auto it = fs::recursive_directory_iterator(
                 layer.baseDir, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            const fs::directory_entry& entry = *it;
            std::string rel = fs::relative(entry.path(), layer.baseDir, ec).generic_string();
            if (ec) {
                ec.clear();
                continue;
            }
            if (rel.empty()) continue;

            if (rel == "branch_manifest.json"
                || rel.rfind(".overrides/", 0) == 0) {
                if (entry.is_directory(ec)) it.disable_recursion_pending();
                continue;
            }
            if (entry.is_directory(ec)) {
                if (rel == ".NSUM" || rel == ".hmcl") {
                    it.disable_recursion_pending();
                    continue;
                }
                buildSet[rel] = { true, "" };
                continue;
            }

            auto mi = layer.manifest.markers.find(rel);
            if (mi != layer.manifest.markers.end()
                && mi->second == NeoWorkspace::FileMarker::Delete) {
                buildSet.erase(rel);
                continue;
            }
            std::string src = entry.path().string();
            if (mi != layer.manifest.markers.end()
                && mi->second == NeoWorkspace::FileMarker::Override) {
                std::string ov = layer.overridesDir + "/" + rel;
                if (fs::exists(ov)) src = ov;
            }
            buildSet[rel] = { false, src };
        }
    }
}

void scanDir(const fs::path& root, std::map<std::string, bool>& out)
{
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return;

    fs::recursive_directory_iterator it(
        root, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (it->is_directory(ec)) {
            const std::string name = it->path().filename().string();
            if (name == ".NSUM" || name == ".hmcl") {
                it.disable_recursion_pending();
                continue;
            }
        }
        ec.clear();
        std::string rel = fs::relative(it->path(), root, ec).generic_string();
        if (ec) {
            ec.clear();
            continue;
        }
        if (rel.empty()) continue;
        out[rel] = it->is_directory(ec);
        ec.clear();
    }
}

std::string sha256File(const fs::path& path)
{
    QFile f(QString::fromStdString(path.string()));
    if (!f.open(QIODevice::ReadOnly)) return {};

    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buf;
    buf.resize(65536);
    while (true) {
        qint64 n = f.read(buf.data(), buf.size());
        if (n <= 0) break;
        hash.addData(buf.constData(), static_cast<int>(n));
    }
    return QString::fromLatin1(hash.result().toHex()).toStdString();
}

bool filesEqual(const fs::path& buildPath, const fs::path& targetPath)
{
    std::error_code ec1, ec2;
    uintmax_t s1 = fs::file_size(buildPath, ec1);
    uintmax_t s2 = fs::file_size(targetPath, ec2);
    if (ec1 || ec2) return false;
    if (s1 != s2) return false;
    return sha256File(buildPath) == sha256File(targetPath);
}

} // namespace

nlohmann::json generateUmdStructure(const std::string& buildDir,
    const std::string& targetDir, NeoCore::IBuildProgress* progress,
    NeoCore::CancelToken* cancel)
{
    nlohmann::json entries = nlohmann::json::array();

    if (buildDir.empty()) {
        return entries;
    }

    std::map<std::string, bool> buildSet;
    scanDir(buildDir, buildSet);

    std::map<std::string, bool> targetSet;
    const bool compare = !targetDir.empty();
    if (compare) {
        scanDir(targetDir, targetSet);
    }

    std::set<std::string> deleted;
    if (compare) {
        for (const auto& kv : targetSet) {
            if (kv.second) continue;
            if (buildSet.find(kv.first) == buildSet.end()) {
                deleted.insert(kv.first);
            }
        }
    }

    const int total = static_cast<int>(buildSet.size() + deleted.size());
    int idx = 0;

    for (const auto& kv : buildSet) {
        if (cancel && cancel->is_cancelled()) break;

        const std::string& rel = kv.first;
        const bool isDir = kv.second;
        std::string umd;

        if (compare && !isDir) {
            auto it = targetSet.find(rel);
            if (it == targetSet.end()) {
                umd = "U";
            } else if (it->second) {
                umd = "M";
            } else if (!filesEqual(fs::path(buildDir) / rel, fs::path(targetDir) / rel)) {
                umd = "M";
            }
        }

        entries.push_back({
            {"path", rel},
            {"dir", isDir},
            {"umd", umd}
        });

        if (progress) {
            progress->set_main_progress(total > 0 ? idx * 100 / total : 0);
        }
        ++idx;
    }

    for (const auto& rel : deleted) {
        if (cancel && cancel->is_cancelled()) break;
        entries.push_back({
            {"path", rel},
            {"dir", false},
            {"umd", "D"}
        });
        if (progress) {
            progress->set_main_progress(total > 0 ? idx * 100 / total : 0);
        }
        ++idx;
    }

    if (progress) {
        progress->set_main_stage("preview");
        progress->set_main_progress(100);
        progress->set_main_message("UMD preview generated");
        progress->log("UMD preview: " + std::to_string(buildSet.size())
            + " build entries, " + std::to_string(deleted.size()) + " to delete");
    }

    return entries;
}

nlohmann::json generateUmdStructureFromLayers(const std::vector<BranchLayer>& layers,
    const std::string& targetDir, NeoCore::IBuildProgress* progress,
    NeoCore::CancelToken* cancel)
{
    nlohmann::json entries = nlohmann::json::array();
    if (layers.empty()) return entries;

    std::map<std::string, std::pair<bool, std::string>> rawSet;
    collectLayers(layers, rawSet);

    std::map<std::string, std::pair<bool, std::string>> buildSet;
    for (const auto& kv : rawSet) {
        buildSet[kv.first] = kv.second;
        std::string p = kv.first;
        while (true) {
            auto pos = p.rfind('/');
            if (pos == std::string::npos) break;
            p = p.substr(0, pos);
            if (p.empty()) break;
            if (buildSet.find(p) == buildSet.end()) {
                buildSet[p] = { true, "" };
            }
        }
    }

    std::map<std::string, bool> targetSet;
    const bool compare = !targetDir.empty();
    if (compare) {
        scanDir(targetDir, targetSet);
    }

    std::set<std::string> deleted;
    if (compare) {
        for (const auto& kv : targetSet) {
            if (kv.second) continue;
            if (buildSet.find(kv.first) == buildSet.end()) {
                deleted.insert(kv.first);
            }
        }
    }

    const int total = static_cast<int>(buildSet.size() + deleted.size());
    int idx = 0;

    for (const auto& kv : buildSet) {
        if (cancel && cancel->is_cancelled()) break;

        const std::string& rel = kv.first;
        const bool isDir = kv.second.first;
        std::string umd;

        if (compare && !isDir) {
            auto it = targetSet.find(rel);
            if (it == targetSet.end()) {
                umd = "U";
            } else if (it->second) {
                umd = "M";
            } else if (!kv.second.second.empty()
                && !filesEqual(fs::path(kv.second.second),
                    fs::path(targetDir) / rel)) {
                umd = "M";
            }
        }

        entries.push_back({
            {"path", rel},
            {"dir", isDir},
            {"umd", umd}
        });

        if (progress) {
            progress->set_main_progress(total > 0 ? idx * 100 / total : 0);
        }
        ++idx;
    }

    for (const auto& rel : deleted) {
        if (cancel && cancel->is_cancelled()) break;
        entries.push_back({
            {"path", rel},
            {"dir", false},
            {"umd", "D"}
        });
        if (progress) {
            progress->set_main_progress(total > 0 ? idx * 100 / total : 0);
        }
        ++idx;
    }

    if (progress) {
        progress->set_main_stage("preview");
        progress->set_main_progress(100);
        progress->set_main_message("UMD preview generated (layers)");
        progress->log("UMD preview (layers): " + std::to_string(buildSet.size())
            + " build entries, " + std::to_string(deleted.size()) + " to delete");
    }

    return entries;
}

} // namespace NeoBuild
