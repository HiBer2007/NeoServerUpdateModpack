#include "gitignore_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QListWidget>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QShortcut>
#include <QKeySequence>
#include <QPalette>
#include <QMenu>
#include <QLineEdit>
#include <QSet>
#include <QDir>
#include <QDirIterator>
#include <QDialogButtonBox>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QStyle>

#include "code_editor.h"
#include "gitignore_highlighter.h"
#include "gitignore_markup.h"

namespace GitIgnoreMarkup {

namespace {
// 图形化列表: 组内行缩进 + 组背景色块 (组边界视觉隔离)
const QString kGroupIndent = QStringLiteral("    ");
const QString kGroupBg = QStringLiteral("#1e3a3f");
// 组首显示符号 (替换 #{ 标记文本)
const QString kGroupBeginMark = QStringLiteral("\u25b6 ");   // ▶
}

// 组尾渲染为水平分割线 (组间隔离, 避免多组连续堆叠);
// 组首渲染为带符号标题 (替换 #{ 标记文本), 其余行常规绘制。
class GitIgnoreListDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        const auto kind = static_cast<GitIgnoreMarkup::LineKind>(
            index.data(Qt::UserRole).toInt());
        if (kind != GitIgnoreMarkup::LineKind::GroupEnd) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        // 组尾: 水平分割线 (不显示 #} 文本)
        painter->save();
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(option.rect, option.palette.highlight());
        } else {
            painter->fillRect(option.rect, QColor(kGroupBg));
        }
        const QRect r = option.rect.adjusted(10, 0, -10, 0);
        const int y = r.center().y();
        painter->setPen(QPen(QColor(QStringLiteral("#3f6a70")), 1));
        painter->drawLine(r.left(), y, r.right(), y);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        const auto kind = static_cast<GitIgnoreMarkup::LineKind>(
            index.data(Qt::UserRole).toInt());
        if (kind == GitIgnoreMarkup::LineKind::GroupEnd) {
            return QSize(option.rect.width(), 14);
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }
};

