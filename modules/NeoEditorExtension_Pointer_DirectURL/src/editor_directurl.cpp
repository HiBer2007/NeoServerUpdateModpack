#include "editor_directurl.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

#include <IPointerEditorExtension.h>

DirectURLEditor::DirectURLEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* group = new QGroupBox("DirectURL 元数据", this);
    auto* form = new QFormLayout(group);

    urlEdit_ = new QLineEdit(group);
    urlEdit_->setPlaceholderText("https://cdn.example.com/files/mod.jar");
    form->addRow("下载 URL:", urlEdit_);

    filenameEdit_ = new QLineEdit(group);
    filenameEdit_->setPlaceholderText("sodium.jar (可选)");
    form->addRow("文件名:", filenameEdit_);

    layout->addWidget(group);
    layout->addStretch();
}

void DirectURLEditor::loadMetadata(const QJsonObject& metadata)
{
    urlEdit_->setText(metadata.value("url").toString());
    filenameEdit_->setText(metadata.value("filename").toString());
}

QJsonObject DirectURLEditor::saveMetadata() const
{
    QJsonObject meta;
    meta["url"] = urlEdit_->text().trimmed();
    if (!filenameEdit_->text().trimmed().isEmpty())
        meta["filename"] = filenameEdit_->text().trimmed();
    return meta;
}

class DirectURLEditorExtension : public NeoCore::IPointerEditorExtension {
public:
    std::string resolverType() const override { return "direct_url"; }

    QWidget* createEditor(QWidget* parent) override {
        return new DirectURLEditor(parent);
    }

    void loadMetadata(QWidget* editor, const QJsonObject& md) override {
        auto* e = static_cast<DirectURLEditor*>(editor);
        if (e) e->loadMetadata(md);
    }

    QJsonObject saveMetadata(QWidget* editor) const override {
        auto* e = static_cast<DirectURLEditor*>(editor);
        return e ? e->saveMetadata() : QJsonObject();
    }
};

extern "C" NeoCore::IPointerEditorExtension* CreateEditorExtension() {
    return new DirectURLEditorExtension();
}
