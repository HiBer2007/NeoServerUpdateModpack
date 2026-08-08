#include <IPluginPointer.h>
#include <plugin_log_sink.h>
#include <logger.h>
#include <nlohmann/json.hpp>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QEventLoop>
#include <QTimer>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>

using json = nlohmann::json;

namespace {

constexpr int kTimeoutMs = 30000;

QByteArray makeModrinthRequest(const QUrl& url, bool& timedOut) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "NeoServerUpdateModpack/1.0 (NeoServer)");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kTimeoutMs);

    QNetworkAccessManager mgr;
    QNetworkReply* reply = mgr.get(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        timedOut = true;
        reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(kTimeoutMs);

    loop.exec();

    if (timedOut) {
        reply->deleteLater();
        return QByteArray();
    }

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto errStr = reply->errorString().toStdString();
        CLogger::Error(
            "Modrinth API request failed: HTTP {} error: {}",
            httpStatus, errStr.c_str());
        reply->deleteLater();
        return QByteArray();
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        CLogger::Error("Modrinth API returned empty response");
    }

    return data;
}

std::string extractFileUrl(const json& response, const std::string& matchSha1) {
    if (!response.contains("files") || !response["files"].is_array()) {
        CLogger::Error(
            "Modrinth API response missing 'files' array");
        return "";
    }

    const auto& files = response["files"];
    if (files.empty()) {
        CLogger::Error(
            "Modrinth API response 'files' array is empty");
        return "";
    }

    if (!matchSha1.empty()) {
        for (const auto& f : files) {
            if (!f.contains("hashes")) continue;
            const auto& hashes = f["hashes"];
            if (!hashes.contains("sha1")) continue;
            try {
                if (hashes["sha1"].get<std::string>() == matchSha1) {
                    if (f.contains("url") && f["url"].is_string()) {
                        return f["url"].get<std::string>();
                    }
                }
            } catch (...) {
                continue;
            }
        }
        CLogger::Warn(
            "Modrinth reverse lookup: no file matched SHA-1 '{}', "
            "falling back to first file", matchSha1.c_str());
    }

    if (files[0].contains("url") && files[0]["url"].is_string()) {
        return files[0]["url"].get<std::string>();
    }
    return "";
}

class ModrinthPointer : public NeoCore::IPluginPointer {
public:
    std::string name() const override { return "Modrinth"; }

    bool can_handle(const NeoCore::PointerInfo& ptr) const override {
        return ptr.resolver == "modrinth";
    }

    std::string resolve_url(const NeoCore::PointerInfo& ptr) override {
        try {
            if (ptr.metadata.contains("project_id") &&
                ptr.metadata.contains("version_id") &&
                ptr.metadata["project_id"].is_string() &&
                ptr.metadata["version_id"].is_string()) {

                std::string projectId =
                    ptr.metadata["project_id"].get<std::string>();
                std::string versionId =
                    ptr.metadata["version_id"].get<std::string>();

                if (projectId.empty() || versionId.empty()) {
                    CLogger::Error(
                        "Modrinth pointer: project_id or version_id is empty");
                    return "";
                }

                QUrl apiUrl(QString::fromStdString(
                    "https://api.modrinth.com/v2/project/" +
                    projectId + "/version/" + versionId));

                bool timedOut = false;
                QByteArray data = makeModrinthRequest(apiUrl, timedOut);
                if (data.isEmpty()) return "";

                try {
                    auto response = json::parse(data.toStdString());
                    return extractFileUrl(response, "");
                } catch (const json::parse_error& e) {
                    CLogger::Error(
                        "Modrinth API JSON parse error: {}", e.what());
                    return "";
                }
            }
            else if (ptr.metadata.contains("sha1") &&
                     ptr.metadata["sha1"].is_string()) {

                std::string sha1Hash =
                    ptr.metadata["sha1"].get<std::string>();

                if (sha1Hash.empty()) {
                    CLogger::Error(
                        "Modrinth pointer: sha1 hash is empty");
                    return "";
                }

                QUrl apiUrl(QString::fromStdString(
                    "https://api.modrinth.com/v2/version_file/" +
                    sha1Hash));
                QUrlQuery query;
                query.addQueryItem("algorithm", "sha1");
                apiUrl.setQuery(query);

                bool timedOut = false;
                QByteArray data = makeModrinthRequest(apiUrl, timedOut);
                if (data.isEmpty()) return "";

                try {
                    auto response = json::parse(data.toStdString());
                    return extractFileUrl(response, sha1Hash);
                } catch (const json::parse_error& e) {
                    CLogger::Error(
                        "Modrinth reverse lookup JSON parse error: {}",
                        e.what());
                    return "";
                }
            }
            else {
                CLogger::Error(
                    "Modrinth pointer: metadata requires "
                    "project_id+version_id or sha1");
                return "";
            }
        } catch (const std::exception& e) {
            CLogger::Error(
                "ModrinthPointer::resolve_url exception: {}", e.what());
            return "";
        } catch (...) {
            CLogger::Error(
                "ModrinthPointer::resolve_url unknown exception");
            return "";
        }
    }

    bool validate(const std::string& filepath,
                  const std::string& expected_sha256) override
    {
        try {
            QFile file(QString::fromStdString(filepath));
            if (!file.exists()) {
                CLogger::Error(
                    "Modrinth validate: file not found '{}'",
                    filepath.c_str());
                return false;
            }
            if (!file.open(QIODevice::ReadOnly)) {
                CLogger::Error(
                    "Modrinth validate: cannot open file '{}'",
                    filepath.c_str());
                return false;
            }

            QCryptographicHash hasher(QCryptographicHash::Sha256);
            static constexpr qint64 kBufferSize = 65536;
            char buffer[kBufferSize];
            qint64 bytesRead;
            while ((bytesRead = file.read(buffer, kBufferSize)) > 0) {
                hasher.addData(buffer, bytesRead);
            }
            file.close();

            QString computed = QString::fromLatin1(
                hasher.result().toHex()).toLower();
            QString expected = QString::fromStdString(
                expected_sha256).toLower();

            if (computed != expected) {
                CLogger::Error(
                    "Modrinth validate: SHA-256 mismatch for '{}'",
                    filepath.c_str());
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            CLogger::Error(
                "ModrinthPointer::validate exception: {}", e.what());
            return false;
        } catch (...) {
            CLogger::Error(
                "ModrinthPointer::validate unknown exception");
            return false;
        }
    }
};

} // anonymous namespace

extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer() {
    return new ModrinthPointer();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoPointer_Modrinth")

