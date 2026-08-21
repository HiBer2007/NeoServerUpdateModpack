#include "config_editor.h"
#include <IConfigEditorExtension.h>
#include <nlohmann/json.hpp>
#include <QRegularExpression>
#include <sstream>

static QStringList extractYamlKeys(const std::string& content) {
    QStringList keys;
    for (auto& line : QString::fromStdString(content).split('\n')) {
        QRegularExpression re("^\\s*([a-zA-Z_][a-zA-Z0-9_.-]*)\\s*:");
        auto m = re.match(line);
        if (m.hasMatch()) keys << m.captured(1);
    }
    return keys;
}

class YAMLConfigEditorExtension : public NeoCore::IConfigEditorExtension {
public:
    std::string fileExtension() const override { return ".yaml"; }
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
        ce->setKeys(extractYamlKeys(remoteContent.empty() ? localContent : remoteContent), tracked);
    }

    nlohmann::json saveSyncRules(QWidget* editor) const override {
        auto* ce = static_cast<ConfigEditor*>(editor);
        nlohmann::json rule;
        rule["policy"] = ce->syncMode();
        if (rule["policy"] == "config_merge") {
            nlohmann::json keys = nlohmann::json::array();
            for (auto& k : ce->trackedKeys()) keys.push_back(k.toStdString());
            rule["tracked_keys"] = keys;
        }
        return rule;
    }

    std::string mergePreview(QWidget*) const override { return ""; }

    // 追踪键行列表: 缩进段上下文 (父键前缀 + "."), 完整路径或末段任一匹配 (1-based)
    std::vector<int> trackedLines(const std::string& content,
        const std::vector<std::string>& trackedKeys) const override
    {
        std::vector<int> lines;
        std::vector<QString> stack;
        int lineNo = 0;
        for (auto& raw : QString::fromStdString(content).split('\n')) {
            ++lineNo;
            const QString line = raw;
            QRegularExpression re("^(\\s*)([a-zA-Z_][a-zA-Z0-9_.-]*)\\s*:");
            auto m = re.match(line);
            if (!m.hasMatch()) continue;
            const int indent = m.captured(1).size();
            const QString localKey = m.captured(2);
            while (!stack.empty()
                && stack.size() > indent / 2 + 1) {
                stack.pop_back();
            }
            stack.push_back(localKey);
            QString fullKey = localKey;
            for (int i = 1; i < static_cast<int>(stack.size()); ++i) {
                fullKey = stack[i] + QLatin1Char('.') + fullKey;
            }
            for (const auto& k : trackedKeys) {
                const QString key = QString::fromStdString(k);
                if (key == fullKey || key == localKey
                    || (key.contains(QLatin1Char('.'))
                        && key.section(QLatin1Char('.'), -1) == localKey)) {
                    lines.push_back(lineNo);
                    break;
                }
            }
        }
        return lines;
    }
};

extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
    return new YAMLConfigEditorExtension();
}
