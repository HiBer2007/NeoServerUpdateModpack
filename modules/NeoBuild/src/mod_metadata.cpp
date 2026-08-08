#include "mod_metadata.h"
#include <libzippp/libzippp.h>
#include <toml++/toml.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <logger.h>

namespace NeoBuild {

namespace {

std::vector<std::string> modIdsFromTomlMods(const std::string& content)
{
    std::vector<std::string> ids;

    try {
        toml::table root = toml::parse(content);

        if (const toml::array* mods = root["mods"].as_array()) {
            for (const toml::node& node : *mods) {
                const toml::table* mod = node.as_table();
                if (!mod) continue;
                if (const toml::node* modIdNode = mod->get("modId")) {
                    if (auto modId = modIdNode->value<std::string>()) {
                        std::string id = std::move(*modId);
                        if (!id.empty()) {
                            ids.push_back(std::move(id));
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        CLogger::Debug("mod_metadata: toml parse exception: {}", e.what());
    }

    return ids;
}

std::vector<std::string> modIdsFromFabricJson(const std::string& content)
{
    std::vector<std::string> ids;

    try {
        auto j = nlohmann::json::parse(content);
        if (j.contains("id") && j["id"].is_string()) {
            std::string id = j["id"].get<std::string>();
            if (!id.empty()) {
                ids.push_back(std::move(id));
            }
        }
    }
    catch (const std::exception& e) {
        CLogger::Debug("mod_metadata: fabric.mod.json parse exception: {}", e.what());
    }

    return ids;
}

std::vector<std::string> modIdsFromMcmodInfo(const std::string& content)
{
    std::vector<std::string> ids;

    try {
        auto j = nlohmann::json::parse(content);
        if (!j.is_array()) return ids;

        for (const auto& entry : j) {
            if (entry.contains("modid") && entry["modid"].is_string()) {
                std::string id = entry["modid"].get<std::string>();
                if (!id.empty()) {
                    ids.push_back(std::move(id));
                }
            }
        }
    }
    catch (const std::exception& e) {
        CLogger::Debug("mod_metadata: mcmod.info parse exception: {}", e.what());
    }

    return ids;
}

std::string readZipEntry(libzippp::ZipArchive& archive, const char* name)
{
    libzippp::ZipEntry entry = archive.getEntry(name);
    if (entry.isNull()) return "";
    return entry.readAsText();
}

} // namespace

std::vector<std::string> extractModIds(const std::string& jarPath)
{
    std::vector<std::string> ids;

    libzippp::ZipArchive archive(jarPath);
    if (!archive.open(libzippp::ZipArchive::ReadOnly)) {
        CLogger::Warn("mod_metadata: cannot open jar: {}", jarPath);
        return ids;
    }

    static const char* tomlCandidates[] = {
        "META-INF/neoforge.mods.toml",
        "META-INF/mods.toml"
    };

    for (const char* name : tomlCandidates) {
        std::string content = readZipEntry(archive, name);
        if (content.empty()) continue;
        ids = modIdsFromTomlMods(content);
        if (!ids.empty()) break;
        ids.clear();
    }

    if (ids.empty()) {
        std::string fabricJson = readZipEntry(archive, "fabric.mod.json");
        if (!fabricJson.empty()) {
            ids = modIdsFromFabricJson(fabricJson);
        }
    }

    if (ids.empty()) {
        std::string mcmodInfo = readZipEntry(archive, "mcmod.info");
        if (!mcmodInfo.empty()) {
            ids = modIdsFromMcmodInfo(mcmodInfo);
        }
    }

    archive.close();

    std::vector<std::string> unique;
    for (const auto& id : ids) {
        if (std::find(unique.begin(), unique.end(), id) == unique.end()) {
            unique.push_back(id);
        }
    }

    return unique;
}

} // namespace NeoBuild
