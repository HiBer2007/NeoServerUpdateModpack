#include "markdown_renderer.h"

#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm-extension_api.h>

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>

#include <cstdlib>
#include <cstring>

namespace PowerHelper {

namespace {

const char* kReset = "\x1b[0m";
const char* kBold = "\x1b[1m";
const char* kItalic = "\x1b[3m";
const char* kUnderline = "\x1b[4m";
const char* kStrike = "\x1b[9m";
const char* kInverse = "\x1b[7m";
const char* kGray = "\x1b[90m";
const char* kBlue = "\x1b[34m";
const char* kCyan = "\x1b[96m";
const char* kYellow = "\x1b[93m";
const char* kGreen = "\x1b[92m";
const char* kMagenta = "\x1b[95m";

bool isType(cmark_node* node, const char* name)
{
    return strcmp(cmark_node_get_type_string(node), name) == 0;
}

struct ParsedDoc {
    cmark_node* root = nullptr;
    cmark_llist* exts = nullptr;
    ~ParsedDoc()
    {
        cmark_mem* mem = cmark_get_default_mem_allocator();
        if (root)
            cmark_node_free(root);
        if (exts)
            cmark_llist_free(mem, exts);
    }
};

ParsedDoc parseDoc(const QString& markdown)
{
    ParsedDoc pd;
    cmark_mem* mem = cmark_get_default_mem_allocator();
    cmark_gfm_core_extensions_ensure_registered();
    const char* extNames[] = { "table", "strikethrough", "autolink",
        "tasklist", "tagfilter" };
    for (const char* n : extNames) {
        cmark_syntax_extension* ext = cmark_find_syntax_extension(n);
        if (ext)
            pd.exts = cmark_llist_append(mem, pd.exts, ext);
    }
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT);
    for (cmark_llist* e = pd.exts; e; e = e->next) {
        cmark_parser_attach_syntax_extension(
            parser, static_cast<cmark_syntax_extension*>(e->data));
    }
    const QByteArray utf8 = markdown.toUtf8();
    cmark_parser_feed(parser, utf8.constData(), static_cast<size_t>(utf8.size()));
    pd.root = cmark_parser_finish(parser);
    cmark_parser_free(parser);
    return pd;
}

QString nodeLiteral(cmark_node* node)
{
    const char* s = cmark_node_get_literal(node);
    return s ? QString::fromUtf8(s) : QString();
}

QString nodeUrl(cmark_node* node)
{
    const char* s = cmark_node_get_url(node);
    return s ? QString::fromUtf8(s) : QString();
}

// 拼接节点子树纯文本 (TOC / 表格单元格用)
QString plainText(cmark_node* node)
{
    QString out;
    switch (cmark_node_get_type(node)) {
    case CMARK_NODE_TEXT:
    case CMARK_NODE_CODE:
        out += nodeLiteral(node);
        break;
    case CMARK_NODE_LINEBREAK:
        out += QLatin1Char('\n');
        break;
    case CMARK_NODE_SOFTBREAK:
        out += QLatin1Char(' ');
        break;
    default: {
        for (cmark_node* c = cmark_node_first_child(node); c;
             c = cmark_node_next(c)) {
            out += plainText(c);
        }
        break;
    }
    }
    return out;
}

void renderInline(cmark_node* node, QString& out);

void renderChildrenInline(cmark_node* node, QString& out)
{
    for (cmark_node* c = cmark_node_first_child(node); c;
         c = cmark_node_next(c)) {
        renderInline(c, out);
    }
}

void renderInline(cmark_node* node, QString& out)
{
    const cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
    case CMARK_NODE_TEXT:
        out += nodeLiteral(node);
        break;
    case CMARK_NODE_CODE:
        out += kInverse;
        out += nodeLiteral(node);
        out += kReset;
        break;
    case CMARK_NODE_EMPH:
        out += kItalic;
        renderChildrenInline(node, out);
        out += kReset;
        break;
    case CMARK_NODE_STRONG:
        out += kBold;
        renderChildrenInline(node, out);
        out += kReset;
        break;
    case CMARK_NODE_LINK: {
        out += kUnderline;
        out += kBlue;
        renderChildrenInline(node, out);
        out += kReset;
        const QString url = nodeUrl(node);
        const QString label = plainText(node);
        if (!url.isEmpty() && url != label) {
            out += kGray;
            out += QStringLiteral(" (%1)").arg(url);
            out += kReset;
        }
        break;
    }
    case CMARK_NODE_IMAGE:
        out += kMagenta;
        out += kItalic;
        out += QStringLiteral("[img: %1]").arg(nodeUrl(node));
        out += kReset;
        break;
    case CMARK_NODE_SOFTBREAK:
        out += QLatin1Char(' ');
        break;
    case CMARK_NODE_LINEBREAK:
        out += QLatin1Char('\n');
        break;
    case CMARK_NODE_HTML_INLINE:
        out += nodeLiteral(node);
        break;
    default:
        if (isType(node, "strikethrough")) {
            out += kStrike;
            renderChildrenInline(node, out);
            out += kReset;
        } else {
            renderChildrenInline(node, out);
        }
        break;
    }
}

