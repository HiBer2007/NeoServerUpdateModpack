#include "config_editor.h"
#include <IConfigEditorExtension.h>
#include <nlohmann/json.hpp>
#include <QRegularExpression>
#include <sstream>

static QStringList extractSnbtKeys(const std::string& content) {
    QStringList keys;
    // SNBT: strip comments, then parse as JSON or use regex
    QString text = QString::fromStdString(content);
    text.remove(QRegularExpression("#[^\n]*")); // remove comments
    try {
        std::string cleaned = text.toStdString();
        auto j = nlohmann::json::parse(cleaned, nullptr, false);
        if (!j.is_discarded()) {
            for (auto it = j.begin(); it != j.end(); ++it)
                keys << QString::fromStdString(it.key());
            return keys;
        }
    } catch (...) {}
    // fallback regex
    for (auto& line : text.split('\n')) {
        QRegularExpression re("^\\s*([a-zA-Z_][a-zA-Z0-9_-]*)\\s*:");
        auto m = re.match(line);
        if (m.hasMatch()) keys << m.captured(1);
    }
    return keys;
}

class SNBTConfigEditorExtension : public NeoCore::IConfigEditorExtension {
public:
    std::string fileExtension() const override { return ".snbt"; }
    QWidget* createEditor(QWidget* parent) override { return new ConfigEditor(parent); }

    void loadConfig(QWidget* editor, const std::string& remoteContent,
        const std::string& localContent, const nlohmann::json& syncRules) override {
        auto* ce = static_cast<ConfigEditor*>(editor);
        ce->setContent(remoteContent.empty() ? localContent : remoteContent);
        ce->setSyncMode(syncRules.value("policy", "full_sync"));
        QStringList tracked;
        if (syncRules.contains("tracked_keys") && syncRules["tracked_keys"].is_array())
            for (auto& k : syncRules["tracked_keys"])
                if (k.is_string()) tracked << QString::fromStdString(k.get<std::string>());
        ce->setKeys(extractSnbtKeys(remoteContent.empty() ? localContent : remoteContent), tracked);
    }

    nlohmann::json saveSyncRules(QWidget* editor) const override {
        auto* ce = static_cast<ConfigEditor*>(editor);
        nlohmann::json rule; rule["policy"] = ce->syncMode();
        if (rule["policy"] == "config_merge") {
            nlohmann::json keys = nlohmann::json::array();
            for (auto& k : ce->trackedKeys()) keys.push_back(k.toStdString());
            rule["tracked_keys"] = keys;
        }
        return rule;
    }
    std::string mergePreview(QWidget*) const override { return ""; }

    // 追踪键行列表: 引号键 "key": 或裸键 key: (嵌套路径取末段, 1-based)
    std::vector<int> trackedLines(const std::string& content,
        const std::vector<std::string>& trackedKeys) const override
    {
        std::vector<int> lines;
        int lineNo = 0;
        for (auto& raw : QString::fromStdString(content).split('\n')) {
            ++lineNo;
            const QString line = raw;
            QRegularExpression qRe("^\\s*\"([^\"]+)\"\\s*:");
            QRegularExpression bRe("^\\s*([a-zA-Z_][a-zA-Z0-9_-]*)\\s*:");
            QString localKey;
            auto qm = qRe.match(line);
            if (qm.hasMatch()) {
                localKey = qm.captured(1);
            } else {
                auto bm = bRe.match(line);
                if (bm.hasMatch()) localKey = bm.captured(1);
            }
            if (localKey.isEmpty()) continue;
            for (const auto& k : trackedKeys) {
                const QString key = QString::fromStdString(k);
                const QString last = key.contains(QLatin1Char('.'))
                    ? key.section(QLatin1Char('.'), -1)
                    : key;
                if (localKey == key || localKey == last) {
                    lines.push_back(lineNo);
                    break;
                }
            }
        }
        return lines;
    }
};

extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
    return new SNBTConfigEditorExtension();
}
