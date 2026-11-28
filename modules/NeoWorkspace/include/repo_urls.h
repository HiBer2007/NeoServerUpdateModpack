#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace NeoWorkspace {

struct RepoUrl {
    std::string type;
    std::string url;
};

std::vector<RepoUrl> inferRepoUrls(const std::string& primary);

std::vector<RepoUrl> workspaceRepoUrls(const nlohmann::json& workspace);

void ensureWorkspaceRepoUrls(nlohmann::json& workspace);

nlohmann::json repoUrlsJson(const std::vector<RepoUrl>& urls);

std::vector<RepoUrl> repoUrlsFromJson(const nlohmann::json& value);

std::string primaryRepoUrl(const std::vector<RepoUrl>& urls, const std::string& fallback);

std::string detectedUrlType(const std::string& url);

} // namespace NeoWorkspace