// 给多行文本每行加前缀
QString prefixLines(const QString& text, const QString& prefix)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    QString out;
    for (const QString& line : lines) {
        out += prefix;
        out += line;
        out += QLatin1Char('\n');
    }
    return out;
}

QString padCell(const QString& text, int width, int align)
{
    const int pad = width - displayWidth(text);
    if (pad <= 0)
        return text;
    if (align == 'c') {
        const int l = pad / 2;
        return QString(l, QLatin1Char(' ')) + text
            + QString(pad - l, QLatin1Char(' '));
    }
    if (align == 'r')
        return QString(pad, QLatin1Char(' ')) + text;
    return text + QString(pad, QLatin1Char(' '));
}

QString renderTable(cmark_node* table)
{
    QVector<QVector<QString>> rows;
    bool firstRow = true;
    for (cmark_node* row = cmark_node_first_child(table); row;
         row = cmark_node_next(row)) {
        if (!isType(row, "table_row"))
            continue;
        QVector<QString> cells;
        for (cmark_node* cell = cmark_node_first_child(row); cell;
             cell = cmark_node_next(cell)) {
            if (isType(cell, "table_cell"))
                cells.append(plainText(cell));
        }
        rows.append(cells);
        firstRow = false;
    }
    if (rows.isEmpty())
        return QString();

    int cols = 0;
    for (const auto& r : rows)
        cols = qMax(cols, r.size());
    if (cols == 0)
        return QString();

    QVector<char> aligns(cols, 'l');
    const uint8_t* raw = cmark_gfm_extensions_get_table_alignments(table);
    for (int c = 0; c < cols; ++c) {
        if (raw && (raw[c] == 'l' || raw[c] == 'c' || raw[c] == 'r'))
            aligns[c] = static_cast<char>(raw[c]);
    }

    QVector<int> widths(cols, 0);
    for (const auto& r : rows) {
        for (int c = 0; c < r.size(); ++c)
            widths[c] = qMax(widths[c], displayWidth(r[c]) + 2);
    }

    auto hline = [&](QChar left, QChar mid, QChar right) {
        QString s;
        s += left;
        for (int c = 0; c < cols; ++c) {
            s += QString(widths[c], QChar(0x2500));
            s += (c + 1 < cols) ? mid : right;
        }
        return s + QLatin1Char('\n');
    };

    auto rowLine = [&](const QVector<QString>& r) {
        QString s = QStringLiteral("│");
        for (int c = 0; c < cols; ++c) {
            const QString cell = (c < r.size()) ? r[c] : QString();
            s += QLatin1Char(' ') + padCell(cell, widths[c] - 2, aligns[c])
                + QLatin1Char(' ') + QStringLiteral("│");
        }
        return s + QLatin1Char('\n');
    };

    QString out = QString::fromLatin1(kGray);
    out += hline(QChar(0x250C), QChar(0x252C), QChar(0x2510));
    out += kReset;
    out += QString::fromLatin1(kCyan) + kBold;
    out += rowLine(rows.first());
    out += kReset;
    out += kGray;
    out += hline(QChar(0x251C), QChar(0x253C), QChar(0x2524));
    out += kReset;
    for (int i = 1; i < rows.size(); ++i)
        out += rowLine(rows[i]);
    out += kGray;
    out += hline(QChar(0x2514), QChar(0x2534), QChar(0x2518));
    out += kReset;
    return out;
}

