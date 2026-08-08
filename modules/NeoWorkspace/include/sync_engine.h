#pragma once

#include <string>
#include <vector>
#include <IConfigParser.h>
#include <cancel_token.h>

namespace NeoWorkspace {

struct SyncResult {
    bool success;
    int syncedFiles;
    int conflictedFiles;
    int failedFiles;
    std::vector<std::string> messages;
};

class SyncEngine {
public:
    SyncEngine();

    bool init(const std::string& targetDir, const std::string& cacheDir);

    SyncResult syncFile(const std::string& sourcePath,
        const std::string& relativeTargetPath,
        const std::string& expectedSha256,
        NeoCore::CancelToken* cancelToken = nullptr);

    bool hasCachedFile(const std::string& sha256) const;
    std::string cacheFilePath(const std::string& sha256) const;
    bool validateCacheFile(const std::string& sha256) const;
    bool storeInCache(const std::string& sha256, const std::string& content);
    bool storeInCacheFile(const std::string& sha256, const std::string& sourcePath);

    SyncResult syncConfig(const std::string& configPath,
        NeoCore::IConfigParser* parser,
        NeoCore::TrackingMode mode,
        const std::string& remoteContent,
        const std::string& localContent,
        const std::vector<std::string>& trackedKeys,
        const std::vector<int>& trackedLines,
        NeoCore::CancelToken* cancelToken = nullptr);

    void cleanTargetDir();
    void cleanCache();

private:
    std::string targetDir_;
    std::string cacheDir_;

    std::string computeSha256(const std::string& filepath) const;
    std::string computeSha256FromData(const std::string& data) const;
    bool copyFileSafe(const std::string& src, const std::string& dst);
    bool writeFileSafe(const std::string& path, const std::string& content);
};

} // namespace NeoWorkspace

