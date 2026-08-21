#include "config_editor.h"
#include <IConfigEditorExtension.h>
#include <nlohmann/json.hpp>
#include <sstream>

class TXTConfigEditorExtension : public NeoCore::IConfigEditorExtension {
public:
    std::string fileExtension() const override { return ".txt"; }
    QWidget* createEditor(QWidget* parent) override {
        auto* ce = new ConfigEditor(parent);
        ce->setLineMode(true);
        return ce;
    }

    void loadConfig(QWidget* editor, const std::string& remoteContent,
        const std::string& localContent, const nlohmann::json& syncRules) override {
        auto* ce = static_cast<ConfigEditor*>(editor);
        ce->setContent(remoteContent.empty() ? localContent : remoteContent);
        ce->setSyncMode(syncRules.value("policy", "full_sync"));
        QList<int> lines;
        if (syncRules.contains("tracked_lines") && syncRules["tracked_lines"].is_array())
            for (auto& l : syncRules["tracked_lines"])
                if (l.is_number_integer()) lines << l.get<int>();
        ce->setTrackedLines(lines);
    }

    nlohmann::json saveSyncRules(QWidget* editor) const override {
        auto* ce = static_cast<ConfigEditor*>(editor);
        nlohmann::json rule; rule["policy"] = ce->syncMode();
        if (rule["policy"] == "line_by_line") {
            nlohmann::json lines = nlohmann::json::array();
            for (int l : ce->trackedLines()) lines.push_back(l);
            rule["tracked_lines"] = lines;
        }
        return rule;
    }
    std::string mergePreview(QWidget*) const override { return ""; }
};

extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
    return new TXTConfigEditorExtension();
}
