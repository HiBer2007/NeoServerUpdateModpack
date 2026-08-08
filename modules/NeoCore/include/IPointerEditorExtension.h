#pragma once

#include <QWidget>
#include <QJsonObject>
#include <string>

namespace NeoCore {

class IPointerEditorExtension {
public:
    virtual ~IPointerEditorExtension() = default;

    virtual std::string resolverType() const = 0;

    virtual QWidget* createEditor(QWidget* parent) = 0;

    virtual void loadMetadata(QWidget* editor,
        const QJsonObject& metadata) = 0;

    virtual QJsonObject saveMetadata(QWidget* editor) const = 0;
};

using CreateEditorExtensionFunc = IPointerEditorExtension* (*)();

} // namespace NeoCore
