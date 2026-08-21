#include "merge_preview_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

namespace HiBerGUI {

MergePreviewDialog::MergePreviewDialog(CodeEditorKind kind, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("merge \u9884\u89c8"));
    resize(680, 520);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    infoLabel_ = new QLabel(this);
    infoLabel_->setWordWrap(true);
    infoLabel_->setStyleSheet(QStringLiteral("color: #9aa0a8; font-size: 12px;"));
    lay->addWidget(infoLabel_);

    editor_ = createCodeEditor(kind, this);
    if (editor_) {
        editor_->setReadOnly(true);
        lay->addWidget(editor_->widget(), 1);
    } else {
        auto* fail = new QLabel(QString::fromUtf8(
            "\u7f16\u8f91\u5668\u521d\u59cb\u5316\u5931\u8d25\uff08\u8be5\u7248\u672c\u672a\u6ce8\u518c\u5de5\u5382\uff09\u3002"), this);
        lay->addWidget(fail, 1);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(QString::fromUtf8("\u5173\u95ed"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(buttons);
}

MergePreviewDialog::~MergePreviewDialog() = default;

void MergePreviewDialog::setContent(const QString& content, const QString& info,
    const QString& langId, const QVector<RegionHighlight>& highlights)
{
    infoLabel_->setText(info);
    if (editor_) {
        editor_->setLanguage(langId);
        editor_->setPlainText(content);
        editor_->setRegionHighlights(highlights);
    }
}

} // namespace HiBerGUI

#include "merge_preview_dialog.moc"
