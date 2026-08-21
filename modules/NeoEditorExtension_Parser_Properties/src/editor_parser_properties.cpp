#include "config_editor.h"
#include <IConfigEditorExtension.h>
#include <nlohmann/json.hpp>
#include <sstream>

namespace {

bool parse_property_line(const std::string& line,
                         std::string& out_key)
{
    auto start = line.find_first_not_of(" \t\r");
    if (start == std::string::npos) {
        return false;
    }
    if (line[start] == '#' || line[start] == '!') {
        return false;
    }
    size_t eq_pos = line.find('=');
    size_t col_pos = line.find(':');
    size_t sep_pos = std::string::npos;
    if (eq_pos != std::string::npos && col_pos != std::string::npos) {
        sep_pos = std::min(eq_pos, col_pos);
    } else if (eq_pos != std::string::npos) {
        sep_pos = eq_pos;
    } else if (col_pos != std::string::npos) {
        sep_pos = col_pos;
    }
    if (sep_pos == std::string::npos) {
        return false;
    }
    std::string key = line.substr(0, sep_pos);
    auto key_start = key.find_first_not_of(" \t");
    if (key_start == std::string::npos) {
        return false;
    }
    auto key_end = key.find_last_not_of(" \t");
    out_key = key.substr(key_start, key_end - key_start + 1);
    return true;
}

} // namespace

class PropertiesConfigEditorExtension : public NeoCore::IConfigEditorExtension {
public:
    std::string fileExtension() const override { return ".properties"; }

    QWidget* createEditor(QWidget* parent) override {
        return new ConfigEditor(parent);
    }

    void loadConfig(QWidget* editor, const std::string& remoteContent,
        const std::string& localContent,
        const nlohmann::json& syncRules) override
    {
        auto* ce = static_cast<ConfigEditor*>(editor);
        ce->setContent(remoteContent.empty() ? localContent : remoteContent);
        ce->setSyncMode(syncRules.value("policy", "full_sync"));

        QStringList keys, tracked;
        std::istringstream stream(remoteContent.empty() ? localContent : remoteContent);
        std::string line;
        while (std::getline(stream, line)) {
            std::string key;
            if (parse_property_line(line, key)) {
                keys << QString::fromStdString(key);
            }
        }
        if (syncRules.contains("tracked_keys") && syncRules["tracked_keys"].is_array())
            for (auto& k : syncRules["tracked_keys"])
                if (k.is_string()) tracked << QString::fromStdString(k.get<std::string>());

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
        }
        return rule;
    }

    std::string mergePreview(QWidget*) const override { return ""; }

    // 追踪键行列表: 逐行解析 key, 与 trackedKeys 精确匹配 (1-based)
    std::vector<int> trackedLines(const std::string& content,
        const std::vector<std::string>& trackedKeys) const override
    {
        std::vector<int> lines;
        std::istringstream stream(content);
        std::string line;
        int lineNo = 0;
        while (std::getline(stream, line)) {
            ++lineNo;
            std::string key;
            if (!parse_property_line(line, key)) continue;
            for (const auto& k : trackedKeys) {
                if (k == key) {
                    lines.push_back(lineNo);
                    break;
                }
            }
        }
        return lines;
    }
};

extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
    return new PropertiesConfigEditorExtension();
}
