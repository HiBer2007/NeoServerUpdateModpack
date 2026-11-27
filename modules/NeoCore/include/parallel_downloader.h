#pragma once

#include <QString>
#include <QUrl>

#include <cstdint>
#include <functional>

namespace NeoCore {

class CancelToken;

/// 多线程（HTTP Range 分片）下载参数，语义参考 FLF 的 hmcl-parallel-download：
/// 线程数、体积阈值、分片大小、重试次数；环境变量可覆盖。
struct ParallelDownloadOptions {
    int threads = 16;              // 最大并发分片数
    qint64 minSize = 8LL * 1024 * 1024; // 小于该体积不启用多线程
    qint64 chunkSize = 0;          // 0 = 按总大小/线程数自动计算
    int retries = 3;               // 单个分片重试次数
    int connectTimeoutMs = 15000;
    int readTimeoutMs = 60000;

    /// 读取环境变量覆盖：NSUM_DOWNLOAD_THREADS / MIN_SIZE_MB / CHUNK_MB /
    /// RETRIES / PARALLEL（=0 关闭）。
    static ParallelDownloadOptions fromEnvironment();
};

struct ParallelDownloadResult {
    bool success = false;
    bool ranged = false;    // 服务器支持 Range（但可能因体积/失败未用）
    bool cancelled = false;
    qint64 bytes = 0;
    int threadsUsed = 0;
    QString message;
};

/// HTTP Range 分片多线程下载器（Qt 事件循环，无需额外线程）。
///
/// 流程：HEAD/Range 探测 → 预分配目标文件 → N 个 Range GET 并发写入
/// （QFile 定位写）→ 进度聚合 → 失败/不支持 Range 时返回 ranged=false
/// 由调用方回退单流。需要存在 QCoreApplication。
class ParallelDownloader {
public:
    using Progress = std::function<void(qint64 received, qint64 total)>;

    static ParallelDownloadResult download(const QUrl& url, const QString& targetPath,
                                           const ParallelDownloadOptions& options,
                                           Progress progress = nullptr,
                                           CancelToken* cancelToken = nullptr);
};

} // namespace NeoCore
