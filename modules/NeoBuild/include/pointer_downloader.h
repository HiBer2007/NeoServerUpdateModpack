#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <IPluginPointer.h>
#include <cancel_token.h>

namespace NeoBuild {

struct DownloadProgress {
    std::string sha256;
    int64_t bytesDownloaded = 0;
    int64_t totalBytes = -1;
    bool completed = false;
    std::string error;
};

struct DownloadResult {
    bool success = false;
    std::string cachedPath;
    std::string sha256;
    std::string errorMessage;
};

struct ResolveResult {
    bool success = false;
    std::string url;
    std::string resolver;
    std::string errorMessage;
};

class PointerDownloader {
public:
    PointerDownloader();
    ~PointerDownloader();

    void registerResolver(std::unique_ptr<NeoCore::IPluginPointer> resolver);

    void scanResolvers(const std::string& pointersDir);

    DownloadResult download(const NeoCore::PointerInfo& pointer,
        const std::string& cacheDir,
        std::function<void(const DownloadProgress&)> progressCallback = nullptr,
        NeoCore::CancelToken* cancelToken = nullptr);

    std::vector<DownloadResult> downloadAll(
        const std::vector<NeoCore::PointerInfo>& pointers,
        const std::string& cacheDir,
        std::function<void(const DownloadProgress&)> progressCallback = nullptr,
        NeoCore::CancelToken* cancelToken = nullptr);

bool isCached(const std::string& cacheDir, const std::string& sha256) const;

    std::string cachePath(const std::string& cacheDir, const std::string& sha256) const;

    ResolveResult resolveUrl(const NeoCore::PointerInfo& pointer) const;

private:
    struct LoadedResolver {
        std::unique_ptr<NeoCore::IPluginPointer> instance;
        void* dllHandle = nullptr;
    };

    std::vector<LoadedResolver> resolvers_;

    NeoCore::IPluginPointer* findResolver(const NeoCore::PointerInfo& ptr) const;
    bool validateFile(const std::string& filepath, const std::string& expectedSha256) const;
    bool downloadToPath(const std::string& url, const std::string& outputPath,
        std::function<void(const DownloadProgress&)> progressCallback,
        NeoCore::CancelToken* cancelToken);
    void loadResolverDLL(const std::string& dllPath);
};

} // namespace NeoBuild