GitIgnoreDialog::GitIgnoreDialog(const QString& repoRoot, int initialTab,
    QWidget* parent)
    : QDialog(parent)
    , repoRoot_(repoRoot)
{
    absPath_ = repoRoot_ + QStringLiteral("/.gitignore");

    setWindowTitle(QString::fromUtf8("\u7f16\u8f91 .gitignore"));
    resize(760, 580);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);

    auto* title = new QLabel(QString::fromUtf8(
        "\u5b58\u6863\u6839\u76ee\u5f55\u7684\u5ffd\u7565\u89c4\u5219: %1")
        .arg(absPath_), this);
    title->setStyleSheet(QStringLiteral(
        "color: #9aa0a8; font-size: 12px;"));
    title->setWordWrap(true);
    outer->addWidget(title);

    tabs_ = new QTabWidget(this);

    // ---- Tab 1: 图形化编辑 ----
    auto* guiPage = new QWidget(tabs_);
    auto* guiLay = new QVBoxLayout(guiPage);
    guiLay->setContentsMargins(8, 8, 8, 8);
    guiLay->setSpacing(6);

    auto* hint = new QLabel(QString::fromUtf8(
        "\u89c4\u5219\u52fe\u9009\u542f\u7528 / \u53d6\u6d88\u52fe\u9009\u7981\u7528 "
        "(\u4fdd\u5b58\u4e3a #! \u524d\u7f00\uff0c\u53ef\u91cd\u65b0\u52fe\u9009\u6062\u590d); "
        "#> \u4e0b\u4e00\u884c\u6807\u6ce8 / #{...}#} \u4e0b\u4e00\u7ec4\u6807\u6ce8 "
        "(\u9996\u5c3e\u6807\u8bb0); # \u666e\u901a\u6ce8\u91ca; \u7a7a\u884c\u5ffd\u7565\u3002"),
        guiPage);
    hint->setStyleSheet(QStringLiteral("color: #8a9099; font-size: 11px;"));
    hint->setWordWrap(true);
    guiLay->addWidget(hint);

    ruleList_ = new QListWidget(guiPage);
    ruleList_->setItemDelegate(new GitIgnoreListDelegate(ruleList_));
    guiLay->addWidget(ruleList_, 1);

    auto* inputRow = new QHBoxLayout;
    inputEdit_ = new QLineEdit(guiPage);
    inputEdit_->setPlaceholderText(QString::fromUtf8(
        "\u624b\u8f93\u89c4\u5219/\u8def\u5f84 (\u652f\u6301\u901a\u914d\u7b26), "
        "\u6216\u4ece\u53f3\u4fa7\u5feb\u6377\u586b\u5145..."));
    inputRow->addWidget(inputEdit_, 1);
    // 通配符快捷插入: tooltip 说明各通配符用途
    struct Wildcard { const char* label; const char* token; const char* tip; };
    static const Wildcard wildcards[] = {
        { "**", "**", "\u8de8\u591a\u7ea7\u76ee\u5f55\u5339\u914d\u4efb\u610f\u8def\u5f84" },
        { "*", "*", "\u5339\u914d\u4efb\u610f\u5b57\u7b26 (\u4e0d\u542b /)" },
        { "?", "?", "\u5339\u914d\u5355\u4e2a\u4efb\u610f\u5b57\u7b26" },
        { "[ab]", "[ab]", "\u5b57\u7b26\u96c6: \u5339\u914d a \u6216 b \u4efb\u4e00" },
        { "!", "!", "\u5426\u5b9a\u524d\u7f00: \u91cd\u65b0\u5305\u542b\u88ab\u5ffd\u7565\u7684\u8def\u5f84" },
        { "/", "/", "\u8def\u5f84\u5206\u9694\u7b26 / \u7ed3\u5c3e\u6807\u8bb0\u76ee\u5f55" },
        { "\\#", "\\#", "\u8f6c\u4e49 #: \u5339\u914d\u5b57\u9762 # \u800c\u975e\u6ce8\u91ca" },
        { "\\!", "\\!", "\u8f6c\u4e49 !: \u5339\u914d\u5b57\u9762 ! \u800c\u975e\u5426\u5b9a" },
    };
    for (const auto& w : wildcards) {
        auto* btn = new QPushButton(QString::fromLatin1(w.label), guiPage);
        btn->setFixedWidth(40);
        btn->setToolTip(QString::fromUtf8(w.tip));
        connect(btn, &QPushButton::clicked, this, [this, w]() {
            onInsertWildcard(QString::fromLatin1(w.token));
        });
        inputRow->addWidget(btn);
    }
    guiLay->addLayout(inputRow);

    auto* btnRow = new QHBoxLayout;
    auto* addBtn = new QPushButton(QString::fromUtf8("\u6dfb\u52a0\u89c4\u5219"), guiPage);
    auto* labelBtn = new QPushButton(QString::fromUtf8("\u6807\u6ce8\u4e0b\u4e00\u884c"), guiPage);
    labelBtn->setToolTip(QString::fromUtf8(
        "\u63d2\u5165 #> \u6807\u6ce8, \u6807\u6ce8\u7d27\u8ddf\u5176\u4e0b\u7684\u89c4\u5219\u884c"));
    auto* groupBtn = new QPushButton(QString::fromUtf8("\u4e0b\u4e00\u7ec4\u6807\u6ce8"), guiPage);
    auto* groupMenu = new QMenu(groupBtn);
    groupBtn->setMenu(groupMenu);
    auto* beginAct = groupMenu->addAction(QString::fromUtf8("\u7ec4\u5f00\u59cb (#{...)..."));
    auto* endAct = groupMenu->addAction(QString::fromUtf8("\u7ec4\u7ed3\u675f (#})"));
    auto* presetsBtn = new QPushButton(QString::fromUtf8("\u5e38\u7528\u9884\u8bbe"), guiPage);
    presetsBtn->setMenu(new QMenu(presetsBtn));
    auto* summarizeBtn = new QPushButton(QString::fromUtf8("\u81ea\u52a8\u603b\u7ed3..."), guiPage);
    auto* removeBtn = new QPushButton(QString::fromUtf8("\u5220\u9664\u6240\u9009"), guiPage);
    auto* upBtn = new QPushButton(QString::fromUtf8("\u4e0a\u79fb"), guiPage);
    auto* downBtn = new QPushButton(QString::fromUtf8("\u4e0b\u79fb"), guiPage);
    auto* guiSaveBtn = new QPushButton(QString::fromUtf8("\u4fdd\u5b58\u56fe\u5f62\u5316\u7ed3\u679c"), guiPage);
    guiSaveBtn->setMinimumHeight(30);
    btnRow->addWidget(addBtn);
    btnRow->addWidget(labelBtn);
    btnRow->addWidget(groupBtn);
    btnRow->addWidget(presetsBtn);
    btnRow->addWidget(summarizeBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addWidget(upBtn);
    btnRow->addWidget(downBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(guiSaveBtn);
    guiLay->addLayout(btnRow);

    tabs_->addTab(guiPage, QString::fromUtf8("\u56fe\u5f62\u5316\u7f16\u8f91"));

    // ---- Tab 2: 直接编辑 (HiBerGUI CodeEditor, Qt 版 + .gitignore 高亮) ----
    auto* codePage = new QWidget(tabs_);
    auto* codeLay = new QVBoxLayout(codePage);
    codeLay->setContentsMargins(8, 8, 8, 8);
    codeLay->setSpacing(8);

    auto* codeHint = new QLabel(QString::fromUtf8(
        "\u76f4\u63a5\u7f16\u8f91 .gitignore \u539f\u6587 (Ctrl+S \u4fdd\u5b58)\uff1b"
        "# \u6ce8\u91ca / #! \u4e34\u65f6\u53d6\u6d88 / #> \u4e0b\u4e00\u884c\u6807\u6ce8 / "
        "#{...}#} \u4e0b\u4e00\u7ec4\u6807\u6ce8 / ! \u5426\u5b9a / \u901a\u914d\u7b26\u9ad8\u4eae\u3002"),
        codePage);
    codeHint->setStyleSheet(QStringLiteral("color: #8a9099; font-size: 11px;"));
    codeHint->setWordWrap(true);
    codeLay->addWidget(codeHint);

    highlighter_ = new HiBerGUI::GitIgnoreHighlighter;
    codeEditor_ = HiBerGUI::createCodeEditor(HiBerGUI::CodeEditorKind::Qt,
        codePage);
    if (codeEditor_) {
        codeEditor_->setLanguage(QStringLiteral("gitignore"));
        codeEditor_->registerHighlighter(highlighter_);
        codeEditor_->setDarkMode(
            palette().color(QPalette::Window).lightness() < 128);
        codeLay->addWidget(codeEditor_->widget(), 1);
    }

    auto* codeBtnRow = new QHBoxLayout;
    auto* codeSaveBtn = new QPushButton(QString::fromUtf8("\u4fdd\u5b58\u7f16\u8f91\u7ed3\u679c"), codePage);
    codeSaveBtn->setMinimumHeight(30);
    codeBtnRow->addWidget(codeSaveBtn);
    codeBtnRow->addStretch(1);
    codeLay->addLayout(codeBtnRow);

    tabs_->addTab(codePage, QString::fromUtf8("\u76f4\u63a5\u7f16\u8f91"));

    outer->addWidget(tabs_, 1);

    auto* closeRow = new QHBoxLayout;
    closeRow->addStretch(1);
    auto* closeBtn = new QPushButton(QString::fromUtf8("\u5173\u95ed"), this);
    closeBtn->setMinimumHeight(30);
    closeRow->addWidget(closeBtn);
    outer->addLayout(closeRow);

    // ---- 预设菜单 ----
    static const char* presets[] = {
        "build/", "dist/", "out/", "target/", "bin/", "obj/",
        "node_modules/", ".gradle/", ".idea/", ".vscode/", ".cache/",
        "*.log", "*.tmp", "*.bak", "*.cache", "*.class", "*.pyc",
        ".DS_Store", "Thumbs.db", ".NSUM/",
    };
    for (const char* p : presets) {
        QString pattern = QString::fromLatin1(p);
        auto* act = presetsBtn->menu()->addAction(pattern);
        connect(act, &QAction::triggered, this, [this, pattern]() {
            onPreset(pattern);
        });
    }

    connect(addBtn, &QPushButton::clicked, this, &GitIgnoreDialog::onAddRule);
    connect(labelBtn, &QPushButton::clicked, this, &GitIgnoreDialog::onInsertLineLabel);
    connect(beginAct, &QAction::triggered, this, &GitIgnoreDialog::onInsertGroupBegin);
    connect(endAct, &QAction::triggered, this, &GitIgnoreDialog::onInsertGroupEnd);
    connect(summarizeBtn, &QPushButton::clicked, this, &GitIgnoreDialog::onAutoSummarize);
    connect(removeBtn, &QPushButton::clicked, this, &GitIgnoreDialog::onRemoveRule);
    connect(upBtn, &QPushButton::clicked, this, &GitIgnoreDialog::onMoveUp);
    connect(downBtn, &QPushButton::clicked, this, &GitIgnoreDialog::onMoveDown);
    connect(guiSaveBtn, &QPushButton::clicked, this, &GitIgnoreDialog::onSaveGui);
    connect(codeSaveBtn, &QPushButton::clicked, this, &GitIgnoreDialog::onSaveCode);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(inputEdit_, &QLineEdit::returnPressed, this, &GitIgnoreDialog::onAddRule);
    if (codeEditor_) {
        auto* shortcut = new QShortcut(QKeySequence::Save, codeEditor_->widget());
        connect(shortcut, &QShortcut::activated, this, &GitIgnoreDialog::onSaveCode);
    }

    tabs_->setCurrentIndex(initialTab);
    loadFromDisk();
}

GitIgnoreDialog::~GitIgnoreDialog()
{
    delete highlighter_;
}

void GitIgnoreDialog::loadFromDisk()
{
    QString content;
    QFile f(absPath_);
    if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(f.readAll());
        f.close();
    }
    rebuildGuiList(content);
    syncCodeEditor();
}

