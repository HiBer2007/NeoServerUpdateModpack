#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

namespace HiBerGUI {

// 高亮片段样式
enum HighlightStyle {
    HlNormal = 0,
    HlKeyword = 1,
    HlString = 2,
    HlComment = 3,
    HlNumber = 4,
    HlType = 5,
    HlConstant = 6
};

struct HighlightSpan {
    int start = 0;
    int length = 0;
    int style = HlNormal;
};

// 语言定义: 内置高亮驱动的规则
struct EditorLanguageDef {
    QStringList keywords;
    QStringList types;
    QStringList constants;
    // 留空则使用内置默认规则
    QString stringPattern;
    QString commentPattern;
    QString numberPattern;
};

// 外部高亮驱动扩展 (注册后优先于内置规则)
class ICodeHighlighter {
public:
    virtual ~ICodeHighlighter() = default;
    // 为 langId 的全文生成高亮片段 (Qt 版与 Web 版共用同一扩展接口)
    virtual void highlight(const QString& langId, const QString& text,
        QVector<HighlightSpan>& spans) = 0;
};

// 编辑器扩展动作 (工具条按钮), 参数 = 编辑器 widget
struct EditorAction {
    QString id;
    QString text;
    QString tooltip;
    std::function<void(QWidget*)> handler;
};

// 区域标记: 行区间背景色 (merge 追踪值/错误行等特殊标记, 可扩展)
// 兼容旧调用: {startLine,endLine,color,tag}; 列区间在末尾追加,
// startColumn/endColumn 任一 <0 则整行背景, 否则行内 [startColumn,endColumn) 0-based 列区间
// colorLight: 浅色主题下的背景色 (空 = 与 color 相同); color 默认 #2f6b31 时
// 按 trackedRegionColor 自动适配深浅 (追踪绿), 显式 colorLight 则精确控制
struct RegionHighlight {
    int startLine = 1;   // 1-based 起始行
    int endLine = 1;     // 1-based 结束行 (含)
    QString color;       // #rrggbb 背景色 (深色主题)
    QString tag;         // 扩展标识 (供外部驱动/工具提示使用)
    int startColumn = -1;   // 0-based 起始列 (含); <0 = 整行
    int endColumn = -1;     // 0-based 结束列 (不含); <0 = 整行
    QString colorLight;  // 浅色主题背景色; 空 = 跟随 color
};

// 内置语言标识 (供 UI 手动选定)
QStringList builtinLanguages();   // json/yaml/properties/toml/snbt/txt/plain

// merge 追踪标记默认背景色 (深色=深绿, 浅色=浅绿, 供调用方按主题选用)
QString trackedRegionColor(bool dark);

// 编辑器抽象接口: Qt 版 (CodeEditor) 与 Web 版 (WebCodeEditor) 实现一致
class ICodeEditor {
public:
    virtual ~ICodeEditor() = default;

    virtual QWidget* widget() = 0;
    virtual void setLanguage(const QString& langId) = 0;
    virtual QString language() const = 0;
    virtual void setPlainText(const QString& text) = 0;
    virtual QString toPlainText() const = 0;
    virtual void setReadOnly(bool ro) = 0;
    virtual bool isReadOnly() const = 0;
    virtual void setLineNumbers(bool on) = 0;
    virtual void setFontSize(int pt) = 0;
    // Tab 宽度 (空格数, 2/4, 默认 4)
    virtual void setTabWidth(int spaces) = 0;
    // 深色/浅色主题 (VS Code 配色逻辑)
    virtual void setDarkMode(bool dark) = 0;
    // 行区间背景标记 (merge 特殊标记等; 清除传空列表)
    virtual void setRegionHighlights(const QVector<RegionHighlight>& regions) = 0;
    virtual void registerHighlighter(ICodeHighlighter* h) = 0;
    virtual void addAction(const EditorAction& action) = 0;
    virtual void scrollToTop() = 0;
};

enum class CodeEditorKind {
    Qt,      // 纯 C++/Qt (无 WebView 依赖)
    Web      // WebView2 网页版 (需要 WebView2 运行时)
};

using CodeEditorFactoryFn = ICodeEditor* (*)(QWidget* parent);

// 工厂注册: HiBerGUI 内置 Qt 实现; Web 实现由 HiBerGUIWebEditor 库注册
void registerCodeEditorFactory(CodeEditorKind kind, CodeEditorFactoryFn fn);
ICodeEditor* createCodeEditor(CodeEditorKind kind, QWidget* parent);

// 内置语言定义 (json/properties/txt 等), 可扩展注册新语言
const QVector<EditorLanguageDef>& defaultLanguageDefs();
void registerLanguageDef(const QString& langId, const EditorLanguageDef& def);

// 颜色主题 (深色/浅色) 供两版实现统一使用
struct EditorThemeColors {
    QString background;
    QString text;
    QString keyword;
    QString string;
    QString comment;
    QString number;
    QString type;
    QString constant;
    QString lineNumber;
    QString currentLine;
};
EditorThemeColors editorThemeColors(bool dark);

} // namespace HiBerGUI
