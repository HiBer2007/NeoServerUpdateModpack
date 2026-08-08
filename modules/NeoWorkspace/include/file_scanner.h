#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <ctime>
#include <cstdint>
#include <IPluginPointer.h>
#include <cancel_token.h>

namespace NeoWorkspace {

struct FileEntry {
    std::string relativePath;
    std::string absolutePath;
    std::string sha256;
    uint64_t fileSize;
    bool isPointer;
    std::time_t lastModified;
};

class FileScanner {
public:
    FileScanner();

    std::vector<FileEntry> scanDirectory(const std::string& rootDir,
        const std::vector<std::string>& excludePatterns = {},
        bool computeHashes = false,
        NeoCore::CancelToken* cancelToken = nullptr);

    std::vector<FileEntry> scanPointers(const std::string& dir);

    std::string computeSha256(const std::string& filepath);

    static NeoCore::PointerInfo parsePointerFile(const std::string& filepath);

    struct DiffResult {
        std::vector<std::string> added;
        std::vector<std::string> removed;
        std::vector<std::string> modified;
        std::vector<std::string> unchanged;
    };
    DiffResult diff(const std::vector<FileEntry>& oldFiles,
        const std::vector<FileEntry>& newFiles) const;

private:
    bool matchesExclude(const std::string& path,
        const std::vector<std::string>& patterns) const;
};

} // namespace NeoWorkspace

