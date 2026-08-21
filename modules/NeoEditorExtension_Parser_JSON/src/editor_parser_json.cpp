#include "config_editor.h"
#include <IConfigEditorExtension.h>
#include <nlohmann/json.hpp>
#include <sstream>

class JSONConfigEditorExtension : public NeoCore::IConfigEditorExtension {
public:
    std::string fileExtension() const override { return ".json"; }

    QWidget* createEditor(QWidget* parent) override {
        return new ConfigEditor(parent);
    }

    void loadConfig(QWidget* editor, const std::string& remoteContent,
        const std::string& localContent,
        const nlohmann::json& syncRules) override
    {
        auto* ce = static_cast<ConfigEditor*>(editor);
        ce->setContent(remoteContent.empty() ? localContent : remoteContent);

        std::string mode = syncRules.value("policy", "full_sync");
        ce->setSyncMode(mode);

        QStringList keys, tracked;
        try {
            auto j = nlohmann::json::parse(remoteContent.empty() ? localContent : remoteContent);
            for (auto it = j.begin(); it != j.end(); ++it)
                keys << QString::fromStdString(it.key());

            if (syncRules.contains("tracked_keys") && syncRules["tracked_keys"].is_array())
                for (auto& k : syncRules["tracked_keys"])
                    if (k.is_string()) tracked << QString::fromStdString(k.get<std::string>());
        } catch (...) {}

        ce->setKeys(keys, tracked);
    }

    nlohmann::json saveSyncRules(QWidget* editor) const override {
        auto* ce = static_cast<ConfigEditor*>(editor);
        nlohmann::json rule;
        rule["policy"] = ce->syncMode();
        if (rule["policy"] == "config_merge") {
            nlohmann::json keys = nlohmann::json::array();
            for (auto& k : ce->trackedKeys())
                keys.push_back(k.toStdString());
            rule["tracked_keys"] = keys;
        } else if (rule["policy"] == "line_by_line") {
            nlohmann::json lines = nlohmann::json::array();
            for (int l : ce->trackedLines())
                lines.push_back(l);
            rule["tracked_lines"] = lines;
        }
        return rule;
    }

    std::string mergePreview(QWidget*) const override { return ""; }

    // 追踪键行列表: 行内 "key": value (嵌套路径键取末段匹配, 1-based)
    std::vector<int> trackedLines(const std::string& content,
        const std::vector<std::string>& trackedKeys) const override
    {
        std::vector<int> lines;
        int lineNo = 0;
        for (auto& raw : QString::fromStdString(content).split('\n')) {
            ++lineNo;
            const QString line = raw;
            QRegularExpression keyRe("^\\s*\"([^\"]+)\"\\s*:");
            auto m = keyRe.match(line);
            if (!m.hasMatch()) continue;
            const QString localKey = m.captured(1);
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
    return new JSONConfigEditorExtension();
}
