#include "git_downloader.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDir>
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#ifdef _WIN32
#include <windows.h>
#endif

namespace NeoInstaller {

GitDownloader::GitDownloader(QObject* parent)
    : QObject(parent)
    , network_(new QNetworkAccessManager(this))
{
}

void GitDownloader::startDownload(GitVariant variant, const QString& extractDir)
{
    if (running_) return;

    variant_ = variant;
    extractDir_ = extractDir;
    cancelled_ = false;
    running_ = true;

    QString url = fetchLatestGitUrl(variant);
    if (url.isEmpty()) {
        running_ = false;
        emit finished(false, QStringLiteral("无法获取 Git 下载地址，请检查网络连接后重试"));
        return;
    }

    QString tempName = QStringLiteral("git_download_%1.zip").arg(QUuid::createUuid().toString(QUuid::Id128));
    QString tempDir = QDir::tempPath();
    QString tempPath = QDir(tempDir).filePath(tempName);

    tempFile_ = new QFile(tempPath, this);
    if (!tempFile_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        running_ = false;
        QString err = tempFile_->errorString();
        tempFile_->close();
        tempFile_->remove();
        tempFile_ = nullptr;
        emit finished(false, QStringLiteral("无法创建临时文件: %1").arg(err));
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("Accept", "application/octet-stream");
    request.setRawHeader("User-Agent", "NeoInstaller/1.0");

    currentReply_ = network_->get(request);

    connect(currentReply_, &QNetworkReply::metaDataChanged,
            this, &GitDownloader::onMetaDataChanged);
    connect(currentReply_, &QNetworkReply::downloadProgress,
            this, &GitDownloader::onDownloadProgress);
    connect(currentReply_, &QNetworkReply::finished,
            this, &GitDownloader::onDownloadFinished);

    emit progressChanged(0, QStringLiteral("正在获取下载地址..."));
}

void GitDownloader::cancel()
{
    cancelled_ = true;
    if (currentReply_) {
        currentReply_->abort();
    }
}

bool GitDownloader::isRunning() const
{
    return running_;
}

QString GitDownloader::fetchLatestGitUrl(GitVariant variant)
{
    QNetworkAccessManager nam;
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/git-for-windows/git/releases/latest")));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "NeoInstaller/1.0");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam.get(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(15000);

    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        return {};
    }

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return {};
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode == 403) {
        int retryAfter = reply->rawHeader("Retry-After").toInt();
        if (retryAfter <= 0) retryAfter = 3;

        int remaining = reply->rawHeader("X-RateLimit-Remaining").toInt();
        if (remaining == 0) {
            reply->deleteLater();

            QNetworkAccessManager retryNam;
            QNetworkRequest retryRequest(QUrl(
                QStringLiteral("https://api.github.com/repos/git-for-windows/git/releases/latest")));
            retryRequest.setRawHeader("Accept", "application/vnd.github+json");
            retryRequest.setRawHeader("User-Agent", "NeoInstaller/1.0");
            retryRequest.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
            retryRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                     QNetworkRequest::NoLessSafeRedirectPolicy);

            QNetworkReply* retryReply = retryNam.get(retryRequest);

            QEventLoop retryLoop;
            QTimer retryTimer;
            retryTimer.setSingleShot(true);
            connect(retryReply, &QNetworkReply::finished, &retryLoop, &QEventLoop::quit);
            connect(&retryTimer, &QTimer::timeout, &retryLoop, &QEventLoop::quit);
            retryTimer.start(15000);

            retryLoop.exec();

            if (!retryReply->isFinished() || retryReply->error() != QNetworkReply::NoError) {
                retryReply->deleteLater();
                return {};
            }

            QByteArray retryData = retryReply->readAll();
            retryReply->deleteLater();

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(retryData, &parseError);
            if (parseError.error != QJsonParseError::NoError) return {};

            QJsonObject release = doc.object();
            QJsonArray assets = release.value(QStringLiteral("assets")).toArray();

            if (variant == GitVariant::MinGit) {
                for (const QJsonValue& val : assets) {
                    QJsonObject asset = val.toObject();
                    QString name = asset.value(QStringLiteral("name")).toString();
                    if (name.contains(QStringLiteral("MinGit"), Qt::CaseInsensitive) &&
                        name.contains(QStringLiteral("64-bit"), Qt::CaseInsensitive) &&
                        name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                        return asset.value(QStringLiteral("browser_download_url")).toString();
                    }
                }
            } else {
                for (const QJsonValue& val : assets) {
                    QJsonObject asset = val.toObject();
                    QString name = asset.value(QStringLiteral("name")).toString();
                    if (name.contains(QStringLiteral("PortableGit"), Qt::CaseInsensitive) &&
                        name.contains(QStringLiteral("64-bit"), Qt::CaseInsensitive) &&
                        name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                        return asset.value(QStringLiteral("browser_download_url")).toString();
                    }
                }
            }
            return {};
        }
        reply->deleteLater();
        return {};
    }

    if (statusCode != 200) {
        reply->deleteLater();
        return {};
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) return {};

    QJsonObject release = doc.object();
    QJsonArray assets = release.value(QStringLiteral("assets")).toArray();

    if (variant == GitVariant::MinGit) {
        for (const QJsonValue& val : assets) {
            QJsonObject asset = val.toObject();
            QString name = asset.value(QStringLiteral("name")).toString();
            if (name.contains(QStringLiteral("MinGit"), Qt::CaseInsensitive) &&
                name.contains(QStringLiteral("64-bit"), Qt::CaseInsensitive) &&
                name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                return asset.value(QStringLiteral("browser_download_url")).toString();
            }
        }
    } else {
        for (const QJsonValue& val : assets) {
            QJsonObject asset = val.toObject();
            QString name = asset.value(QStringLiteral("name")).toString();
            if (name.contains(QStringLiteral("PortableGit"), Qt::CaseInsensitive) &&
                name.contains(QStringLiteral("64-bit"), Qt::CaseInsensitive) &&
                name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                return asset.value(QStringLiteral("browser_download_url")).toString();
            }
        }
    }

    return {};
}

