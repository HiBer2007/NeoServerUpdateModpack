#pragma once

#include <QString>
#include <QVector>

#include "powerhelper_export.h"

class QTextDocument;

namespace PowerHelper {

struct TocEntry {
    int level = 0;
    QString text;
};

// Markdown -> HTML (CommonMark + GFM: tables/strikethrough/autolink/tasklist/tagfilter)
QString PH_API renderToHtml(const QString& markdown);

// Markdown -> ANSI 终端文本 (颜色/粗斜体/制表符/表格列对齐, CJK 全角宽度)
QString PH_API renderToTerminal(const QString& markdown);

// Markdown -> QTextDocument (标题分级/表格/代码块/列表, 标题带 ph-n 锚点供 TOC 跳转)
// darkMode=true 时使用深色配色 (代码/引用背景、链接、分隔线、表格边框)
void PH_API renderToDocument(QTextDocument* doc, const QString& markdown,
    bool darkMode = false);

// 提取标题目录 (level >= 1)
QVector<TocEntry> PH_API extractToc(const QString& markdown);

// 显示宽度: CJK 全角按 2 计
int PH_API displayWidth(const QString& text);

// 制表符展开为空格 (按显示宽度对齐到下一制表位)
QString PH_API expandTabs(const QString& text, int tabWidth = 4);

// 标题锚点名: ph-<index>
QString PH_API anchorName(int index);

} // namespace PowerHelper
