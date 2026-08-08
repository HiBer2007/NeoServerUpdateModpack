#include "history_store.h"

#include <QCoreApplication>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <algorithm>

namespace NeoWorkspace {

namespace fs = std::filesystem;
using json = nlohmann::json;

static fs::path historyDirPath()
{
    return fs::path(QCoreApplication::applicationDirPath().toStdString())
        / "config" / "history";
}

std::string HistoryStore::historyDir()
{
    return historyDirPath().string();
}

std::string HistoryStore::historyPath()
{
    return (historyDirPath() / "main.json").string();
}

std::string HistoryStore::recentCacheDir()
{
    return (historyDirPath() / "cache").string();
}

static std::string trim(const std::string& s)
{
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<RecentRepo> HistoryStore::readRecentRepos()
{
    std::vector<RecentRepo> result;
    std::ifstream f(historyPath());
    if (!f.is_open()) return result;

    json arr;
    try {
        arr = json::parse(f);
    } catch (const std::exception&) {
        return result;
    }
    if (!arr.is_array()) return result;

    for (const auto& v : arr) {
        if (!v.is_object()) continue;
        RecentRepo e;
        e.type = static_cast<RepoType>(
            v.value("type", static_cast<int>(RepoType::Remote)));
        e.location = v.value("location", "");
        e.cachePath = v.value("cache_path", "");
        if (e.location.empty()) continue;
        result.push_back(std::move(e));
    }
    return result;
}

void HistoryStore::saveRecentRepo(const std::string& location,
    RepoType type, const std::string& cachePath)
{
    std::string qurl = trim(location);
    if (qurl.empty()) return;

    auto entries = readRecentRepos();
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
            [&qurl](const RecentRepo& old) { return old.location == qurl; }),
        entries.end());

    RecentRepo e;
    e.type = type;
    e.location = qurl;
    e.cachePath = cachePath;
    entries.insert(entries.begin(), std::move(e));
    if (entries.size() > static_cast<size_t>(MaxRecentRepos)) {
        entries.resize(static_cast<size_t>(MaxRecentRepos));
    }

    std::error_code ec;
    fs::create_directories(historyDirPath(), ec);

    json arr = json::array();
    for (const auto& entry : entries) {
        json obj;
        obj["type"] = static_cast<int>(entry.type);
        obj["location"] = entry.location;
        if (!entry.cachePath.empty()) {
            obj["cache_path"] = entry.cachePath;
        }
        arr.push_back(std::move(obj));
    }

    std::ofstream f(historyPath(), std::ios::trunc);
    f << arr.dump(2);
    f.close();
}

} // namespace NeoWorkspace