void renderBlock(cmark_node* node, QString& out, int indent)
{
    const cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
    case CMARK_NODE_HEADING: {
        const int level = cmark_node_get_heading_level(node);
        QString style = QString::fromLatin1(kBold);
        if (level <= 1)
            style = QString::fromLatin1(kBold) + kUnderline
                + QStringLiteral("\x1b[97m");
        else if (level == 2)
            style = QString::fromLatin1(kBold) + kCyan;
        else if (level == 3)
            style = QString::fromLatin1(kBold) + kYellow;
        out += style;
        renderChildrenInline(node, out);
        out += kReset;
        out += QStringLiteral("\n\n");
        break;
    }
    case CMARK_NODE_PARAGRAPH:
        renderChildrenInline(node, out);
        out += QStringLiteral("\n\n");
        break;
    case CMARK_NODE_THEMATIC_BREAK:
        out += kGray;
        out += QString(48, QChar(0x2500));
        out += kReset;
        out += QStringLiteral("\n\n");
        break;
    case CMARK_NODE_CODE_BLOCK: {
        out += kGreen;
        QString code = nodeLiteral(node);
        code = expandTabs(code);
        out += prefixLines(code, QStringLiteral("  "));
        out += kReset;
        out += QLatin1Char('\n');
        break;
    }
    case CMARK_NODE_HTML_BLOCK:
        out += kMagenta;
        out += nodeLiteral(node);
        out += kReset;
        out += QLatin1Char('\n');
        break;
    case CMARK_NODE_BLOCK_QUOTE: {
        QString inner;
        for (cmark_node* c = cmark_node_first_child(node); c;
             c = cmark_node_next(c)) {
            renderBlock(c, inner, indent + 1);
        }
        out += kGray;
        out += prefixLines(inner, QStringLiteral("│ "));
        out += kReset;
        out += QLatin1Char('\n');
        break;
    }
    case CMARK_NODE_LIST: {
        const bool ordered =
            cmark_node_get_list_type(node) == CMARK_ORDERED_LIST;
        const int start = cmark_node_get_list_start(node);
        int counter = start;
        for (cmark_node* item = cmark_node_first_child(node); item;
             item = cmark_node_next(item)) {
            if (cmark_node_get_type(item) != CMARK_NODE_ITEM)
                continue;
            QString marker;
            if (isType(item, "tasklist")) {
                marker = cmark_gfm_extensions_get_tasklist_item_checked(item)
                    ? QStringLiteral("☑ ")
                    : QStringLiteral("☐ ");
            } else if (ordered) {
                marker = QStringLiteral("%1. ").arg(counter++);
            } else {
                marker = QStringLiteral("• ");
            }
            out += QString(indent * 2, QLatin1Char(' ')) + marker;
            QString itemInner;
            for (cmark_node* c = cmark_node_first_child(item); c;
                 c = cmark_node_next(c)) {
                if (cmark_node_get_type(c) == CMARK_NODE_PARAGRAPH) {
                    renderChildrenInline(c, itemInner);
                    itemInner += QLatin1Char('\n');
                } else {
                    renderBlock(c, itemInner, indent + 1);
                }
            }
            out += itemInner;
        }
        out += QLatin1Char('\n');
        break;
    }
    default:
        if (isType(node, "table")) {
            out += renderTable(node);
            out += QLatin1Char('\n');
        } else {
            for (cmark_node* c = cmark_node_first_child(node); c;
                 c = cmark_node_next(c)) {
                renderBlock(c, out, indent);
            }
        }
        break;
    }
}

} // namespace

QString renderToHtml(const QString& markdown)
{
    ParsedDoc pd = parseDoc(markdown);
    if (!pd.root)
        return QString();
    char* html = cmark_render_html(pd.root, CMARK_OPT_DEFAULT, pd.exts);
    QString out = html ? QString::fromUtf8(html) : QString();
    if (html)
        free(html);
    return out;
}

QString renderToTerminal(const QString& markdown)
{
    ParsedDoc pd = parseDoc(markdown);
    if (!pd.root)
        return QString();
    QString out;
    renderBlock(pd.root, out, 0);
    while (out.endsWith(QLatin1Char('\n')))
        out.chop(1);
    return out;
}

QVector<TocEntry> extractToc(const QString& markdown)
{
    QVector<TocEntry> toc;
    ParsedDoc pd = parseDoc(markdown);
    if (!pd.root)
        return toc;
    for (cmark_node* n = cmark_node_first_child(pd.root); n;
         n = cmark_node_next(n)) {
        if (cmark_node_get_type(n) == CMARK_NODE_HEADING) {
            TocEntry e;
            e.level = cmark_node_get_heading_level(n);
            e.text = plainText(n);
            toc.append(e);
        }
    }
    return toc;
}