bool GitDownloader::isSystemGitAvailable()
{
    QProcess process;
    process.start(QStringLiteral("git"), QStringList() << QStringLiteral("--version"));
    process.waitForFinished(5000);
    return process.exitCode() == 0;
}

QString GitDownloader::systemGitPath()
{
#ifdef _WIN32
    QProcess whereProc;
    whereProc.start(QStringLiteral("where"), QStringList() << QStringLiteral("git"));
    whereProc.waitForFinished(5000);
    if (whereProc.exitCode() == 0) {
        QString out = QString::fromUtf8(whereProc.readAllStandardOutput()).trimmed();
        QStringList lines = out.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
        if (lines.isEmpty()) {
            lines = out.split('\n', Qt::SkipEmptyParts);
        }
        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            if (QFileInfo::exists(trimmed)) {
                return QDir::toNativeSeparators(trimmed);
            }
        }
    }

    const char* commonPaths[] = {
        "C:\\Program Files\\Git\\bin\\git.exe",
        "C:\\Program Files (x86)\\Git\\bin\\git.exe",
        "C:\\Git\\bin\\git.exe",
        "C:\\msys64\\usr\\bin\\git.exe",
        "C:\\msys64\\mingw64\\bin\\git.exe",
    };
    for (const char* cp : commonPaths) {
        if (QFileInfo::exists(cp)) return QDir::toNativeSeparators(cp);
    }

    const char* pathEnv = std::getenv("PATH");
    if (pathEnv) {
        QString pathStr = QString::fromLocal8Bit(pathEnv);
        QStringList dirs = pathStr.split(';', Qt::SkipEmptyParts);
        for (const QString& dir : dirs) {
            QString candidate = QDir(dir).filePath(QStringLiteral("git.exe"));
            if (QFileInfo::exists(candidate)) {
                return QDir::toNativeSeparators(candidate);
            }
        }
    }
#else
    QProcess whichProc;
    whichProc.start(QStringLiteral("which"), QStringList() << QStringLiteral("git"));
    whichProc.waitForFinished(5000);
    if (whichProc.exitCode() == 0) {
        QString path = QString::fromUtf8(whichProc.readAllStandardOutput()).trimmed();
        if (!path.isEmpty() && QFileInfo::exists(path)) return path;
    }

    const char* fallbackPaths[] = {
        "/usr/bin/git",
        "/usr/local/bin/git",
        "/opt/homebrew/bin/git",
    };
    for (const char* fp : fallbackPaths) {
        if (QFileInfo::exists(fp)) return fp;
    }
#endif
    return {};
}

QString GitDownloader::systemGitVersion()
{
    QProcess process;
    process.start(QStringLiteral("git"), QStringList() << QStringLiteral("--version"));
    process.waitForFinished(5000);
    if (process.exitCode() != 0) return {};

    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    const QString prefix = QStringLiteral("git version ");
    if (output.startsWith(prefix, Qt::CaseInsensitive)) {
        return output.mid(prefix.length());
    }
    return output;
}