void GitIgnoreDialog::rebuildGuiList(const QString& content)
{
    ruleList_->clear();
    const auto lines = GitIgnoreMarkup::parseLines(content);
    int groupDepth = 0;
    for (const auto& l : lines) {
        auto* item = new QListWidgetItem(ruleList_);
        item->setData(Qt::UserRole, static_cast<int>(l.kind));
        const bool inGroup = groupDepth > 0;
        switch (l.kind) {
        case GitIgnoreMarkup::LineKind::Comment:
            item->setText((inGroup ? kGroupIndent : QString()) + l.text);
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(QColor(QStringLiteral("#8a9099")));
            if (inGroup) {
                item->setBackground(QBrush(QColor(kGroupBg)));
            }
            break;
        case GitIgnoreMarkup::LineKind::LineLabel:
            item->setText((inGroup ? kGroupIndent : QString())
                + QStringLiteral(">>> ") + l.text);
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(QColor(QStringLiteral("#e2a03f")));
            if (inGroup) {
                item->setBackground(QBrush(QColor(kGroupBg)));
            }
            break;
        case GitIgnoreMarkup::LineKind::GroupBegin:
            // 组首标记: 独立行 + 组底色块 (组边界), 显示标记替换为 ▶ 符号
            item->setText(kGroupBeginMark + l.text);
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(QColor(QStringLiteral("#4dd0e1")));
            item->setBackground(QBrush(QColor(kGroupBg)));
            {
                QFont f = item->font();
                f.setBold(true);
                item->setFont(f);
            }
            ++groupDepth;
            break;
        case GitIgnoreMarkup::LineKind::GroupEnd:
            // 组尾标记: 文本置空, 由 GitIgnoreListDelegate 渲染为水平分割线
            item->setText(QString());
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            if (groupDepth > 0) {
                --groupDepth;
            }
            break;
        case GitIgnoreMarkup::LineKind::DisabledRule:
            item->setText((inGroup ? kGroupIndent : QString()) + l.text);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            item->setForeground(QColor(QStringLiteral("#9aa0a8")));
            if (inGroup) {
                item->setBackground(QBrush(QColor(kGroupBg)));
            }
            break;
        case GitIgnoreMarkup::LineKind::Rule:
            item->setText((inGroup ? kGroupIndent : QString()) + l.text);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
            if (inGroup) {
                item->setBackground(QBrush(QColor(kGroupBg)));
            }
            break;
        }
    }
}

