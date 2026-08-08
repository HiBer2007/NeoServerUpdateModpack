#include "serverconfig_sync.h"
#include <PluginLoader.h>
#include <logger.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>

namespace NeoBuild {

namespace fs = std::filesystem;

namespace {

std::string trim(const std::string& s)
{
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

} // namespace

ServerConfigSync::ServerConfigSync() = default;

bool ServerConfigSync::init(const std::string& savesDir,
    const std::string& repoRoot,
    const std::string& branchName)
{
    savesDir_ = savesDir;
    repoRoot_ = repoRoot;
    branchName_ = branchName;

    fs::path ruleBase = fs::path(repoRoot_) / "branches" / branchName_
        / "[save]" / "serverconfig" / ".rule";
    ruleDir_ = ruleBase.string();

    if (branchName_.empty()) {
        CLogger::Warn("ServerConfigSync: empty branch name, rules disabled");
        ruleDir_.clear();
    }

    loadRules();

    CLogger::Info("ServerConfigSync: initialized saves={} rules={}",
        savesDir_, ruleDir_.empty() ? "(none)" : ruleDir_);
    return true;
}

bool ServerConfigSync::hasRules() const
{
    return !ruleDir_.empty() && fs::exists(ruleDir_);
}

void ServerConfigSync::loadRules()
{
    defaultMode_ = ServerConfigMode::Overwrite;
    fileModes_.clear();

    if (ruleDir_.empty() || !fs::exists(ruleDir_)) {
        return;
    }

    auto parseMode = [](const std::string& m, ServerConfigMode fallback) -> ServerConfigMode {
        if (m == "overwrite" || m == "full") return ServerConfigMode::Overwrite;
        if (m == "partial" || m == "merge") return ServerConfigMode::Partial;
        if (m == "ignore") return ServerConfigMode::Ignore;
        return fallback;
    };

    try {
        fs::path globlePath = fs::path(ruleDir_) / "globle.json";
        if (fs::exists(globlePath)) {
            std::ifstream f(globlePath);
            nlohmann::json j = nlohmann::json::parse(f, nullptr, false);
            if (j.is_object() && j.contains("default_mode") && j["default_mode"].is_string()) {
                defaultMode_ = parseMode(j["default_mode"].get<std::string>(),
                    ServerConfigMode::Overwrite);
            }
        }

        fs::path listPath = fs::path(ruleDir_) / "list.json";
        if (fs::exists(listPath)) {
            std::ifstream f(listPath);
            nlohmann::json j = nlohmann::json::parse(f, nullptr, false);
            if (j.is_object() && j.contains("files") && j["files"].is_object()) {
                for (const auto& [path, modeVal] : j["files"].items()) {
                    if (!modeVal.is_string()) continue;
                    std::string rel = path;
                    std::replace(rel.begin(), rel.end(), '\\', '/');
                    fileModes_[rel] = parseMode(modeVal.get<std::string>(),
                        ServerConfigMode::Overwrite);
                }
            }
        }

        CLogger::Info("ServerConfigSync: loaded rules: default={} listed={}",
            static_cast<int>(defaultMode_), fileModes_.size());
    } catch (const std::exception& e) {
        CLogger::Error("ServerConfigSync::loadRules exception: {}", e.what());
    }
}

ServerConfigMode ServerConfigSync::modeFor(const std::string& relPath) const
{
    auto it = fileModes_.find(relPath);
    if (it != fileModes_.end()) return it->second;
    return defaultMode_;
}

std::string ServerConfigSync::sourcePathFor(const std::string& relPath) const
{
    if (ruleDir_.empty()) return "";
    fs::path src = fs::path(repoRoot_) / "branches" / branchName_
        / "[save]" / "serverconfig" / relPath;
    return src.string();
}

std::vector<ServerConfigEntry> ServerConfigSync::scanServerConfigs()
{
    entries_.clear();
    std::vector<ServerConfigEntry> result;

    try {
        if (!fs::exists(savesDir_) || !fs::is_directory(savesDir_)) {
            return result;
        }

        for (const auto& worldEntry : fs::directory_iterator(savesDir_,
            fs::directory_options::skip_permission_denied)) {

            if (!worldEntry.is_directory()) continue;

            std::string worldName = worldEntry.path().filename().string();
            fs::path serverConfigDir = worldEntry.path() / "serverconfig";

            if (!fs::exists(serverConfigDir) || !fs::is_directory(serverConfigDir)) {
                continue;
            }

            for (const auto& configEntry : fs::recursive_directory_iterator(
                serverConfigDir, fs::directory_options::skip_permission_denied)) {

                if (!configEntry.is_regular_file()) continue;

                ServerConfigEntry entry;
                entry.worldName = worldName;
                entry.configPath = configEntry.path().string();
                entry.relativePath = fs::relative(configEntry.path(), serverConfigDir)
                    .generic_string();

                result.push_back(entry);
            }
        }

        entries_ = result;

        CLogger::Info("ServerConfigSync: scanned {} configs across worlds",
            result.size());
    } catch (const std::exception& e) {
        CLogger::Error("ServerConfigSync::scanServerConfigs exception: {}", e.what());
    }

    return result;
}

bool ServerConfigSync::syncConfig(const ServerConfigEntry& entry,
    NeoCore::CancelToken* cancelToken)
{
    try {
        if (cancelToken && cancelToken->is_cancelled()) return false;

        if (ruleDir_.empty() || !fs::exists(ruleDir_)) {
            CLogger::Debug("ServerConfigSync: no rules, skipping {} -> {}",
                entry.worldName, entry.relativePath);
            return true;
        }

        std::string sourcePath = sourcePathFor(entry.relativePath);
        if (!fs::exists(sourcePath)) {
            CLogger::Info("ServerConfigSync: no repo version for {} -> {}",
                entry.worldName, entry.relativePath);
            return true;
        }

        if (cancelToken && cancelToken->is_cancelled()) return false;

        std::string remoteContent = readFile(sourcePath);

        ServerConfigMode mode = modeFor(entry.relativePath);
        if (mode == ServerConfigMode::Ignore) {
            ++skipped_;
            CLogger::Debug("ServerConfigSync: ignored: {}", entry.relativePath);
            return true;
        }

        std::string localContent = readFile(entry.configPath);
        if (remoteContent == localContent) {
            ++skipped_;
            CLogger::Debug("ServerConfigSync: identical, skipping: {}",
                entry.relativePath);
            return true;
        }

        bool ok = false;
        if (mode == ServerConfigMode::Partial) {
            ok = syncPartial(entry, remoteContent);
        } else {
            ok = syncOverwrite(entry, remoteContent);
        }

        if (ok) {
            ++synced_;
            CLogger::Info("ServerConfigSync: {} '{}' for world {}",
                mode == ServerConfigMode::Partial ? "merged" : "updated",
                entry.relativePath, entry.worldName);
        } else {
            ++failed_;
            CLogger::Warn("ServerConfigSync: sync failed: {} -> {}",
                entry.configPath, entry.relativePath);
        }
        return ok;
    } catch (const std::exception& e) {
        CLogger::Error("ServerConfigSync::syncConfig exception: {}", e.what());
        return false;
    }
}

bool ServerConfigSync::syncOverwrite(const ServerConfigEntry& entry,
    const std::string& remoteContent)
{
    if (remoteContent.empty() && fs::exists(entry.configPath)) {
        CLogger::Info("ServerConfigSync: empty repo version for {}",
            entry.relativePath);
        return true;
    }

    std::error_code ec;
    fs::path backupPath = entry.configPath + ".backup";
    fs::copy_file(entry.configPath, backupPath,
        fs::copy_options::overwrite_existing, ec);
    if (ec) {
        CLogger::Warn("ServerConfigSync: cannot create backup for {}",
            entry.relativePath);
    }

    if (writeFile(entry.configPath, remoteContent)) {
        if (fs::exists(backupPath)) {
            fs::remove(backupPath, ec);
        }
        return true;
    } else {
        if (fs::exists(backupPath)) {
            std::error_code rec;
            fs::copy_file(backupPath, entry.configPath,
                fs::copy_options::overwrite_existing, rec);
            fs::remove(backupPath, rec);
        }
        return false;
    }
}

bool ServerConfigSync::syncPartial(const ServerConfigEntry& entry,
    const std::string& remoteContent)
{
    std::string sourcePath = sourcePathFor(entry.relativePath);
    std::string localContent = readFile(entry.configPath);

    NeoCore::PluginLoader loader;
    std::string parsersDir = (fs::path(
        QCoreApplication::applicationDirPath().toStdString()) / "parsers").string();
    if (fs::exists(parsersDir)) {
        loader.ScanDirectory(parsersDir);
    }

    NeoCore::IConfigParser* parser = loader.FindParser(sourcePath);
    if (!parser) {
        CLogger::Warn(
            "ServerConfigSync: no parser for '{}', falling back to full overwrite",
            entry.relativePath);
        return syncOverwrite(entry, remoteContent);
    }

    std::vector<std::string> trackedKeys = parser->list_keys(sourcePath);
    std::string merged = parser->merge_entries(
        sourcePath, trackedKeys, remoteContent, localContent);

    if (merged == localContent) {
        ++skipped_;
        CLogger::Debug("ServerConfigSync: partial merge identical: {}",
            entry.relativePath);
        return true;
    }

    return writeFile(entry.configPath, merged);
}

bool ServerConfigSync::syncAll(NeoCore::CancelToken* cancelToken)
{
    bool allOk = true;

    if (entries_.empty()) {
        entries_ = scanServerConfigs();
    }

    for (const auto& entry : entries_) {
        if (cancelToken && cancelToken->is_cancelled()) return false;

        if (!syncConfig(entry, cancelToken)) {
            allOk = false;
            CLogger::Warn("ServerConfigSync: sync failed for {} in world {}",
                entry.relativePath, entry.worldName);
        }
    }

    return allOk;
}

std::string ServerConfigSync::readFile(const std::string& filepath) const
{
    try {
        QFile f(QString::fromStdString(filepath));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return "";
        }
        QByteArray data = f.readAll();
        f.close();
        return data.toStdString();
    } catch (const std::exception& e) {
        CLogger::Error("ServerConfigSync::readFile exception: {}", e.what());
        return "";
    }
}

bool ServerConfigSync::writeFile(const std::string& filepath,
    const std::string& content) const
{
    try {
        fs::create_directories(fs::path(filepath).parent_path());

        QFile f(QString::fromStdString(filepath));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            CLogger::Error("ServerConfigSync: cannot open file for writing: {}", filepath);
            return false;
        }
        f.write(content.c_str(), static_cast<qint64>(content.size()));
        f.close();
        return true;
    } catch (const std::exception& e) {
        CLogger::Error("ServerConfigSync::writeFile exception: {}", e.what());
        return false;
    }
}

} // namespace NeoBuild
