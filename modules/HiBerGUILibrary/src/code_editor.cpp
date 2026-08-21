#include "code_editor.h"

#include <QPainter>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QRegularExpression>
#include <QPalette>
#include <QTimer>

namespace HiBerGUI {

EditorThemeColors editorThemeColors(bool dark)
{
    // VS Code 配色逻辑 (Dark+/Light+)
    if (dark) {
        EditorThemeColors c;
        c.background = "#1e1e1e";
        c.text = "#d4d4d4";
        c.keyword = "#569cd6";
        c.string = "#ce9178";
        c.comment = "#6a9955";
        c.number = "#b5cea8";
        c.type = "#4ec9b0";
        c.constant = "#dcdcaa";
        c.lineNumber = "#858585";
        c.currentLine = "#2a2d2e";
        return c;
    }
    EditorThemeColors c;
    c.background = "#ffffff";
    c.text = "#000000";
    c.keyword = "#0000ff";
    c.string = "#a31515";
    c.comment = "#008000";
    c.number = "#098658";
    c.type = "#267f99";
    c.constant = "#795e26";
    c.lineNumber = "#a9a9a9";
    c.currentLine = "#e8f0fe";
    return c;
}

QStringList builtinLanguages()
{
    return { QStringLiteral("json"), QStringLiteral("yaml"),
        QStringLiteral("properties"), QStringLiteral("toml"),
        QStringLiteral("snbt"), QStringLiteral("txt"),
        QStringLiteral("plain") };
}

QString trackedRegionColor(bool dark)
{
    return dark ? QStringLiteral("#2f6b31")
                : QStringLiteral("#b7e4c7");   // 浅色模式用浅绿保证可读
}

const QVector<EditorLanguageDef>& defaultLanguageDefs()
{
    static QVector<EditorLanguageDef> list;
    static bool init = false;
    if (!init) {
        init = true;
        EditorLanguageDef json;
        json.keywords = { "true", "false", "null" };
        json.stringPattern = "\"(?:[^\"\\\\]|\\\\.)*\"";
        json.commentPattern = "//[^\n]*";
        json.numberPattern = "-?\\b\\d+(\\.\\d+)?([eE][+-]?\\d+)?\\b";
        list.push_back(json);

        EditorLanguageDef props;
        props.commentPattern = "[#!][^\n]*";
        props.numberPattern = "\\b\\d+(\\.\\d+)?\\b";
        list.push_back(props);

        EditorLanguageDef txt;
        txt.commentPattern = "[#!][^\n]*";
        list.push_back(txt);

        EditorLanguageDef yaml;
        yaml.keywords = { "true", "false", "null", "yes", "no", "on", "off" };
        yaml.commentPattern = "#[^\n]*";
        yaml.numberPattern = "-?\\b\\d+(\\.\\d+)?\\b";
        list.push_back(yaml);

        EditorLanguageDef toml;
        toml.keywords = { "true", "false" };
        toml.commentPattern = "#[^\n]*";
        toml.numberPattern = "-?\\b\\d+(\\.\\d+)?\\b";
        list.push_back(toml);

        EditorLanguageDef snbt;
        snbt.keywords = { "true", "false", "null" };
        snbt.commentPattern = "//[^\n]*";
        snbt.numberPattern = "-?\\b\\d+(\\.\\d+)?\\b";
        list.push_back(snbt);
    }
    return list;
}

void registerLanguageDef(const QString& langId, const EditorLanguageDef& def)
{
    Q_UNUSED(langId);
    Q_UNUSED(def);
}

CodeEditorFactoryFn g_qtFactory = nullptr;
CodeEditorFactoryFn g_webFactory = nullptr;

void registerCodeEditorFactory(CodeEditorKind kind, CodeEditorFactoryFn fn)
{
    if (kind == CodeEditorKind::Qt) {
        g_qtFactory = fn;
    } else {
        g_webFactory = fn;
    }
}

ICodeEditor* createCodeEditor(CodeEditorKind kind, QWidget* parent)
{
    if (kind == CodeEditorKind::Qt) {
        if (g_qtFactory) {
            return g_qtFactory(parent);
        }
        return new CodeEditor(parent);
    }
    if (g_webFactory) {
        return g_webFactory(parent);
    }
    return nullptr;
}

// ---------------------------------------------------------------
// 行号栏: 独立控件, 与编辑区左右布局, 随编辑区滚动同步
// ---------------------------------------------------------------
// 编辑视图: 提升 protected 的块几何 API 供行号栏绘制
class EditorView : public QPlainTextEdit {
public:
    using QPlainTextEdit::firstVisibleBlock;
    using QPlainTextEdit::blockBoundingGeometry;
    using QPlainTextEdit::blockBoundingRect;
    using QPlainTextEdit::contentOffset;