// 图形化行显示前缀还原为纯内容 (剥离组缩进与 ▶/>>> 显示标记)
QString GitIgnoreDialog::stripDisplayPrefix(const QString& shown) const
{
    QString t = shown;
    if (t.startsWith(kGroupIndent)) {
        t = t.mid(kGroupIndent.size());
    }
    if (t.startsWith(kGroupBeginMark)) {
        t = t.mid(kGroupBeginMark.size());
    }
    return t;
}

QString GitIgnoreDialog::guiContent() const
{
    QVector<GitIgnoreMarkup::Line> lines;
    for (int i = 0; i < ruleList_->count(); ++i) {
        auto* item = ruleList_->item(i);
        const auto kind = static_cast<GitIgnoreMarkup::LineKind>(
            item->data(Qt::UserRole).toInt());
        GitIgnoreMarkup::Line l;
        l.kind = kind;
        l.text = stripDisplayPrefix(item->text());
        switch (kind) {
        case GitIgnoreMarkup::LineKind::Comment:
            break;
        case GitIgnoreMarkup::LineKind::LineLabel:
            if (l.text.startsWith(QLatin1String(">>> "))) {
                l.text = l.text.mid(4);
            }
            break;
        case GitIgnoreMarkup::LineKind::GroupBegin:
            break;
        case GitIgnoreMarkup::LineKind::GroupEnd:
            break;
        case GitIgnoreMarkup::LineKind::DisabledRule:
        case GitIgnoreMarkup::LineKind::Rule:
            l.checked = (item->checkState() == Qt::Checked);
            break;
        }
        lines.append(l);
    }
    return GitIgnoreMarkup::serialize(lines);
}

