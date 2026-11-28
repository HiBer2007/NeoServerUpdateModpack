#include <repo_urls.h>

#include <algorithm>
#include <cctype>

namespace NeoWorkspace {
namespace {

struct RemoteParts {
    bool remote = false;
    std::string host;
    std::string path;
};

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.size() >= prefix.size()
        && value.compare(0, prefix.size(), prefix) == 0;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string stripGitSuffix(std::string path)
{
    while (!path.empty() && (path.back() == '/' || path.back() == '\\')) path.pop_back();
    if (path.size() > 4) {
        const std::string tail = lower(path.substr(path.size() - 4));
        if (tail == ".git") path.resize(path.size() - 4);
    }
    while (!path.empty() && (path.front() == '/' || path.front() == '\\')) path.erase(path.begin());
    return path;
}

RemoteParts parseRemote(const std::string& raw)
{
    RemoteParts parts;
    std::string url = raw;
    while (!url.empty() && std::isspace(static_cast<unsigned char>(url.front()))) url.erase(url.begin());
    while (!url.empty() && std::isspace(static_cast<unsigned char>(url.back()))) url.pop_back();
    if (url.empty()) return parts;

    if (url.rfind("ssh://", 0) == 0) {
        std::string rest = url.substr(6);
        const size_t at = rest.find('@');
        if (at != std::string::npos) rest = rest.substr(at + 1);
        const size_t slash = rest.find('/');
        if (slash == std::string::npos) return parts;
        parts.host = rest.substr(0, slash);
        parts.path = stripGitSuffix(rest.substr(slash + 1));
        parts.remote = !parts.host.empty() && !parts.path.empty();
        return parts;
    }

    if (url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0) {
        const size_t schemeEnd = url.find("://");
        std::string rest = url.substr(schemeEnd + 3);
        const size_t slash = rest.find('/');
        if (slash == std::string::npos) return parts;
        parts.host = rest.substr(0, slash);
        parts.path = stripGitSuffix(rest.substr(slash + 1));
        const size_t at = parts.host.find('@');
        if (at != std::string::npos) parts.host = parts.host.substr(at + 1);
        parts.remote = !parts.host.empty() && !parts.path.empty();
        return parts;
    }

    if (url.find('@') != std::string::npos && url.find(':') != std::string::npos
        && url.find("://") == std::string::npos) {
        const size_t at = url.find('@');
        const size_t colon = url.find(':', at);
        if (colon != std::string::npos) {
            parts.host = url.substr(at + 1, colon - at - 1);
            parts.path = stripGitSuffix(url.substr(colon + 1));
            parts.remote = !parts.host.empty() && !parts.path.empty();
        }
    }
    return parts;
}

void addUnique(std::vector<RepoUrl>& urls, const std::string& type, const std::string& url)
{
    if (url.empty()) return;
    for (const auto& item : urls) {
        if (lower(item.url) == lower(url)) return;
    }
    urls.push_back({type, url});
}

} // namespace

std::string detectedUrlType(const std::string& url)
{
    if (url.rfind("git@", 0) == 0 || url.rfind("ssh://", 0) == 0) return "ssh";
    if (url.rfind("https://", 0) == 0) return "https";
    if (url.rfind("http://", 0) == 0) return "http";
    return "local";
}

std::vector<RepoUrl> inferRepoUrls(const std::string& primary)
{
    std::vector<RepoUrl> urls;
    if (primary.empty()) return urls;

    const RemoteParts parts = parseRemote(primary);
    if (parts.remote) {
        addUnique(urls, "ssh", "git@" + parts.host + ":" + parts.path + ".git");
        addUnique(urls, "https", "https://" + parts.host + "/" + parts.path + ".git");
        addUnique(urls, "http", "http://" + parts.host + "/" + parts.path + ".git");
    }
    const std::string type = detectedUrlType(primary);
    addUnique(urls, type, primary);
    return urls;
}

std::vector<RepoUrl> workspaceRepoUrls(const nlohmann::json& workspace)
{
    std::vector<RepoUrl> urls;
    if (workspace.contains("git") && workspace["git"].is_object()) {
        const auto& git = workspace["git"];
        if (git.contains("remotes") && git["remotes"].is_array()) {
            for (const auto& entry : git["remotes"]) {
                if (!entry.is_object()) continue;
                const std::string url = entry.value("url", "");
                if (url.empty()) continue;
                std::string type = entry.value("type", "");
                if (type.empty()) type = detectedUrlType(url);
                addUnique(urls, type, url);
            }
        }
        if (urls.empty()) {
            const std::string remote = git.value("remote", "");
            if (git.contains("remote") && git["remote"].is_string()) {
                urls = inferRepoUrls(remote);
            }
        }
    }
    return urls;
}

void ensureWorkspaceRepoUrls(nlohmann::json& workspace)
{
    if (!workspace.is_object()) return;
    if (!workspace.contains("git") || !workspace["git"].is_object()) {
        workspace["git"] = nlohmann::json::object();
    }
    auto& git = workspace["git"];
    std::vector<RepoUrl> urls = workspaceRepoUrls(workspace);
    if (urls.empty()) {
        const std::string remote = git.value("remote", "");
        urls = inferRepoUrls(remote);
    }
    if (urls.empty()) return;

    nlohmann::json array = nlohmann::json::array();
    for (const auto& item : urls) {
        nlohmann::json entry = {{"type", item.type}, {"url", item.url}};
        array.push_back(entry);
    }
    git["remotes"] = array;
    if (!git.contains("remote") || git["remote"].get<std::string>().empty()) {
        git["remote"] = urls.front().url;
    }
}

nlohmann::json repoUrlsJson(const std::vector<RepoUrl>& urls)
{
    nlohmann::json array = nlohmann::json::array();
    for (const auto& item : urls) {
        array.push_back({{"type", item.type}, {"url", item.url}});
    }
    return array;
}

std::vector<RepoUrl> repoUrlsFromJson(const nlohmann::json& value)
{
    std::vector<RepoUrl> urls;
    if (!value.is_array()) return urls;
    for (const auto& entry : value) {
        if (!entry.is_object()) continue;
        const std::string url = entry.value("url", "");
        if (url.empty()) continue;
        std::string type = entry.value("type", "");
        if (type.empty()) type = detectedUrlType(url);
        addUnique(urls, type, url);
    }
    return urls;
}

std::string primaryRepoUrl(const std::vector<RepoUrl>& urls, const std::string& fallback)
{
    return urls.empty() ? fallback : urls.front().url;
}

} // namespace NeoWorkspace