int displayWidth(const QString& text)
{
    int w = 0;
    for (const QChar& c : text) {
        const ushort u = c.unicode();
        const bool wide =
            (u >= 0x1100 && u <= 0x115F) || u == 0x2329 || u == 0x232A
            || (u >= 0x2E80 && u <= 0xA4CF && u != 0x303F)
            || (u >= 0xAC00 && u <= 0xD7A3) || (u >= 0xF900 && u <= 0xFAFF)
            || (u >= 0xFE30 && u <= 0xFE4F) || (u >= 0xFF00 && u <= 0xFF60)
            || (u >= 0xFFE0 && u <= 0xFFE6) || (u >= 0x20000 && u <= 0x2FFFD)
            || (u >= 0x30000 && u <= 0x3FFFD);
        w += wide ? 2 : 1;
    }
    return w;
}

QString expandTabs(const QString& text, int tabWidth)
{
    if (tabWidth <= 0)
        return text;
    QString out;
    int col = 0;
    for (const QChar& c : text) {
        if (c == QLatin1Char('\t')) {
            const int next = (col / tabWidth + 1) * tabWidth;
            out += QString(next - col, QLatin1Char(' '));
            col = next;
        } else {
            out += c;
            col += displayWidth(QString(c));
            if (c == QLatin1Char('\n'))
                col = 0;
        }
    }
    return out;
}

QString anchorName(int index)
{
    return QStringLiteral("ph-%1").arg(index);
}

// ============================================================
// QTextDocument 构建 (GUI 阅读器)
// ============================================================
namespace {

struct DocColors {
    QColor codeBg;
    QColor codeFg;
    QColor quoteBg;
    QColor link;
    QColor linkUrl;
    QColor rule;
    QColor border;
};

DocColors docColors(bool dark)
{
    DocColors c;
    if (dark) {
        c.codeBg = QColor(0x2B, 0x2B, 0x2B);
        c.codeFg = QColor(0xE0, 0xE0, 0xE0);
        c.quoteBg = QColor(0x26, 0x26, 0x26);
        c.link = QColor(0x6C, 0xB4, 0xEE);
        c.linkUrl = QColor(0x88, 0x88, 0x88);
        c.rule = QColor(0x55, 0x55, 0x55);
        c.border = QColor(0x66, 0x66, 0x66);
    } else {
        c.codeBg = QColor(0xF2, 0xF2, 0xF2);
        c.codeFg = QColor(); // 默认前景
        c.quoteBg = QColor(0xF7, 0xF7, 0xF7);
        c.link = QColor(0x1A, 0x66, 0xCC);
        c.linkUrl = QColor(0x88, 0x88, 0x88);
        c.rule = QColor(0xAA, 0xAA, 0xAA);
        c.border = QColor(0xC0, 0xC0, 0xC0);
    }
    return c;
}

struct DocState {
    int anchorCount = 0;
    DocColors colors;
};

QTextCharFormat docBaseFormat()
{
    QTextCharFormat f;
    f.setFontFamily(QStringLiteral("Microsoft YaHei"));
    f.setFontPointSize(10.5);
    return f;
}

QTextCharFormat docMonoFormat(const QTextCharFormat& base,
    const DocColors& c)
{
    QTextCharFormat f = base;
    f.setFontFamily(QStringLiteral("Consolas"));
    f.setFontPointSize(9.5);
    f.setBackground(c.codeBg);
    if (c.codeFg.isValid())
        f.setForeground(c.codeFg);
    return f;
}

void insertDocInline(cmark_node* node, QTextCursor& cur,
    QTextCharFormat base, const DocColors& c);

void insertDocChildrenInline(cmark_node* node, QTextCursor& cur,
    QTextCharFormat base, const DocColors& c)
{
    for (cmark_node* ch = cmark_node_first_child(node); ch;
         ch = cmark_node_next(ch)) {
        insertDocInline(ch, cur, base, c);
    }
}

void insertDocInline(cmark_node* node, QTextCursor& cur,
    QTextCharFormat base, const DocColors& c)
{
    const cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
    case CMARK_NODE_TEXT:
        cur.insertText(nodeLiteral(node), base);
        break;
    case CMARK_NODE_CODE:
        cur.insertText(nodeLiteral(node), docMonoFormat(base, c));
        break;
    case CMARK_NODE_EMPH: {
        QTextCharFormat f = base;
        f.setFontItalic(true);
        insertDocChildrenInline(node, cur, f, c);
        break;
    }
    case CMARK_NODE_STRONG: {
        QTextCharFormat f = base;
        f.setFontWeight(QFont::Bold);
        insertDocChildrenInline(node, cur, f, c);
        break;
    }
    case CMARK_NODE_LINK: {
        QTextCharFormat f = base;
        f.setForeground(c.link);
        f.setFontUnderline(true);
        const QString url = nodeUrl(node);
        if (!url.isEmpty())
            f.setAnchorHref(url);
        insertDocChildrenInline(node, cur, f, c);
        const QString label = plainText(node);
        if (!url.isEmpty() && url != label) {
            QTextCharFormat g = base;
            g.setForeground(c.linkUrl);
            cur.insertText(QStringLiteral(" (%1)").arg(url), g);
        }
        break;
    }
    case CMARK_NODE_IMAGE:
        cur.insertText(QStringLiteral("[%1]").arg(nodeUrl(node)),
            base);
        break;
    case CMARK_NODE_SOFTBREAK:
        cur.insertText(QStringLiteral(" "), base);
        break;
    case CMARK_NODE_LINEBREAK:
        cur.insertBlock();
        break;
    case CMARK_NODE_HTML_INLINE:
        cur.insertText(nodeLiteral(node), base);
        break;
    default:
        if (isType(node, "strikethrough")) {
            QTextCharFormat f = base;
            f.setFontStrikeOut(true);
            insertDocChildrenInline(node, cur, f, c);
        } else {
            insertDocChildrenInline(node, cur, base, c);
        }
        break;
    }
}

