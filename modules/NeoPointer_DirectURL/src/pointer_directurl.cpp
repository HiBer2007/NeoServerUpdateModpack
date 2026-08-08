#include <IPluginPointer.h>
#include <plugin_log_sink.h>
#include <logger.h>
#include <nlohmann/json.hpp>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>

using json = nlohmann::json;

namespace {

class DirectUrlPointer : public NeoCore::IPluginPointer {
public:
    std::string name() const override { return "DirectURL"; }

    bool can_handle(const NeoCore::PointerInfo& ptr) const override {
        return ptr.resolver == "direct_url";
    }

    std::string resolve_url(const NeoCore::PointerInfo& ptr) override {
        try {
            if (!ptr.metadata.contains("url") ||
                !ptr.metadata["url"].is_string()) {
                CLogger::Error(
                    "DirectURL pointer: metadata missing 'url' field");
                return "";
            }

            std::string url = ptr.metadata["url"].get<std::string>();
            if (url.empty()) {
                CLogger::Error(
                    "DirectURL pointer: 'url' field is empty");
                return "";
            }

            return url;
        } catch (const std::exception& e) {
            CLogger::Error(
                "DirectUrlPointer::resolve_url exception: {}", e.what());
            return "";
        } catch (...) {
            CLogger::Error(
                "DirectUrlPointer::resolve_url unknown exception");
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
                    "DirectURL validate: file not found '{}'",
                    filepath.c_str());
                return false;
            }
            if (!file.open(QIODevice::ReadOnly)) {
                CLogger::Error(
                    "DirectURL validate: cannot open file '{}'",
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
                    "DirectURL validate: SHA-256 mismatch for '{}'",
                    filepath.c_str());
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            CLogger::Error(
                "DirectUrlPointer::validate exception: {}", e.what());
            return false;
        } catch (...) {
            CLogger::Error(
                "DirectUrlPointer::validate unknown exception");
            return false;
        }
    }
};

} // anonymous namespace

extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer() {
    return new DirectUrlPointer();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoPointer_DirectURL")

