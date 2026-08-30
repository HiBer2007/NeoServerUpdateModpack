#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// .gitignore 标记/逆向标记系统 (独立模块 GitIgnoreMarkup, 供编辑器/CLI/其他宿主复用):
//
//   标准行格式 (均以行首原样保留, 前缀后为内容):
//     <规则>        = 活动规则 (勾选态)
//     #! <规则>     = 临时取消忽略的规则 (未勾选态, 可重新勾选恢复)
//     #> <文本>     = 下一行标注 (标注紧跟其下的规则行)
//     #{ <文本>     = 下一组标注 · 组开始 (首标记)
//     #}            = 下一组标注 · 组结束 (尾标记)
//     # <文本>      = 普通注释 (不绑定任何行/组)
//     空行          = 忽略 (解析与序列化均不产生)
//
// 序列化约定: 未勾选规则写为 "#! " 前缀 (往返可恢复勾选); 组标记成对输出,
//   孤立组结束标记丢弃, 末尾未闭合的组自动补 "#}" 保证首尾成对。
namespace GitIgnoreMarkup {

enum class LineKind {
    Comment,      // # 普通注释
    LineLabel,    // #> 下一行标注
    GroupBegin,   // #{ 组开始 (首标记)
    GroupEnd,     // #} 组结束 (尾标记)
    DisabledRule, // #! 临时取消规则 (未勾选)
    Rule          // 活动规则 (勾选)
};

struct Line {
    LineKind kind = LineKind::Comment;
    QString prefix;      // 标准前缀 (不含内容): "# ", "#> ", "#{ ", "#} ", "#! "
    QString text;        // 内容 (去前缀, 规则行为规则文本)
    bool checked = true; // Rule/DisabledRule 勾选态
};

// 解析文本 -> 行列表 (空行丢弃; Rule/DisabledRule checked 相应置位)
QVector<Line> parseLines(const QString& content);
// 序列化行列表 -> 文本 (空行不输出; 未勾选规则 -> "#! "; 组首尾成对)
QString serialize(const QVector<Line>& lines);

// 行判定 (trimmed 行首)
bool isComment(const QString& trimmed);
bool isLineLabel(const QString& trimmed);
bool isGroupBegin(const QString& trimmed);
bool isGroupEnd(const QString& trimmed);
bool isDisabledRule(const QString& trimmed);

// 便捷构造
Line makeRule(const QString& pattern, bool checked);
Line makeDisabledRule(const QString& pattern);
Line makeLineLabel(const QString& text);
Line makeGroupBegin(const QString& title);
Line makeGroupEnd();
Line makeComment(const QString& text);

// 标准标记行 (供高亮器/UI 判定标签)
QString prefixFor(LineKind kind);

} // namespace GitIgnoreMarkup