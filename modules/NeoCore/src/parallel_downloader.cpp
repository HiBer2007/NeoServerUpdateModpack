#include "parallel_downloader.h"

#include "cancel_token.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace NeoCore {

namespace {

constexpr qint64 kMinChunk = 1024 * 1024;
constexpr qint64 kMaxChunk = 4 * 1024 * 1024;
constexpr int kMaxThreads = 64;
constexpr int kMaxPerServer = 32;

qint64 envInt(const char* name, qint64 fallback)
{
    const QByteArray value = qgetenv(name);
    if (value.isEmpty()) return fallback;
    bool ok = false;
    const qint64 parsed = value.toLongLong(&ok);
    return ok ? parsed : fallback;
}

struct Probe {
    bool ok = false;
    bool ranged = false;
    qint64 length = -1;
};

qint64 headerLength(QNetworkReply* reply)
{
    bool ok = false;
    const qint64 value = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&ok);
    return ok ? value : -1;
}

qint64 contentRangeTotal(const QByteArray& value)
{
    const int slash = value.lastIndexOf('/');
    if (slash < 0) return -1;
    bool ok = false;
    const qint64 total = value.mid(slash + 1).trimmed().toLongLong(&ok);
    return ok ? total : -1;
}

Probe probe(const QUrl& url, const ParallelDownloadOptions& options)
{
    QNetworkAccessManager manager;
    manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    auto runRequest = [&](bool head, bool rangeProbe) -> QNetworkReply* {
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, "NSUM/1.0");
        if (rangeProbe) request.setRawHeader("Range", "bytes=0-0");
        QNetworkReply* reply = head ? manager.head(request) : manager.get(request);
        if (!reply) return nullptr;
        reply->setReadBufferSize(1024 * 1024);
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(options.connectTimeoutMs + options.readTimeoutMs);
        loop.exec();
        if (!reply->isFinished()) reply->abort();
        return reply;
    };

    Probe result;
    if (QNetworkReply* reply = runRequest(true, false)) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const qint64 length = headerLength(reply);
        const QByteArray accept = reply->rawHeader("Accept-Ranges");
        reply->deleteLater();
        if (status >= 200 && status < 300 && length > 0) {
            result.ok = true;
            result.length = length;
            result.ranged = accept.toLower().contains("bytes");
            if (result.ranged) return result;
        }
    }

    if (QNetworkReply* reply = runRequest(false, true)) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray contentRange = reply->rawHeader("Content-Range");
        reply->readAll();
        reply->deleteLater();
        if (status == 206) {
            const qint64 total = contentRangeTotal(contentRange);
            if (total > 0) {
                result.ok = true;
                result.ranged = true;
                result.length = total;
            }
        }
    }
    return result;
}

} // namespace

ParallelDownloadOptions ParallelDownloadOptions::fromEnvironment()
{
    ParallelDownloadOptions options;
    if (qgetenv("NSUM_DOWNLOAD_PARALLEL") == "0") {
        options.threads = 1;
        return options;
    }
    options.threads = static_cast<int>(envInt("NSUM_DOWNLOAD_THREADS", options.threads));
    options.threads = std::max(1, std::min(options.threads, kMaxThreads));
    options.minSize = envInt("NSUM_DOWNLOAD_MIN_SIZE_MB", options.minSize / (1024 * 1024))
        * 1024 * 1024;
    options.chunkSize = envInt("NSUM_DOWNLOAD_CHUNK_MB", options.chunkSize / (1024 * 1024))
        * 1024 * 1024;
    options.retries = static_cast<int>(envInt("NSUM_DOWNLOAD_RETRIES", options.retries));
    options.retries = std::max(1, std::min(options.retries, 10));
    return options;
}

