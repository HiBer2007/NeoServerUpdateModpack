#pragma once

#include <string>
#include <vector>

namespace NeoWorkspace {

enum class RepoType {
    Remote = 0,
    Local = 1,
    Cache = 2
};

struct RecentRepo {
    RepoType type = RepoType::Remote;
    std::string location;
    std::string cachePath;
};

class HistoryStore {
public:
    static constexpr int MaxRecentRepos = 10;

    static std::string historyDir();
    static std::string historyPath();
    static std::string recentCacheDir();

    static std::vector<RecentRepo> readRecentRepos();
    static void saveRecentRepo(const std::string& location,
        RepoType type = RepoType::Remote,
        const std::string& cachePath = "");
};

} // namespace NeoWorkspace
