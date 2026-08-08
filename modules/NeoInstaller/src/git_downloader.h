#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QProcess>

namespace NeoInstaller {

enum class GitVariant { MinGit, PortableGit };

class GitDownloader : public QObject {
    Q_OBJECT
public:
    explicit GitDownloader(QObject* parent = nullptr);

    void startDownload(GitVariant variant, const QString& extractDir);
    void cancel();
    bool isRunning() const;

    static bool isSystemGitAvailable();
    static QString systemGitPath();
    static QString systemGitVersion();
    static QString fetchLatestGitUrl(GitVariant variant);

signals:
    void progressChanged(int percent, const QString& status);
    void downloadStarted(const QString& url, qint64 totalBytes);
    void downloadProgress(qint64 received, qint64 total);
    void extractProgress(int current, int total);
    void finished(bool success, const QString& message);

private slots:
    void onMetaDataChanged();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();

private:
    QNetworkAccessManager* network_;
    QNetworkReply* currentReply_ = nullptr;
    QFile* tempFile_ = nullptr;
    QString extractDir_;
    GitVariant variant_ = GitVariant::MinGit;
    bool cancelled_ = false;
    bool running_ = false;

    bool extractZip(const QString& zipPath, const QString& destDir);
};

} // namespace NeoInstaller