ParallelDownloadResult ParallelDownloader::download(const QUrl& url,
                                                    const QString& targetPath,
                                                    const ParallelDownloadOptions& options,
                                                    Progress progress,
                                                    CancelToken* cancelToken)
{
    ParallelDownloadResult result;
    if (!url.isValid() || targetPath.isEmpty()) {
        result.message = QStringLiteral("invalid url or target");
        return result;
    }

    Probe probeResult = probe(url, options);
    if (!probeResult.ok || probeResult.length <= 0) {
        result.message = QStringLiteral("cannot probe remote size");
        return result;
    }
    result.ranged = probeResult.ranged;
    if (!probeResult.ranged || probeResult.length < options.minSize) {
        result.message = probeResult.ranged
            ? QStringLiteral("file below parallel threshold")
            : QStringLiteral("server does not support Range");
        return result;
    }

    const qint64 total = probeResult.length;
    qint64 chunk = options.chunkSize > 0 ? options.chunkSize
        : std::clamp(total / static_cast<qint64>(std::max(1, options.threads) * 16),
                     kMinChunk, kMaxChunk);
    chunk = std::max<qint64>(256 * 1024, std::min(chunk, total));
    int segments = static_cast<int>(std::min<qint64>(
        std::max(1, options.threads), (total + chunk - 1) / chunk));
    segments = std::min(segments, kMaxPerServer);

    QFile file(targetPath);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        result.message = QStringLiteral("cannot open target file");
        return result;
    }
    if (!file.resize(total)) {
        result.message = QStringLiteral("cannot allocate target file");
        return result;
    }

    struct Segment {
        qint64 start = 0;
        qint64 end = 0;
        qint64 written = 0;
        int attempts = 0;
        QNetworkReply* reply = nullptr;
    };
    std::vector<Segment> states(static_cast<size_t>(segments));
    for (int i = 0; i < segments; ++i) {
        states[static_cast<size_t>(i)].start = static_cast<qint64>(i) * chunk;
        states[static_cast<size_t>(i)].end =
            (i == segments - 1) ? total - 1 : states[static_cast<size_t>(i)].start + chunk - 1;
    }

    QNetworkAccessManager manager;
    manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    QEventLoop loop;
    int active = 0;
    bool failed = false;
    bool cancelled = false;
    QString failure;

    qint64 received = 0;
    QElapsedTimer reportTimer;
    reportTimer.start();
    auto report = [&](qint64 delta) {
        received += delta;
        if (!progress) return;
        if (received == total || reportTimer.elapsed() >= 300) {
            reportTimer.restart();
            progress(received, total);
        }
    };

    std::function<void(int)> startSegment = [&](int index) {
        Segment& segment = states[static_cast<size_t>(index)];
        if (segment.written >= segment.end - segment.start + 1) {
            --active;
            if (active == 0) loop.quit();
            return;
        }
        if (failed || cancelled) return;
        ++segment.attempts;
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, "NSUM/1.0");
        const qint64 from = segment.start + segment.written;
        request.setRawHeader("Range",
            "bytes=" + QByteArray::number(from) + "-" + QByteArray::number(segment.end));
        QNetworkReply* reply = manager.get(request);
        if (!reply) {
            failed = true;
            failure = QStringLiteral("cannot start request");
            loop.quit();
            return;
        }
        segment.reply = reply;
        ++active;

        QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&, index, reply]() {
            Segment& state = states[static_cast<size_t>(index)];
            if (failed || cancelled) {
                reply->abort();
                return;
            }
            const QByteArray data = reply->readAll();
            if (data.isEmpty()) return;
            if (!file.seek(state.start + state.written)
                || file.write(data) != data.size()) {
                failed = true;
                failure = QStringLiteral("cannot write target file");
                reply->abort();
                return;
            }
            state.written += data.size();
            report(data.size());
        });

        QObject::connect(reply, &QNetworkReply::finished, &loop, [&, index, reply]() {
            Segment& state = states[static_cast<size_t>(index)];
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool ok = reply->error() == QNetworkReply::NoError && status == 206
                && state.written >= state.end - state.start + 1;
            reply->deleteLater();
            state.reply = nullptr;
            if (cancelled || failed) {
                --active;
                if (active == 0) loop.quit();
                return;
            }
            if (!ok && state.attempts <= options.retries && !cancelled) {
                --active;
                QTimer::singleShot(200 * state.attempts, &loop, [&, index]() {
                    if (!failed && !cancelled) startSegment(index);
                });
                return;
            }
            if (!ok) {
                failed = true;
                failure = status == 200
                    ? QStringLiteral("server ignored Range")
                    : QStringLiteral("segment failed");
                for (auto& other : states) {
                    if (other.reply) other.reply->abort();
                }
                loop.quit();
                return;
            }
            --active;
            if (active == 0) loop.quit();
        });
    };

    QTimer cancelTimer;
    cancelTimer.setInterval(100);
    if (cancelToken) {
        QObject::connect(&cancelTimer, &QTimer::timeout, &loop, [&]() {
            if (cancelToken->is_cancelled() && !cancelled) {
                cancelled = true;
                for (auto& state : states) {
                    if (state.reply) state.reply->abort();
                }
                loop.quit();
            }
        });
        cancelTimer.start();
    }

    for (int i = 0; i < segments; ++i) startSegment(i);
    if (active > 0) loop.exec();
    cancelTimer.stop();

    for (auto& state : states) {
        if (state.reply) {
            state.reply->abort();
            state.reply->deleteLater();
        }
    }
    file.close();

    if (cancelled || (cancelToken && cancelToken->is_cancelled())) {
        QFile::remove(targetPath);
        result.cancelled = true;
        result.message = QStringLiteral("cancelled");
        return result;
    }
    if (failed || QFileInfo(targetPath).size() != total) {
        QFile::remove(targetPath);
        result.message = failure.isEmpty() ? QStringLiteral("parallel download failed")
                                           : failure;
        return result;
    }

    if (progress) progress(total, total);
    result.success = true;
    result.bytes = total;
    result.threadsUsed = segments;
    return result;
}

} // namespace NeoCore
