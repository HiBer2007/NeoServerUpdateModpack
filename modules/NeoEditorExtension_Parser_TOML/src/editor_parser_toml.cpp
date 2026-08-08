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
};

extern "C" NeoCore::IConfigEditorExtension* CreateConfigEditor() {
    return new TOMLConfigEditorExtension();
}
