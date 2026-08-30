#include "file_content_editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QFileInfo>
#include <QShortcut>
#include <QKeySequence>

#include "code_editor.h"
#include "gitignore_highlighter.h"

namespace HiBerGUI {

FileContentEditor::FileContentEditor(QWidget* parent)
    : QWidget(parent)
    , gitIgnoreHl_(new GitIgnoreHighlighter)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(8);

    auto* title = new QLabel(QString::fromUtf8("\u6587\u4ef6\u5185\u5bb9\u7f16\u8f91\u5668"), this);
    title->setStyleSheet(QStringLiteral(
        "font-size: 15px; font-weight: bold; color: #e8eaed;"));
    lay->addWidget(title);

    pathLabel_ = new QLabel(this);
    pathLabel_->setStyleSheet(QStringLiteral("color: #9aa0a8; font-size: 12px;"));
    pathLabel_->setWordWrap(true);
    lay->addWidget(pathLabel_);

    stateLabel_ = new QLabel(this);
    stateLabel_->setStyleSheet(QStringLiteral("color: #4dd0e1; font-size: 11px;"));
    stateLabel_->setWordWrap(true);
    lay->addWidget(stateLabel_);

    editor_ = createCodeEditor(CodeEditorKind::Qt, this);
    if (editor_) {
        editor_->setDarkMode(palette().color(QPalette::Window).lightness() < 128);
        lay->addWidget(editor_->widget(), 1);
    }

    auto* row = new QHBoxLayout();
    saveBtn_ = new QPushButton(QString::fromUtf8("\u4fdd\u5b58\u5185\u5bb9"), this);
    saveBtn_->setMinimumHeight(32);
    row->addWidget(saveBtn_);
    row->addStretch(1);
    lay->addLayout(row);

    connect(saveBtn_, &QPushButton::clicked, this, [this]() {
        emit contentSaveRequested(relPath_, editor_->toPlainText(), inherited_);
    });
    if (editor_) {
        auto* saveShortcut = new QShortcut(QKeySequence::Save, editor_->widget());
        connect(saveShortcut, &QShortcut::activated, this, [this]() {
            emit contentSaveRequested(relPath_, editor_->toPlainText(), inherited_);
        });
    }
}

void FileContentEditor::loadContent(const QString& relPath,
    const QString& absPath, bool inherited, const QString& sourceAbs)
{
    relPath_ = relPath;
    inherited_ = inherited;

    pathLabel_->setText(QString::fromUtf8("\u76f8\u5bf9\u8def\u5f84: %1").arg(relPath));

    QString content;
    const QString readPath = (!sourceAbs.isEmpty()) ? sourceAbs : absPath;
    QFile f(readPath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(f.readAll());
        f.close();
    }

    if (inherited) {
        stateLabel_->setText(
            QString::fromUtf8(
                "\u2718 \u7ee7\u627f\u81ea\u7236\u5206\u652f\uff0c\u672c\u5206\u652f\u65e0\u5b9e\u4f53\u3002"
                "\u4fdd\u5b58\u5c06\u5728\u672c\u5206\u652f\u521b\u5efa\u8986\u76d6\u5b9e\u4f53"
                "(override \u6807\u8bb0)\u3002"));
    } else {
        stateLabel_->setText(
            QString::fromUtf8("\u2705 \u672c\u5206\u652f\u5b9e\u4f53\uff0c\u4fdd\u5b58\u76f4\u63a5\u5199\u56de\u3002"));
    }

    if (editor_) {
        // 按扩展名选择高亮语言 (.gitignore 无扩展名, 按文件名识别 + 专用高亮)
        const QString fileName = QFileInfo(relPath).fileName();
        const QString suffix = QFileInfo(relPath).suffix().toLower();
        QString lang = QStringLiteral("txt");
        if (fileName == QLatin1String(".gitignore")) {
            lang = QStringLiteral("gitignore");
            editor_->registerHighlighter(gitIgnoreHl_);
        } else if (suffix == QLatin1String("json")) {
            lang = QStringLiteral("json");
            editor_->registerHighlighter(nullptr);
        } else if (suffix == QLatin1String("yaml")
            || suffix == QLatin1String("yml")) {
            lang = QStringLiteral("yaml");
            editor_->registerHighlighter(nullptr);
        } else if (suffix == QLatin1String("toml")) {
            lang = QStringLiteral("toml");
            editor_->registerHighlighter(nullptr);
        } else if (suffix == QLatin1String("snbt")) {
            lang = QStringLiteral("snbt");
            editor_->registerHighlighter(nullptr);
        } else if (suffix == QLatin1String("properties")
            || suffix == QLatin1String("ini")
            || suffix == QLatin1String("cfg")) {
            lang = QStringLiteral("properties");
            editor_->registerHighlighter(nullptr);
        } else if (suffix == QLatin1String("txt")
            || suffix == QLatin1String("log")) {
            lang = QStringLiteral("txt");
            editor_->registerHighlighter(nullptr);
        } else {
            editor_->registerHighlighter(nullptr);
        }
        editor_->setLanguage(lang);
        editor_->setPlainText(content);
        editor_->setReadOnly(false);
        editor_->scrollToTop();
    }
}

FileContentEditor::~FileContentEditor()
{
    delete gitIgnoreHl_;
}

} // namespace HiBerGUI

#include "file_content_editor.moc"