    explicit EditorView(QWidget* parent = nullptr)
        : QPlainTextEdit(parent) {}
};

class LineNumberArea : public QWidget {
public:
    LineNumberArea(CodeEditor* host, QPlainTextEdit* editor)
        : QWidget(host)
        , host_(host)
        , editor_(static_cast<EditorView*>(editor))
    {
        setAttribute(Qt::WA_StyledBackground, true);
    }

    QSize sizeHint() const override
    {
        return QSize(host_->width(), 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        const EditorThemeColors& c = host_->themeColors();
        QPainter painter(this);
        painter.fillRect(rect(), QColor(c.background));

        QTextBlock block = editor_->firstVisibleBlock();
        if (!block.isValid()) {
            return;
        }
        int blockNumber = block.blockNumber();
        int top = qRound(editor_->blockBoundingGeometry(block)
            .translated(editor_->contentOffset()).top());
        int bottom = top + qRound(editor_->blockBoundingRect(block).height());
        QFontMetrics fm(editor_->font());
        painter.setPen(QColor(c.lineNumber));
        while (block.isValid() && top <= height()) {
            if (block.isVisible() && bottom >= 0) {
                const QString num = QString::number(blockNumber + 1);
                // 按块布局实际行位置绘制 (行盒居中偏移一并计入, 与文本严格对齐)
                qreal lineY = 0;
                if (QTextLayout* tl = block.layout()) {
                    if (tl->lineCount() > 0) {
                        lineY = tl->lineAt(0).y();
                    }
                }
                painter.drawText(width() - fm.horizontalAdvance(num) - 8,
                    top + qRound(lineY) + fm.ascent(), num);
            }
            block = block.next();
            if (!block.isValid()) {
                break;
            }
            top = bottom;
            bottom = top + qRound(editor_->blockBoundingRect(block).height());
            ++blockNumber;
        }
    }

private:
    CodeEditor* host_;
    EditorView* editor_;
};

// ---------------------------------------------------------------
// CodeEditor 实现
// ---------------------------------------------------------------
struct CodeEditorPrivate {
    QString langId = QStringLiteral("plain");
    EditorLanguageDef def;
    QVector<QRegularExpression> keywords;
    QVector<QRegularExpression> types;
    QVector<QRegularExpression> constants;
    QRegularExpression stringRe;
    QRegularExpression commentRe;
    QRegularExpression numberRe;
    ICodeHighlighter* external = nullptr;
    QVector<RegionHighlight> regions;
    EditorThemeColors colors = editorThemeColors(true);
    bool dark = true;
    bool showLineNumbers = true;
    bool hlPending = false;
};

CodeEditor::CodeEditor(QWidget* parent)
    : QWidget(parent)
    , d_(new CodeEditorPrivate)
{
    // 左右布局: 顶部工具条(可选) + 行号栏(固定宽) + 编辑区
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* mid = new QHBoxLayout;
    mid->setContentsMargins(0, 0, 0, 0);
    mid->setSpacing(0);

    editor_ = new EditorView(this);
    editor_->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor_->setTabChangesFocus(false);   // Tab 键输入制表符而非切换焦点
    editor_->setFrameShape(QFrame::NoFrame);   // 行号栏与文本同一起点(去 frame 偏移)
    QFont mono = editor_->font();
    mono.setFamily(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(10);
    editor_->setFont(mono);

    numberArea_ = new LineNumberArea(this, editor_);
    mid->addWidget(numberArea_);
    mid->addWidget(editor_, 1);
    outer->addLayout(mid, 1);

    // 初始主题: 跟随宿主调色板 (深色模式兼容)
    setDarkMode(palette().color(QPalette::Window).lightness() < 128);

    // 文本变化后防抖重高亮: 延迟到事件循环 (文档稳定后), 合并连续输入;
    // 高亮内部 blockSignals + beginEditBlock 防止重入与布局风暴
    connect(editor_, &QPlainTextEdit::textChanged, this, [this]() {
        if (!d_->hlPending) {
            d_->hlPending = true;
            QTimer::singleShot(60, this, [this]() {
                d_->hlPending = false;
                applyHighlighting();
            });
        }
    });

    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        updateExtraSelections();
    });
    connect(editor_, &QPlainTextEdit::blockCountChanged, this,
        [this](int) { updateNumberAreaWidth(); });
    connect(editor_->verticalScrollBar(), &QScrollBar::valueChanged, this,
        [this](int) { numberArea_->update(); });
    connect(editor_, &QPlainTextEdit::updateRequest, this,
        [this](const QRect&, int dy) {
            if (dy) {
                numberArea_->update();
            }
        });
    updateNumberAreaWidth();
}

CodeEditor::~CodeEditor()
{
    delete d_;
}

const EditorThemeColors& CodeEditor::themeColors() const
{
    return d_->colors;
}

void CodeEditor::updateNumberAreaWidth()
{
    const int digits = QString::number(qMax(1, editor_->blockCount())).size();
    const int w = 16
        + editor_->fontMetrics().horizontalAdvance(QLatin1Char('9'))
            * qMax(2, digits);
    numberArea_->setFixedWidth(d_->showLineNumbers ? w : 0);
    numberArea_->update();
}

void CodeEditor::setDarkMode(bool dark)
{
    d_->dark = dark;
    d_->colors = editorThemeColors(dark);
    QPalette pal = editor_->palette();
    pal.setColor(QPalette::Base, QColor(d_->colors.background));
    pal.setColor(QPalette::Text, QColor(d_->colors.text));
    editor_->setPalette(pal);
    numberArea_->update();
    applyHighlighting();
    updateExtraSelections();
}

void CodeEditor::setLanguage(const QString& langId)
{
    d_->langId = langId;
    const QString lower = langId.toLower();
    if (lower.contains(QStringLiteral("json"))) {
        d_->def = defaultLanguageDefs().value(0);
    } else if (lower.contains(QStringLiteral("properties"))
        || lower.contains(QStringLiteral("cfg"))
        || lower.endsWith(QStringLiteral("txt"))) {
        d_->def = defaultLanguageDefs().value(1);
    } else if (lower.contains(QStringLiteral("yaml"))
        || lower.contains(QStringLiteral("yml"))) {
        d_->def = defaultLanguageDefs().value(3);
    } else if (lower.contains(QStringLiteral("toml"))) {
        d_->def = defaultLanguageDefs().value(4);
    } else if (lower.contains(QStringLiteral("snbt"))) {
        d_->def = defaultLanguageDefs().value(5);
    } else {
        d_->def = EditorLanguageDef();
    }
    d_->keywords.clear();
    for (const QString& k : d_->def.keywords) {
        d_->keywords << QRegularExpression(
            "\\b" + QRegularExpression::escape(k) + "\\b");
    }
    d_->types.clear();
    for (const QString& t : d_->def.types) {
        d_->types << QRegularExpression(
            "\\b" + QRegularExpression::escape(t) + "\\b");
    }
    d_->constants.clear();
    for (const QString& c : d_->def.constants) {
        d_->constants << QRegularExpression(
            "\\b" + QRegularExpression::escape(c) + "\\b");
    }
    d_->stringRe = QRegularExpression(d_->def.stringPattern.isEmpty()
        ? "\"(?:[^\"\\\\]|\\\\.)*\"|'(?:[^'\\\\]|\\\\.)*'"
        : d_->def.stringPattern);
    d_->commentRe = QRegularExpression(d_->def.commentPattern);
    d_->numberRe = QRegularExpression(d_->def.numberPattern.isEmpty()
        ? "\\b\\d+(\\.\\d+)?\\b" : d_->def.numberPattern);
    applyHighlighting();
    emit languageChanged(langId);
}

QString CodeEditor::language() const
{
    return d_->langId;
}

void CodeEditor::setPlainText(const QString& text)
{
    editor_->setPlainText(text);
    scrollToTop();
    // setPlainText 后光标可能仍在原位 (0->0) 不触发 cursorPositionChanged,
    // 需强制刷新区域标记 (merge 预览等先设置标记再加载内容的场景)
    updateExtraSelections();
}

QString CodeEditor::toPlainText() const
{
    return editor_->toPlainText();
}

void CodeEditor::setReadOnly(bool ro)
{
    editor_->setReadOnly(ro);
}

bool CodeEditor::isReadOnly() const
{
    return editor_->isReadOnly();
}

void CodeEditor::setLineNumbers(bool on)
{
    d_->showLineNumbers = on;
    updateNumberAreaWidth();
}

void CodeEditor::setFontSize(int pt)
{
    QFont f = editor_->font();
    f.setPointSize(pt);
    editor_->setFont(f);
    updateNumberAreaWidth();
    applyHighlighting();   // 字号变化后重设行距与高亮
}

void CodeEditor::setTabWidth(int spaces)
{
    editor_->setTabStopDistance(
        editor_->fontMetrics().horizontalAdvance(QLatin1Char(' '))
        * qMax(1, spaces));
}

void CodeEditor::setRegionHighlights(const QVector<RegionHighlight>& regions)
{
    d_->regions = regions;
    updateExtraSelections();
}

void CodeEditor::registerHighlighter(ICodeHighlighter* h)
{
    d_->external = h;
    applyHighlighting();
}

void CodeEditor::addAction(const EditorAction& action)
{
    if (!toolbar_) {
        toolbar_ = new QToolBar(this);
        toolbar_->setIconSize(QSize(16, 16));
        toolbar_->setMovable(false);
        auto* outer = qobject_cast<QVBoxLayout*>(layout());
        if (outer) {
            outer->insertWidget(0, toolbar_);
        }
    }
    auto* btn = toolbar_->addAction(action.text);
    btn->setToolTip(action.tooltip);
    connect(btn, &QAction::triggered, this,
        [this, action]() { if (action.handler) action.handler(this); });
}

void CodeEditor::scrollToTop()
{
    QTextCursor c = editor_->textCursor();
    c.movePosition(QTextCursor::Start);
    editor_->setTextCursor(c);
}

// 手动高亮: 重置全文档格式后逐行应用正则/外部驱动。
// 性能关键: beginEditBlock/endEditBlock 合并格式修改 + blockSignals 防重入 + 抑制重绘
void CodeEditor::applyHighlighting()
{
    QTextDocument* doc = editor_->document();
    if (!doc) {
        return;
    }
    doc->blockSignals(true);
    editor_->setUpdatesEnabled(false);

    QTextCursor edit(doc);
    edit.beginEditBlock();

    QTextCharFormat normalFmt;
    normalFmt.setForeground(QColor(d_->colors.text));
    QTextCursor reset(doc);
    reset.select(QTextCursor::Document);
    reset.setCharFormat(normalFmt);

    QTextBlock block = doc->begin();
    while (block.isValid()) {
        // 行距统一: 字体行距 +3px (FixedHeight), 行号栏按块几何自动跟随
        const qreal lh = editor_->fontMetrics().lineSpacing() + 3;
        QTextBlockFormat bf = block.blockFormat();
        if (qAbs(bf.lineHeight() - lh) > 0.1
            || bf.lineHeightType() != QTextBlockFormat::FixedHeight) {
            bf.setLineHeight(lh, QTextBlockFormat::FixedHeight);
            QTextCursor bc(block);
            bc.setBlockFormat(bf);
        }
        const QString text = block.text();
        if (d_->external) {
            QVector<HighlightSpan> spans;
            d_->external->highlight(d_->langId, text, spans);
            for (const auto& s : spans) {
                if (s.length <= 0) continue;
                QTextCursor cur(block);
                cur.setPosition(block.position() + s.start);
                cur.setPosition(block.position() + s.start + s.length,
                    QTextCursor::KeepAnchor);
                cur.setCharFormat(formatFor(s.style));
            }
        } else {
            struct Pat {
                QRegularExpression re;
                int style;
            };
            QVector<Pat> pats;
            if (!d_->commentRe.pattern().isEmpty()) {
                pats << Pat{ d_->commentRe, HlComment };
            }
            if (!d_->stringRe.pattern().isEmpty()) {
                pats << Pat{ d_->stringRe, HlString };
            }
            if (!d_->numberRe.pattern().isEmpty()) {
                pats << Pat{ d_->numberRe, HlNumber };
            }
            for (const auto& re : d_->keywords) {
                pats << Pat{ re, HlKeyword };
            }
            for (const auto& re : d_->types) {
                pats << Pat{ re, HlType };
            }
            for (const auto& re : d_->constants) {
                pats << Pat{ re, HlConstant };
            }
            for (const auto& p : pats) {
                auto it = p.re.globalMatch(text);
                while (it.hasNext()) {
                    auto m = it.next();
                    QTextCursor cur(block);
                    cur.setPosition(block.position() + m.capturedStart());
                    cur.setPosition(block.position() + m.capturedStart()
                        + m.capturedLength(), QTextCursor::KeepAnchor);
                    cur.setCharFormat(formatFor(p.style));
                }
            }
        }
        block = block.next();
    }

    edit.endEditBlock();
    doc->blockSignals(false);
    editor_->setUpdatesEnabled(true);
    editor_->viewport()->update();
}

// 当前行高亮 + 区域背景标记 (extraSelections 合并)
// 所有背景 25% 透明 (alpha 0.75), 避免全实心遮挡
void CodeEditor::updateExtraSelections()
{
    QList<QTextEdit::ExtraSelection> sel;
    QTextEdit::ExtraSelection cur;
    QColor curBg(d_->colors.currentLine);
    curBg.setAlphaF(0.75f);
    cur.format.setBackground(curBg);
    cur.format.setProperty(QTextFormat::FullWidthSelection, true);
    cur.cursor = editor_->textCursor();
    cur.cursor.clearSelection();
    sel << cur;

    for (const auto& r : d_->regions) {
        const int first = qMax(1, r.startLine);
        const int last = qMax(first, r.endLine);
        // 颜色对: 深色主题用 color (追踪绿自动适配), 浅色主题优先 colorLight
        const QString bg = d_->dark
            ? ((r.color == QStringLiteral("#2f6b31"))
                ? trackedRegionColor(true) : r.color)
            : (!r.colorLight.isEmpty()
                ? r.colorLight
                : ((r.color == QStringLiteral("#2f6b31"))
                    ? trackedRegionColor(false) : r.color));
        const bool colRange = (r.startColumn >= 0) && (r.endColumn >= 0)
            && (r.endColumn > r.startColumn);
        for (int ln = first; ln <= last; ++ln) {
            QTextBlock b = editor_->document()->findBlockByNumber(ln - 1);
            if (!b.isValid()) {
                continue;
            }
            QTextEdit::ExtraSelection hs;
            QColor hb(bg);
            hb.setAlphaF(0.75f);
            hs.format.setBackground(hb);
            if (colRange) {
                // 列区间: 只框选行内 [startColumn, endColumn)
                const int len = b.text().size();
                const int cs = qBound(0, r.startColumn, len);
                const int ce = qBound(cs, r.endColumn, len);
                hs.cursor = QTextCursor(b);
                hs.cursor.setPosition(b.position() + cs);
                hs.cursor.setPosition(b.position() + ce,
                    QTextCursor::KeepAnchor);
            } else {
                hs.format.setProperty(QTextFormat::FullWidthSelection, true);
                hs.cursor = QTextCursor(b);
            }
            sel << hs;
        }
    }
    editor_->setExtraSelections(sel);
}

QTextCharFormat CodeEditor::formatFor(int style) const
{
    QTextCharFormat f;
    QColor c = d_->colors.text;
    switch (style) {
    case HlKeyword: c = d_->colors.keyword; f.setFontWeight(QFont::Bold); break;
    case HlString: c = d_->colors.string; break;
    case HlComment: c = d_->colors.comment; f.setFontItalic(true); break;
    case HlNumber: c = d_->colors.number; break;
    case HlType: c = d_->colors.type; break;
    case HlConstant: c = d_->colors.constant; break;
    default: break;
    }
    f.setForeground(c);
    return f;
}

} // namespace HiBerGUI

#include "code_editor.moc"
