#include "gitignore_highlighter.h"

namespace HiBerGUI {

namespace {
// 与 GitIgnoreMarkup 模块同款标准前缀 (高亮器自身判定, 不依赖该模块避免循环依赖)
const QString kLineLabelPrefix = QStringLiteral("#> ");
const QString kGroupBeginPrefix = QStringLiteral("#{ ");
const QString kGroupEndPrefix = QStringLiteral("#}");
const QString kDisabledPrefix = QStringLiteral("#! ");
}

void GitIgnoreHighlighter::highlight(const QString& langId,
    const QString& text, QVector<HighlightSpan>& spans)
{
    if (!langId.contains(QLatin1String("gitignore"))) {
        return;
    }
    if (text.isEmpty()) {
        return;
    }

    int first = 0;
    while (first < text.size() && text.at(first).isSpace()) {
        ++first;
    }
    if (first >= text.size()) {
        return;
    }

    auto add = [&spans](int start, int len, int style) {
        if (start < 0 || len <= 0) return;
        spans.append({ start, len, style });
    };

    const QString trimmed = text.mid(first).trimmed();

    // 标记行 (注释/行标注/组首尾/临时取消): 前缀高亮 + 内容整行注释
    if (trimmed.startsWith(kGroupBeginPrefix)) {
        add(first, 2, HlType);                      // #{
        add(first + 2, text.size() - first - 2, HlComment);
        return;
    }
    if (trimmed == kGroupEndPrefix
        || trimmed.startsWith(kGroupEndPrefix + QLatin1Char(' '))) {
        add(first, 2, HlType);
        return;
    }
    if (trimmed.startsWith(kLineLabelPrefix)) {
        add(first, 2, HlType);                      // #>
        add(first + 2, text.size() - first - 2, HlComment);
        return;
    }
    if (trimmed.startsWith(kDisabledPrefix)) {
        add(first, 2, HlKeyword);                   // #!
        const int contentStart = first + 3;         // "#! "
        add(contentStart, text.size() - contentStart, HlComment);
        highlightPattern(text.mid(contentStart), contentStart, spans);
        return;
    }
    if (trimmed.startsWith(QLatin1Char('#'))) {
        add(first, text.size() - first, HlComment);
        return;
    }

    highlightPattern(text.mid(first), first, spans);
}

void GitIgnoreHighlighter::highlightPattern(const QString& pattern,
    int base, QVector<HighlightSpan>& spans) const
{
    auto add = [&spans](int start, int len, int style) {
        if (start < 0 || len <= 0) return;
        spans.append({ start, len, style });
    };

    if (pattern.isEmpty()) {
        return;
    }
    // 否定前缀 ! (或 \!)
    if (pattern.at(0) == QLatin1Char('!')) {
        add(base, 1, HlKeyword);
    }

    for (int i = 0; i < pattern.size(); ++i) {
        const QChar c = pattern.at(i);
        if (c == QLatin1Char('\\')) {
            add(base + i, 2, HlNumber);
            ++i;
            continue;
        }
        if (c == QLatin1Char('*') || c == QLatin1Char('?')) {
            add(base + i, 1, HlConstant);
            continue;
        }
        if (c == QLatin1Char('[')) {
            int j = i + 1;
            if (j < pattern.size()
                && (pattern.at(j) == QLatin1Char('!')
                    || pattern.at(j) == QLatin1Char('^'))) {
                ++j;
            }
            while (j < pattern.size() && pattern.at(j) != QLatin1Char(']')) {
                ++j;
            }
            if (j < pattern.size()) {
                add(base + i, j - i + 1, HlConstant);
                i = j;
            } else {
                add(base + i, 1, HlConstant);
            }
            continue;
        }
        if (c == QLatin1Char('/')) {
            add(base + i, 1, HlType);
        }
    }
}

} // namespace HiBerGUI