void insertDocBlock(cmark_node* node, QTextCursor& cur, int depth,
    DocState& st);

void insertDocItemChildren(cmark_node* item, QTextCursor& cur, int depth,
    DocState& st)
{
    for (cmark_node* c = cmark_node_first_child(item); c;
         c = cmark_node_next(c)) {
        if (cmark_node_get_type(c) == CMARK_NODE_PARAGRAPH) {
            insertDocChildrenInline(c, cur, docBaseFormat(), st.colors);
            cur.insertBlock();
        } else {
            insertDocBlock(c, cur, depth, st);
        }
    }
}

void insertDocBlock(cmark_node* node, QTextCursor& cur, int depth,
    DocState& st)
{
    const cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
    case CMARK_NODE_HEADING: {
        const int level = cmark_node_get_heading_level(node);
        QTextCharFormat f = docBaseFormat();
        f.setFontWeight(QFont::Bold);
        switch (level) {
        case 1: f.setFontPointSize(19); break;
        case 2: f.setFontPointSize(15); break;
        case 3: f.setFontPointSize(13); break;
        default: f.setFontPointSize(11.5); break;
        }
        const QString anchor = anchorName(st.anchorCount++);
        f.setAnchor(true);
        f.setAnchorNames({ anchor });
        insertDocChildrenInline(node, cur, f, st.colors);
        cur.insertBlock();
        QTextBlockFormat bf = cur.blockFormat();
        bf.setBottomMargin(8);
        bf.setTopMargin(depth == 0 && level == 1 ? 4 : 12);
        cur.setBlockFormat(bf);
        break;
    }
    case CMARK_NODE_PARAGRAPH: {
        insertDocChildrenInline(node, cur, docBaseFormat(), st.colors);
        cur.insertBlock();
        break;
    }
    case CMARK_NODE_THEMATIC_BREAK: {
        QTextCharFormat f = docBaseFormat();
        f.setForeground(st.colors.rule);
        cur.insertText(QString(60, QChar(0x2500)), f);
        cur.insertBlock();
        break;
    }
    case CMARK_NODE_CODE_BLOCK: {
        const QString code = expandTabs(nodeLiteral(node));
        QTextCharFormat f = docMonoFormat(docBaseFormat(), st.colors);
        QTextBlockFormat bf;
        bf.setLeftMargin(12);
        bf.setBackground(st.colors.codeBg);
        for (const QString& line : code.split(QLatin1Char('\n'))) {
            cur.setBlockFormat(bf);
            cur.insertText(line, f);
            cur.insertBlock();
        }
        break;
    }
    case CMARK_NODE_HTML_BLOCK:
        cur.insertText(nodeLiteral(node),
            docMonoFormat(docBaseFormat(), st.colors));
        cur.insertBlock();
        break;
    case CMARK_NODE_BLOCK_QUOTE: {
        for (cmark_node* c = cmark_node_first_child(node); c;
             c = cmark_node_next(c)) {
            QTextBlockFormat bf = cur.blockFormat();
            bf.setLeftMargin(18);
            bf.setBackground(st.colors.quoteBg);
            cur.setBlockFormat(bf);
            insertDocBlock(c, cur, depth + 1, st);
        }
        break;
    }
    case CMARK_NODE_LIST: {
        const bool ordered =
            cmark_node_get_list_type(node) == CMARK_ORDERED_LIST;
        const int start = cmark_node_get_list_start(node);
        int counter = start;
        QTextBlockFormat bf;
        bf.setLeftMargin(16 + depth * 20);
        for (cmark_node* item = cmark_node_first_child(node); item;
             item = cmark_node_next(item)) {
            if (cmark_node_get_type(item) != CMARK_NODE_ITEM)
                continue;
            cur.setBlockFormat(bf);
            QString marker;
            if (isType(item, "tasklist")) {
                marker = cmark_gfm_extensions_get_tasklist_item_checked(item)
                    ? QStringLiteral("[x] ")
                    : QStringLiteral("[ ] ");
            } else if (ordered) {
                marker = QStringLiteral("%1. ").arg(counter++);
            } else {
                marker = QStringLiteral("• ");
            }
            QTextCharFormat mf = docBaseFormat();
            mf.setFontWeight(QFont::Bold);
            cur.insertText(marker, mf);
            insertDocItemChildren(item, cur, depth + 1, st);
        }
        break;
    }
    default:
        if (isType(node, "table")) {
            QVector<QVector<QString>> rows;
            for (cmark_node* row = cmark_node_first_child(node); row;
                 row = cmark_node_next(row)) {
                if (!isType(row, "table_row"))
                    continue;
                QVector<QString> cells;
                for (cmark_node* cell = cmark_node_first_child(row); cell;
                     cell = cmark_node_next(cell)) {
                    if (isType(cell, "table_cell"))
                        cells.append(plainText(cell));
                }
                rows.append(cells);
            }
            if (!rows.isEmpty()) {
                int cols = 0;
                for (const auto& r : rows)
                    cols = qMax(cols, r.size());
                if (cols > 0) {
                    QTextTable* table = cur.insertTable(
                        rows.size(), cols);
                    QTextTableFormat tf;
                    tf.setCellPadding(4);
                    tf.setCellSpacing(0);
                    tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
                    tf.setBorder(1);
                    tf.setBorderBrush(st.colors.border);
                    table->setFormat(tf);
                    const uint8_t* raw =
                        cmark_gfm_extensions_get_table_alignments(node);
                    for (int r = 0; r < rows.size(); ++r) {
                        for (int c = 0; c < cols; ++c) {
                            QTextTableCell cell = table->cellAt(r, c);
                            QTextCursor cc = cell.firstCursorPosition();
                            QTextCharFormat cf = docBaseFormat();
                            if (r == 0)
                                cf.setFontWeight(QFont::Bold);
                            if (r < rows.size()
                                && c < rows[r].size())
                                cc.insertText(rows[r][c], cf);
                            if (raw && c < cols) {
                                QTextBlockFormat bf = cc.blockFormat();
                                if (raw[c] == 'r')
                                    bf.setAlignment(Qt::AlignRight);
                                else if (raw[c] == 'c')
                                    bf.setAlignment(Qt::AlignCenter);
                                cc.setBlockFormat(bf);
                            }
                        }
                    }
                    cur = table->lastCursorPosition();
                    cur.insertBlock();
                }
            }
        } else {
            for (cmark_node* c = cmark_node_first_child(node); c;
                 c = cmark_node_next(c)) {
                insertDocBlock(c, cur, depth, st);
            }
        }
        break;
    }
}

} // namespace

void renderToDocument(QTextDocument* doc, const QString& markdown,
    bool darkMode)
{
    if (!doc)
        return;
    doc->clear();
    doc->setDefaultFont(QFont(QStringLiteral("Microsoft YaHei"), 10));
    ParsedDoc pd = parseDoc(markdown);
    if (!pd.root)
        return;
    QTextCursor cur(doc);
    DocState st;
    st.colors = docColors(darkMode);
    insertDocBlock(pd.root, cur, 0, st);
    doc->setModified(false);
}

} // namespace PowerHelper
