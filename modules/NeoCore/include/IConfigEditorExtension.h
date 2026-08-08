#pragma once

#include <QWidget>
#include <QJsonObject>
#include <string>
#include <nlohmann/json.hpp>

namespace NeoCore {

class IConfigEditorExtension {
public:
    virtual ~IConfigEditorExtension() = default;

    virtual std::string fileExtension() const = 0;

    virtual QWidget* createEditor(QWidget* parent) = 0;

    virtual void loadConfig(QWidget* editor,
        const std::string& remoteContent,
        const std::string& localContent,
        const nlohmann::json& syncRules) = 0;

    virtual nlohmann::json saveSyncRules(QWidget* editor) const = 0;

    virtual std::string mergePreview(QWidget* editor) const = 0;
};

using CreateConfigEditorFunc = IConfigEditorExtension* (*)();

} // namespace NeoCore