void GitDownloader::onMetaDataChanged()
{
    if (!currentReply_ || cancelled_) return;

    qint64 total = currentReply_->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    if (total > 0) {
        emit downloadStarted(currentReply_->url().toString(), total);
    }
}

void GitDownloader::onDownloadProgress(qint64 received, qint64 total)
{
    if (cancelled_) return;

    if (total > 0) {
        int percent = static_cast<int>((received * 100) / total);
        emit downloadProgress(received, total);
        emit progressChanged(percent, QStringLiteral("正在下载 Git (%1 MB / %2 MB)")
                                        .arg(received / (1024 * 1024))
                                        .arg(total / (1024 * 1024)));
    } else {
        emit downloadProgress(received, -1);
        emit progressChanged(0, QStringLiteral("正在下载 Git (%1 MB)")
                                        .arg(received / (1024 * 1024)));
    }
}

void GitDownloader::onDownloadFinished()
{
    if (!currentReply_) {
        running_ = false;
        return;
    }

    if (cancelled_) {
        tempFile_->close();
        tempFile_->remove();
        tempFile_ = nullptr;
        currentReply_->deleteLater();
        currentReply_ = nullptr;
        running_ = false;
        emit finished(false, QStringLiteral("下载已取消"));
        return;
    }

    if (currentReply_->error() != QNetworkReply::NoError) {
        QString errMsg = currentReply_->errorString();
        tempFile_->close();
        tempFile_->remove();
        tempFile_ = nullptr;
        currentReply_->deleteLater();
        currentReply_ = nullptr;
        running_ = false;
        emit finished(false, QStringLiteral("下载失败: %1").arg(errMsg));
        return;
    }

    QByteArray fileData = currentReply_->readAll();
    tempFile_->write(fileData);
    tempFile_->close();

    currentReply_->deleteLater();
    currentReply_ = nullptr;

    emit progressChanged(100, QStringLiteral("下载完成，正在解压..."));

    QString zipPath = tempFile_->fileName();
    bool ok = extractZip(zipPath, extractDir_);

    tempFile_->remove();
    tempFile_ = nullptr;

    running_ = false;

    if (ok) {
        emit progressChanged(100, QStringLiteral("安装完成"));
        emit finished(true, QStringLiteral("Git 便携版安装成功"));
    } else {
        emit finished(false, QStringLiteral("解压失败，请检查磁盘空间或权限"));
    }
}

bool GitDownloader::extractZip(const QString& zipPath, const QString& destDir)
{
    QDir().mkpath(destDir);

    emit extractProgress(0, 1);

#ifdef _WIN32
    QProcess tarProc;
    tarProc.start(QStringLiteral("tar"), QStringList()
        << QStringLiteral("-xf")
        << zipPath
        << QStringLiteral("-C")
        << destDir);
    tarProc.waitForFinished(60000);

    if (tarProc.exitCode() == 0) {
        emit extractProgress(1, 1);
        emit progressChanged(100, QStringLiteral("解压完成"));
        return true;
    }

    QProcess psProc;
    psProc.start(QStringLiteral("powershell"), QStringList()
        << QStringLiteral("-Command")
        << QStringLiteral("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
            .arg(zipPath, destDir));
    psProc.waitForFinished(120000);

    if (psProc.exitCode() == 0) {
        emit extractProgress(1, 1);
        emit progressChanged(100, QStringLiteral("解压完成"));
        return true;
    }
#else
    QProcess tarProc;
    tarProc.start(QStringLiteral("tar"), QStringList()
        << QStringLiteral("-xf")
        << zipPath
        << QStringLiteral("-C")
        << destDir);
    tarProc.waitForFinished(60000);

    if (tarProc.exitCode() == 0) {
        emit extractProgress(1, 1);
        emit progressChanged(100, QStringLiteral("解压完成"));
        return true;
    }

    QProcess unzipProc;
    unzipProc.start(QStringLiteral("unzip"), QStringList()
        << QStringLiteral("-o")
        << zipPath
        << QStringLiteral("-d")
        << destDir);
    unzipProc.waitForFinished(60000);

    if (unzipProc.exitCode() == 0) {
        emit extractProgress(1, 1);
        emit progressChanged(100, QStringLiteral("解压完成"));
        return true;
    }
#endif

    return false;
}

} // namespace NeoInstaller