void GitIgnoreDialog::syncCodeEditor()
{
    if (!codeEditor_) return;
    codeEditor_->setPlainText(guiContent());
    codeEditor_->scrollToTop();
}

void GitIgnoreDialog::onAddRule()
{
    const QString input = inputEdit_->text().trimmed();
    if (input.isEmpty()) return;
    // 以 # 开头按普通注释添加 (内容去前缀, 序列化时统一补 "# ")
    if (input.startsWith(QLatin1Char('#'))) {
        QString text = input.mid(1);
        if (text.startsWith(QLatin1Char(' '))) {
            text = text.mid(1);
        }
        auto* item = new QListWidgetItem(text, ruleList_);
        item->setData(Qt::UserRole,
            static_cast<int>(GitIgnoreMarkup::LineKind::Comment));
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        item->setForeground(QColor(QStringLiteral("#8a9099")));
        ruleList_->setCurrentItem(item);
    } else {
        addRuleRow(input, true);
    }
    inputEdit_->clear();
    inputEdit_->setFocus();
    ruleList_->scrollToBottom();
}

void GitIgnoreDialog::addRuleRow(const QString& pattern, bool checked)
{
    auto* item = new QListWidgetItem(pattern, ruleList_);
    item->setData(Qt::UserRole,
        static_cast<int>(checked
            ? GitIgnoreMarkup::LineKind::Rule
            : GitIgnoreMarkup::LineKind::DisabledRule));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    if (!checked) {
        item->setForeground(QColor(QStringLiteral("#9aa0a8")));
    }
}

void GitIgnoreDialog::onInsertWildcard(const QString& token)
{
    inputEdit_->insert(token);
    inputEdit_->setFocus();
}

void GitIgnoreDialog::onPreset(const QString& pattern)
{
    inputEdit_->setText(pattern);
    inputEdit_->setFocus();
    inputEdit_->selectAll();
}

