#include "config_editor.h"
#include <IConfigEditorExtension.h>
#include <nlohmann/json.hpp>
#include <QRegularExpression>
#include <sstream>

static QStringList extractTomlKeys(const std::string& content) {
    QStringList keys;
    QString section;
    for (auto& line : QString::fromStdString(content).split('\n')) {
        QRegularExpression secRe("^\\s*\\[([^\\]]+)\\]");
        auto sm = secRe.match(line);
        if (sm.hasMatch()) { section = sm.captured(1) + "."; continue; }
        QRegularExpression keyRe("^\\s*([a-zA-Z_][a-zA-Z0-9_-]*)\\s*=");
        auto km = keyRe.match(line);
        if (km.hasMatch()) keys << (section + km.captured(1));
    }
    return keys;
}

class TOMLConfigEditorExtension : public NeoCore::IConfigEditorExtension {
public:
    std::string fileExtension() const override { return ".toml"; }
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
        ce->setKeys(extractTomlKeys(remoteContent.empty() ? localContent : remoteContent), tracked);
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

    // 追踪键行列表: 维护段上下文 ([section]), 键 = 段前缀 + 键名,
    // 行内键名或完整段路径任一匹配即计入 (1-based)
    std::vector<int> trackedLines(const std::string& content,
        const std::vector<std::string>& trackedKeys) const override
    {
        std::vector<int> lines;
        std::string section;
        int lineNo = 0;
        for (auto& raw : QString::fromStdString(content).split('\n')) {
            ++lineNo;
            QString line = raw;
            QRegularExpression secRe("^\\s*\\[([^\\]]+)\\]");
            auto sm = secRe.match(line);
            if (sm.hasMatch()) {
                section = sm.captured(1).toStdString() + ".";
                continue;
            }
            QRegularExpression keyRe("^\\s*([a-zA-Z_][a-zA-Z0-9_-]*)\\s*=");
            auto km = keyRe.match(line);
            if (!km.hasMatch()) continue;
            const std::string localKey = km.captured(1).toStdString();
            const std::string fullKey = section + localKey;
            for (const auto& k : trackedKeys) {
                if (k == localKey || k == fullKey) {
                    lines.push_back(lineNo);
                    break;
                }
            }
        }
        return lines;
    }
};

extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
    return new TOMLConfigEditorExtension();
}
