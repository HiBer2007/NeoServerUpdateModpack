#pragma once

#include "code_editor_interface.h"

namespace HiBerGUI {

// .gitignore 语法高亮驱动 (ICodeHighlighter 外部驱动, Qt 与 Web 版共用接口):
//   #  普通注释        -> HlComment
//   #> 下一行标注       -> HlComment + 前缀 HlType
//   #{ 组开始 / #} 组结束 -> 前缀 HlType (组边界可见)
//   #! 临时取消规则     -> 前缀 HlKeyword, 规则片段 HlNormal
//   ! 否定前缀          -> HlKeyword
//   * ? [..] 通配符     -> HlConstant
//   尾部 / 目录标记      -> HlType
//   \ 转义序列          -> HlNumber
// langId 非 gitignore 时不输出任何片段 (交由内置规则)。
class GitIgnoreHighlighter : public ICodeHighlighter {
public:
    void highlight(const QString& langId, const QString& text,
        QVector<HighlightSpan>& spans) override;

private:
    // 规则行模式高亮 (否定/通配符/目录/转义), base = 片段在整行的起始偏移
    void highlightPattern(const QString& pattern, int base,
        QVector<HighlightSpan>& spans) const;
};

} // namespace HiBerGUI