void GitIgnoreDialog::onInsertLineLabel()
{
    bool ok = false;
    const QString text = QInputDialog::getText(this,
        QString::fromUtf8("\u4e0b\u4e00\u884c\u6807\u6ce8"),
        QString::fromUtf8("\u8f93\u5165\u6807\u6ce8\u6587\u672c "
            "(\u5c06\u6807\u6ce8\u7d27\u8ddf\u5176\u4e0b\u7684\u89c4\u5219\u884c):"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || text.trimmed().isEmpty()) return;

    auto* item = new QListWidgetItem(QStringLiteral(">>> ") + text.trimmed(),
        ruleList_);
    item->setData(Qt::UserRole,
        static_cast<int>(GitIgnoreMarkup::LineKind::LineLabel));
    item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
    item->setForeground(QColor(QStringLiteral("#e2a03f")));
    const int row = ruleList_->currentRow();
    if (row >= 0 && row < ruleList_->count() - 1) {
        ruleList_->insertItem(row + 1, item);
        ruleList_->setCurrentRow(row + 1);
    } else {
        ruleList_->addItem(item);
        ruleList_->setCurrentItem(item);
    }
}

void GitIgnoreDialog::onInsertGroupBegin()
{
    bool ok = false;
    const QString title = QInputDialog::getText(this,
        QString::fromUtf8("\u4e0b\u4e00\u7ec4\u6807\u6ce8 \u7ec4\u5f00\u59cb"),
        QString::fromUtf8("\u8f93\u5165\u7ec4\u6807\u9898 "
            "(\u63d2\u5165 #{ \u9996\u6807\u8bb0, \u4e0b\u4e00\u884c\u8d77\u4e3a\u7ec4\u5185\u5185\u5bb9):"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || title.trimmed().isEmpty()) return;

    auto* item = new QListWidgetItem(kGroupBeginMark + title.trimmed(),
        ruleList_);
    item->setData(Qt::UserRole,
        static_cast<int>(GitIgnoreMarkup::LineKind::GroupBegin));
    item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
    item->setForeground(QColor(QStringLiteral("#4dd0e1")));
    item->setBackground(QBrush(QColor(kGroupBg)));
    {
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
    }
    const int row = ruleList_->currentRow();
    if (row >= 0 && row < ruleList_->count() - 1) {
        ruleList_->insertItem(row + 1, item);
        ruleList_->setCurrentRow(row + 1);
    } else {
        ruleList_->addItem(item);
        ruleList_->setCurrentItem(item);
    }
}

void GitIgnoreDialog::onInsertGroupEnd()
{
    // 组尾: 文本置空, 由 delegate 渲染为水平分割线
    auto* item = new QListWidgetItem(QString(), ruleList_);
    item->setData(Qt::UserRole,
        static_cast<int>(GitIgnoreMarkup::LineKind::GroupEnd));
    item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
    const int row = ruleList_->currentRow();
    if (row >= 0 && row < ruleList_->count() - 1) {
        ruleList_->insertItem(row + 1, item);
        ruleList_->setCurrentRow(row + 1);
    } else {
        ruleList_->addItem(item);
        ruleList_->setCurrentItem(item);
    }
}

void GitIgnoreDialog::onAutoSummarize()
{
    // 自动总结: 扫描仓库内容, 汇总常见应忽略的目录/文件/扩展名
    static const QStringList knownDirs = {
        "build", "dist", "out", "target", "bin", "obj",
        "node_modules", ".gradle", ".idea", ".vscode", ".cache",
        "logs", "tmp", "temp", "venv", ".venv", "__pycache__",
        ".settings", "classes",
    };
    static const QStringList knownExts = {
        "log", "tmp", "bak", "cache", "class", "pyc", "obj", "pdb",
        "suo", "user", "dmp", "trace",
    };
    static const QStringList knownFiles = { ".DS_Store", "Thumbs.db",
        "desktop.ini", "thumbs.db" };

    QSet<QString> suggested;
    QStringList roots{ repoRoot_ };
    QDir branchDir(repoRoot_ + QStringLiteral("/branches"));
    if (branchDir.exists()) {
        for (const auto& sub : branchDir.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            roots << branchDir.filePath(sub);
        }
    }
    for (const QString& root : roots) {
        QDirIterator it(root, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
            QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString name = it.fileName();
            if (name == QLatin1String(".git") || name == QLatin1String(".NSUM"))
                continue;
            const bool isDir = it.fileInfo().isDir();
            if (isDir) {
                if (knownDirs.contains(name)) {
                    suggested.insert(name + QLatin1Char('/'));
                }
            } else {
                if (knownFiles.contains(name)) {
                    suggested.insert(name);
                }
                const QString ext = QFileInfo(name).suffix().toLower();
                if (knownExts.contains(ext)) {
                    suggested.insert(QStringLiteral("*.") + ext);
                }
            }
        }
    }
    if (suggested.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("\u81ea\u52a8\u603b\u7ed3"),
            QString::fromUtf8(
                "\u672a\u53d1\u73b0\u5e38\u89c1\u53ef\u5ffd\u7565\u7684\u76ee\u5f55/\u6587\u4ef6\u3002"));
        return;
    }

    // 勾选对话框: 用户选择要添加的规则
    QStringList sorted = suggested.values();
    sorted.sort();
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("\u81ea\u52a8\u603b\u7ed3\u5efa\u8bae"));
    dlg.resize(420, 460);
    auto* lay = new QVBoxLayout(&dlg);
    auto* info = new QLabel(QString::fromUtf8(
        "\u68c0\u6d4b\u5230\u4ee5\u4e0b\u53ef\u5ffd\u7565\u9879\uff0c\u52fe\u9009\u540e\u6dfb\u52a0:"),
        &dlg);
    lay->addWidget(info);
    auto* list = new QListWidget(&dlg);
    for (const QString& s : sorted) {
        auto* item = new QListWidgetItem(s, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    lay->addWidget(list, 1);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dlg);
    bb->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("\u6dfb\u52a0\u9009\u4e2d"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    QSet<QString> existing;
    for (int i = 0; i < ruleList_->count(); ++i) {
        auto* item = ruleList_->item(i);
        const auto kind = static_cast<GitIgnoreMarkup::LineKind>(
            item->data(Qt::UserRole).toInt());
        if (kind == GitIgnoreMarkup::LineKind::Rule
            || kind == GitIgnoreMarkup::LineKind::DisabledRule) {
            existing.insert(item->text().trimmed());
        }
    }
    int added = 0;
    for (int i = 0; i < list->count(); ++i) {
        auto* item = list->item(i);
        if (item->checkState() != Qt::Checked) continue;
        const QString pat = item->text().trimmed();
        if (pat.isEmpty() || existing.contains(pat)) continue;
        addRuleRow(pat, true);
        existing.insert(pat);
        ++added;
    }
    if (added > 0) {
        ruleList_->scrollToBottom();
    }
}

void GitIgnoreDialog::onRemoveRule()
{
    auto* item = ruleList_->currentItem();
    if (!item) return;
    delete item;
}

void GitIgnoreDialog::onMoveUp()
{
    const int row = ruleList_->currentRow();
    if (row <= 0) return;
    auto* item = ruleList_->takeItem(row);
    ruleList_->insertItem(row - 1, item);
    ruleList_->setCurrentRow(row - 1);
}

void GitIgnoreDialog::onMoveDown()
{
    const int row = ruleList_->currentRow();
    if (row < 0 || row >= ruleList_->count() - 1) return;
    auto* item = ruleList_->takeItem(row);
    ruleList_->insertItem(row + 1, item);
    ruleList_->setCurrentRow(row + 1);
}

void GitIgnoreDialog::writeFile(const QString& content)
{
    QFile f(absPath_);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QString::fromUtf8("\u4fdd\u5b58\u5931\u8d25"),
            QString::fromUtf8("\u65e0\u6cd5\u5199\u5165: %1").arg(absPath_));
        return;
    }
    f.write(content.toUtf8());
    f.close();
    emit saved(absPath_);
}

void GitIgnoreDialog::onSaveGui()
{
    writeFile(guiContent());
    syncCodeEditor();
}

void GitIgnoreDialog::onSaveCode()
{
    if (!codeEditor_) return;
    const QString content = codeEditor_->toPlainText();
    writeFile(content);
    // 图形化页同步磁盘最新内容
    rebuildGuiList(content);
}

} // namespace GitIgnoreMarkup

// moc handled by AUTOMOC