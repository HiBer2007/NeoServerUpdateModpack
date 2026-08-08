#include "sync_engine.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <logger.h>

namespace NeoWorkspace {

namespace fs = std::filesystem;

SyncEngine::SyncEngine()
    : targetDir_()
    , cacheDir_()
{
}

bool SyncEngine::init(const std::string& targetDir, const std::string& cacheDir)
{
    try {
        targetDir_ = targetDir;
        cacheDir_ = cacheDir;

        std::replace(targetDir_.begin(), targetDir_.end(), '\\', '/');
        std::replace(cacheDir_.begin(), cacheDir_.end(), '\\', '/');

        std::error_code ec;
        fs::create_directories(targetDir_, ec);
        if (ec) {
            CLogger::Error("SyncEngine: Cannot create target directory '{}': {}",
                targetDir_, ec.message());
            return false;
        }

        fs::create_directories(cacheDir_, ec);
        if (ec) {
            CLogger::Error("SyncEngine: Cannot create cache directory '{}': {}",
                cacheDir_, ec.message());
            return false;
        }

        CLogger::Info("SyncEngine: Initialized target='{}' cache='{}'",
            targetDir_, cacheDir_);
        return true;
    }
    catch (const std::exception& e) {
        CLogger::Error("SyncEngine: Init failed: {}", e.what());
        return false;
    }
}

SyncResult SyncEngine::syncFile(const std::string& sourcePath,
    const std::string& relativeTargetPath,
    const std::string& expectedSha256,
    NeoCore::CancelToken* cancelToken)
{
    SyncResult result;
    result.success = false;
    result.syncedFiles = 0;
    result.conflictedFiles = 0;
    result.failedFiles = 0;

    try {
        if (cancelToken && cancelToken->is_cancelled()) {
            result.messages.push_back("Sync cancelled");
            return result;
        }

        std::string relPath = relativeTargetPath;
        std::replace(relPath.begin(), relPath.end(), '\\', '/');

        if (!fs::exists(sourcePath)) {
            result.failedFiles = 1;
            result.messages.push_back("Source file not found: " + sourcePath);
            CLogger::Error("SyncEngine: Source file not found: {}", sourcePath);
            return result;
        }

        std::string computedHash = computeSha256(sourcePath);
        if (computedHash.empty()) {
            result.failedFiles = 1;
            result.messages.push_back("Cannot compute hash for source: " + sourcePath);
            CLogger::Error("SyncEngine: Cannot compute hash for source: {}", sourcePath);
            return result;
        }

        if (computedHash != expectedSha256) {
            result.failedFiles = 1;
            result.messages.push_back(
                "Hash mismatch for " + relPath +
                " (expected: " + expectedSha256 +
                ", actual: " + computedHash + ")");
            CLogger::Error("SyncEngine: Hash mismatch for {}: expected={} actual={}",
                relPath, expectedSha256, computedHash);
            return result;
        }

        storeInCacheFile(computedHash, sourcePath);

        fs::path targetPath = fs::path(targetDir_) / relPath;
        std::error_code ec;

        fs::create_directories(targetPath.parent_path(), ec);
        if (ec) {
            result.failedFiles = 1;
            result.messages.push_back(
                "Cannot create directory: " + targetPath.parent_path().string());
            CLogger::Error("SyncEngine: Cannot create parent dir for {}: {}",
                relPath, ec.message());
            return result;
        }

        if (!copyFileSafe(sourcePath, targetPath.string())) {
            result.failedFiles = 1;
            result.messages.push_back("Failed to copy: " + relPath);
            return result;
        }

        result.success = true;
        result.syncedFiles = 1;
        result.messages.push_back("Synced: " + relPath);
        CLogger::Debug("SyncEngine: Synced file '{}'", relPath);
    }
    catch (const std::exception& e) {
        result.failedFiles = 1;
        result.messages.push_back(std::string("Exception: ") + e.what());
        CLogger::Error("SyncEngine: Exception syncing file: {}", e.what());
    }

    return result;
}

bool SyncEngine::hasCachedFile(const std::string& sha256) const
{
    if (sha256.empty()) {
        return false;
    }

    std::string path = cacheFilePath(sha256);
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

std::string SyncEngine::cacheFilePath(const std::string& sha256) const
{
    std::string path = (fs::path(cacheDir_) / sha256).string();
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

bool SyncEngine::validateCacheFile(const std::string& sha256) const
{
    if (sha256.empty()) {
        return false;
    }

    if (!hasCachedFile(sha256)) {
        return false;
    }

    std::string path = cacheFilePath(sha256);
    std::string computedHash = computeSha256(path);

    if (computedHash.empty()) {
        CLogger::Error("SyncEngine: Cannot compute hash for cache validation: {}", path);
        return false;
    }

    if (computedHash != sha256) {
        CLogger::Warn(
            "SyncEngine: Cache validation failed for {}: stored hash mismatch, deleting corrupt entry",
            sha256);

        std::error_code ec;
        fs::remove(path, ec);
        return false;
    }

    return true;
}

bool SyncEngine::storeInCache(const std::string& sha256, const std::string& content)
{
    if (sha256.empty()) {
        CLogger::Error("SyncEngine: Cannot store in cache with empty sha256");
        return false;
    }

    std::string computedHash = computeSha256FromData(content);
    if (computedHash != sha256) {
        CLogger::Error(
            "SyncEngine: Content hash mismatch when storing in cache: expected={} actual={}",
            sha256, computedHash);
        return false;
    }

    if (hasCachedFile(sha256) && validateCacheFile(sha256)) {
        CLogger::Debug("SyncEngine: Cache entry {} already exists and valid", sha256);
        return true;
    }

    std::string path = cacheFilePath(sha256);
    if (!writeFileSafe(path, content)) {
        CLogger::Error("SyncEngine: Failed to write cache entry {}", sha256);
        return false;
    }

    CLogger::Debug("SyncEngine: Stored {} bytes in cache as {}", content.size(), sha256);
    return true;
}

bool SyncEngine::storeInCacheFile(const std::string& sha256, const std::string& sourcePath)
{
    if (sha256.empty() || sourcePath.empty()) {
        return false;
    }

    if (hasCachedFile(sha256) && validateCacheFile(sha256)) {
        return true;
    }

    std::string fullSourcePath = sourcePath;
    std::replace(fullSourcePath.begin(), fullSourcePath.end(), '\\', '/');

    try {
        if (!fs::exists(fullSourcePath)) {
            CLogger::Error("SyncEngine: Source file for cache does not exist: {}",
                fullSourcePath);
            return false;
        }

        std::string computedHash = computeSha256(fullSourcePath);
        if (computedHash.empty()) {
            return false;
        }
        if (computedHash != sha256) {
            CLogger::Error(
                "SyncEngine: Source hash mismatch for cache: expected={} actual={}",
                sha256, computedHash);
            return false;
        }

        std::string cachePath = cacheFilePath(sha256);
        return copyFileSafe(fullSourcePath, cachePath);
    }
    catch (const std::exception& e) {
        CLogger::Error("SyncEngine: Error storing file in cache: {}", e.what());
        return false;
    }
}

SyncResult SyncEngine::syncConfig(const std::string& configPath,
    NeoCore::IConfigParser* parser,
    NeoCore::TrackingMode mode,
    const std::string& remoteContent,
    const std::string& localContent,
    const std::vector<std::string>& trackedKeys,
    const std::vector<int>& trackedLines,
    NeoCore::CancelToken* cancelToken)
{
    SyncResult result;
    result.success = false;
    result.syncedFiles = 0;
    result.conflictedFiles = 0;
    result.failedFiles = 0;

    try {
        if (cancelToken && cancelToken->is_cancelled()) {
            result.messages.push_back("Config sync cancelled");
            return result;
        }

        if (!parser) {
            result.failedFiles = 1;
            result.messages.push_back("No config parser available for: " + configPath);
            CLogger::Error("SyncEngine: No parser for config sync: {}", configPath);
            return result;
        }

        std::string merged;

        switch (mode) {
        case NeoCore::TrackingMode::ConfigMerge: {
            merged = parser->merge_entries(configPath, trackedKeys,
                remoteContent, localContent);
            break;
        }
        case NeoCore::TrackingMode::LineByLine: {
            merged = parser->merge_lines(configPath, trackedLines,
                remoteContent, localContent);
            break;
        }
        case NeoCore::TrackingMode::FullSync: {
            merged = remoteContent;
            break;
        }
        case NeoCore::TrackingMode::NoSync: {
            result.success = true;
            result.messages.push_back("Config skipped (NoSync): " + configPath);
            return result;
        }
        default: {
            merged = remoteContent;
            break;
        }
        }

        if (merged.empty() && mode != NeoCore::TrackingMode::NoSync) {
            CLogger::Warn("SyncEngine: Merged content is empty for {}, falling back to remote",
                configPath);
            merged = remoteContent;
        }

        std::string relPath = configPath;
        std::replace(relPath.begin(), relPath.end(), '\\', '/');

        fs::path targetPath = fs::path(targetDir_) / relPath;
        std::error_code ec;
        fs::create_directories(targetPath.parent_path(), ec);
        if (ec) {
            result.failedFiles = 1;
            result.messages.push_back(
                "Cannot create directory for config: " + targetPath.parent_path().string());
            return result;
        }

        if (!writeFileSafe(targetPath.string(), merged)) {
            result.failedFiles = 1;
            result.messages.push_back("Failed to write merged config: " + relPath);
            return result;
        }

        result.success = true;
        result.syncedFiles = 1;
        result.messages.push_back("Config synced: " + relPath);
        CLogger::Info("SyncEngine: Config sync complete for {}", relPath);
    }
    catch (const std::exception& e) {
        result.failedFiles = 1;
        result.messages.push_back(std::string("Config sync exception: ") + e.what());
        CLogger::Error("SyncEngine: Exception during config sync: {}", e.what());
    }

    return result;
}

void SyncEngine::cleanTargetDir()
{
    try {
        if (targetDir_.empty()) {
            CLogger::Error("SyncEngine: Cannot clean empty target directory path");
            return;
        }

        fs::path target = targetDir_;
        if (!fs::exists(target)) {
            return;
        }

        std::error_code ec;
        fs::remove_all(target, ec);
        if (ec) {
            CLogger::Error("SyncEngine: Failed to clean target dir: {}", ec.message());
        }
        else {
            fs::create_directories(target, ec);
            CLogger::Info("SyncEngine: Target directory cleaned");
        }
    }
    catch (const std::exception& e) {
        CLogger::Error("SyncEngine: Exception cleaning target dir: {}", e.what());
    }
}

void SyncEngine::cleanCache()
{
    try {
        if (cacheDir_.empty()) {
            CLogger::Error("SyncEngine: Cannot clean empty cache directory path");
            return;
        }

        fs::path cache = cacheDir_;
        if (!fs::exists(cache)) {
            return;
        }

        std::error_code ec;
        fs::remove_all(cache, ec);
        if (ec) {
            CLogger::Error("SyncEngine: Failed to clean cache dir: {}", ec.message());
        }
        else {
            fs::create_directories(cache, ec);
            CLogger::Info("SyncEngine: Cache directory cleaned");
        }
    }
    catch (const std::exception& e) {
        CLogger::Error("SyncEngine: Exception cleaning cache: {}", e.what());
    }
}

std::string SyncEngine::computeSha256(const std::string& filepath) const
{
    try {
        QFile file(QString::fromStdString(filepath));
        if (!file.open(QIODevice::ReadOnly)) {
            CLogger::Error("SyncEngine: Cannot open file for hash: {}", filepath);
            return "";
        }

        QCryptographicHash hasher(QCryptographicHash::Sha256);

        constexpr qint64 chunkSize = 64 * 1024;
        QByteArray buffer;

        while (!file.atEnd()) {
            buffer = file.read(chunkSize);
            if (buffer.isEmpty()) {
                break;
            }
            hasher.addData(buffer);
        }

        file.close();

        if (hasher.result().isEmpty()) {
            return "";
        }

        return QString(hasher.result().toHex()).toStdString();
    }
    catch (const std::exception& e) {
        CLogger::Error("SyncEngine: Exception computing hash for {}: {}",
            filepath, e.what());
        return "";
    }
}

std::string SyncEngine::computeSha256FromData(const std::string& data) const
{
    try {
        QByteArray qdata(data.data(), static_cast<int>(data.size()));
        QCryptographicHash hasher(QCryptographicHash::Sha256);
        hasher.addData(qdata);
        return QString(hasher.result().toHex()).toStdString();
    }
    catch (const std::exception& e) {
        CLogger::Error("SyncEngine: Exception computing hash from data: {}", e.what());
        return "";
    }
}

bool SyncEngine::copyFileSafe(const std::string& src, const std::string& dst)
{
    try {
        std::string normSrc = src;
        std::string normDst = dst;
        std::replace(normSrc.begin(), normSrc.end(), '\\', '/');
        std::replace(normDst.begin(), normDst.end(), '\\', '/');

        QString srcPath = QString::fromStdString(normSrc);
        QString dstPath = QString::fromStdString(normDst);

        QFileInfo srcInfo(srcPath);
        if (!srcInfo.exists() || !srcInfo.isFile()) {
            CLogger::Error("SyncEngine: Cannot copy - source not found: {}", normSrc);
            return false;
        }

        QFileInfo dstInfo(dstPath);
        if (dstInfo.exists()) {
            QFile::remove(dstPath);
        }

        std::error_code ec;
        fs::path parentDir = fs::path(normDst).parent_path();
        fs::create_directories(parentDir, ec);
        if (ec) {
            CLogger::Error("SyncEngine: Cannot create parent directory for copy: {} -> {}",
                normDst, ec.message());
            return false;
        }

        QFile srcFile(srcPath);
        if (!srcFile.open(QIODevice::ReadOnly)) {
            CLogger::Error("SyncEngine: Cannot open source for copy: {}", normSrc);
            return false;
        }

        QFile dstFile(dstPath);
        if (!dstFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            srcFile.close();
            CLogger::Error("SyncEngine: Cannot open destination for copy: {}", normDst);
            return false;
        }

        constexpr qint64 bufferSize = 64 * 1024;

        while (!srcFile.atEnd()) {
            QByteArray buffer = srcFile.read(bufferSize);
            if (buffer.isEmpty() && srcFile.atEnd()) {
                break;
            }
            qint64 written = dstFile.write(buffer);
            if (written != buffer.size()) {
                srcFile.close();
                dstFile.close();
                QFile::remove(dstPath);
                CLogger::Error("SyncEngine: Write error during copy to {}", normDst);
                return false;
            }
        }

        srcFile.close();
        dstFile.close();

        dstFile.flush();

        if (dstFile.error() != QFile::NoError) {
            QFile::remove(dstPath);
            CLogger::Error("SyncEngine: Copy file error for {}: {}",
                normDst, dstFile.errorString().toStdString());
            return false;
        }

        CLogger::Debug("SyncEngine: Copied {} to {}", normSrc, normDst);
        return true;
    }
    catch (const std::exception& e) {
        CLogger::Error("SyncEngine: Exception copying file {}: {}", src, e.what());
        return false;
    }
}

bool SyncEngine::writeFileSafe(const std::string& path, const std::string& content)
{
    try {
        std::string normPath = path;
        std::replace(normPath.begin(), normPath.end(), '\\', '/');

        fs::path parentDir = fs::path(normPath).parent_path();
        std::error_code ec;
        fs::create_directories(parentDir, ec);
        if (ec) {
            CLogger::Error("SyncEngine: Cannot create parent directory for write: {} -> {}",
                normPath, ec.message());
            return false;
        }

        QString qPath = QString::fromStdString(normPath);
        QString tmpPath = qPath + ".tmp_" + QString::number(
            std::chrono::steady_clock::now().time_since_epoch().count());

        QFile tmpFile(tmpPath);
        if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            CLogger::Error("SyncEngine: Cannot open temp file for write: {}",
                tmpPath.toStdString());
            return false;
        }

        QByteArray data(content.data(), static_cast<int>(content.size()));
        qint64 written = tmpFile.write(data);
        if (written != static_cast<qint64>(content.size())) {
            tmpFile.close();
            QFile::remove(tmpPath);
            CLogger::Error("SyncEngine: Partial write to {} ({} of {} bytes)",
                normPath, written, content.size());
            return false;
        }

        tmpFile.flush();
        tmpFile.close();

        if (tmpFile.error() != QFile::NoError) {
            QFile::remove(tmpPath);
            CLogger::Error("SyncEngine: Write error for {}: {}",
                normPath, tmpFile.errorString().toStdString());
            return false;
        }

        if (QFile::exists(qPath)) {
            QFile::remove(qPath);
        }

        if (!QFile::rename(tmpPath, qPath)) {
            QFile::remove(tmpPath);
            CLogger::Error("SyncEngine: Failed to rename temp file to {}",
                normPath);
            return false;
        }

        CLogger::Debug("SyncEngine: Wrote {} bytes to {}", content.size(), normPath);
        return true;
    }
    catch (const std::exception& e) {
        CLogger::Error("SyncEngine: Exception writing file {}: {}", path, e.what());
        return false;
    }
}

} // namespace NeoWorkspace

