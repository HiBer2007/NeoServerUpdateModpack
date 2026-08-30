#include "gitignore_markup.h"

namespace GitIgnoreMarkup {

namespace {

const QString kCommentPrefix = QStringLiteral("# ");
const QString kLineLabelPrefix = QStringLiteral("#> ");
const QString kGroupBeginPrefix = QStringLiteral("#{ ");
const QString kGroupEndPrefix = QStringLiteral("#}");
const QString kDisabledPrefix = QStringLiteral("#! ");

} // namespace

QString prefixFor(LineKind kind)
{
    switch (kind) {
    case LineKind::Comment: return kCommentPrefix;
    case LineKind::LineLabel: return kLineLabelPrefix;
    case LineKind::GroupBegin: return kGroupBeginPrefix;
    case LineKind::GroupEnd: return kGroupEndPrefix;
    case LineKind::DisabledRule: return kDisabledPrefix;
    case LineKind::Rule: return QString();
    }
    return QString();
}

bool isComment(const QString& trimmed)
{
    return trimmed.startsWith(kCommentPrefix)
        || (trimmed.startsWith(QLatin1Char('#'))
            && !isLineLabel(trimmed) && !isGroupBegin(trimmed)
            && !isGroupEnd(trimmed) && !isDisabledRule(trimmed));
}

bool isLineLabel(const QString& trimmed)
{
    return trimmed.startsWith(kLineLabelPrefix);
}

bool isGroupBegin(const QString& trimmed)
{
    return trimmed.startsWith(kGroupBeginPrefix);
}

bool isGroupEnd(const QString& trimmed)
{
    return trimmed == kGroupEndPrefix
        || trimmed.startsWith(kGroupEndPrefix + QLatin1Char(' '));
}

bool isDisabledRule(const QString& trimmed)
{
    return trimmed.startsWith(kDisabledPrefix);
}

Line makeRule(const QString& pattern, bool checked)
{
    Line l;
    l.kind = LineKind::Rule;
    l.text = pattern;
    l.checked = checked;
    return l;
}

Line makeDisabledRule(const QString& pattern)
{
    Line l;
    l.kind = LineKind::DisabledRule;
    l.text = pattern;
    l.checked = false;
    return l;
}

Line makeLineLabel(const QString& text)
{
    Line l;
    l.kind = LineKind::LineLabel;
    l.text = text;
    return l;
}

Line makeGroupBegin(const QString& title)
{
    Line l;
    l.kind = LineKind::GroupBegin;
    l.text = title;
    return l;
}

Line makeGroupEnd()
{
    Line l;
    l.kind = LineKind::GroupEnd;
    return l;
}

Line makeComment(const QString& text)
{
    Line l;
    l.kind = LineKind::Comment;
    l.text = text;
    return l;
}

QVector<Line> parseLines(const QString& content)
{
    QVector<Line> lines;
    const QStringList rawLines = content.split(QLatin1Char('\n'));
    for (const QString& raw : rawLines) {
        const QString t = raw.trimmed();
        Line l;
        if (isDisabledRule(t)) {
            l.kind = LineKind::DisabledRule;
            l.text = t.mid(kDisabledPrefix.size());
            l.checked = false;
        } else if (isLineLabel(t)) {
            l.kind = LineKind::LineLabel;
            l.text = t.mid(kLineLabelPrefix.size());
        } else if (isGroupBegin(t)) {
            l.kind = LineKind::GroupBegin;
            l.text = t.mid(kGroupBeginPrefix.size());
        } else if (isGroupEnd(t)) {
            l.kind = LineKind::GroupEnd;
        } else if (t.startsWith(QLatin1Char('#'))) {
            l.kind = LineKind::Comment;
            l.text = t.mid(1);
            if (l.text.startsWith(QLatin1Char(' '))) {
                l.text = l.text.mid(1);
            }
        } else if (!t.isEmpty()) {
            l.kind = LineKind::Rule;
            l.text = t;
            l.checked = true;
        } else {
            continue;   // 空行忽略
        }
        lines.append(l);
    }
    return lines;
}

QString serialize(const QVector<Line>& lines)
{
    QStringList out;
    int groupDepth = 0;
    for (const Line& l : lines) {
        switch (l.kind) {
        case LineKind::Comment:
            out << kCommentPrefix + l.text;
            break;
        case LineKind::LineLabel:
            out << kLineLabelPrefix + l.text;
            break;
        case LineKind::GroupBegin:
            out << kGroupBeginPrefix + l.text;
            ++groupDepth;
            break;
        case LineKind::GroupEnd:
            if (groupDepth > 0) {
                out << kGroupEndPrefix;
                --groupDepth;
            }
            // 孤立组结束标记丢弃
            break;
        case LineKind::DisabledRule:
            // 勾选态统一按 checked 输出: 勾选恢复活动规则, 未勾选保持 "#! "
            if (l.checked) {
                out << l.text;
            } else {
                out << (kDisabledPrefix + l.text);
            }
            break;
        case LineKind::Rule:
            if (l.checked) {
                out << l.text;
            } else {
                out << (kDisabledPrefix + l.text);
            }
            break;
        }
    }
    // 末尾未闭合并组: 自动补组结束尾标记 (保证首尾成对)
    while (groupDepth > 0) {
        out << kGroupEndPrefix;
        --groupDepth;
    }
    QString result = out.join(QLatin1Char('\n'));
    if (!result.isEmpty() && !result.endsWith(QLatin1Char('\n'))) {
        result += QLatin1Char('\n');
    }
    return result;
}

} // namespace GitIgnoreMarkup