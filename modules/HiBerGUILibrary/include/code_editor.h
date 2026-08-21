#pragma once

#include <QWidget>
#include <QTextCharFormat>
#include <QVector>

#include "code_editor_interface.h"

class QPlainTextEdit;
class QToolBar;

namespace HiBerGUI {

class CodeEditorPrivate;
class LineNumberArea;

// 纯 C++/Qt 代码编辑器 (左右布局): 左侧固定宽度行号栏 + 右侧编辑区。
// 行号/文本不再错位 (组合布局, 无 viewport margin 计算); 语法高亮
// (内置规则/外部驱动) + 当前行高亮 + 区域背景标记 + 扩展动作工具条。
// 零 WebView 依赖。
class CodeEditor : public QWidget, public ICodeEditor {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget* parent = nullptr);
    ~CodeEditor() override;

    // ICodeEditor
    QWidget* widget() override { return this; }
    void setLanguage(const QString& langId) override;
    QString language() const override;
    void setPlainText(const QString& text) override;
    QString toPlainText() const override;
    void setReadOnly(bool ro) override;
    bool isReadOnly() const override;
    void setLineNumbers(bool on) override;
    void setFontSize(int pt) override;
    void setTabWidth(int spaces) override;
    void setDarkMode(bool dark) override;
    void setRegionHighlights(const QVector<RegionHighlight>& regions) override;
    void registerHighlighter(ICodeHighlighter* h) override;
    void addAction(const EditorAction& action) override;
    void scrollToTop() override;

    // 供行号栏绘制使用
    QPlainTextEdit* editor() const { return editor_; }
    const EditorThemeColors& themeColors() const;

signals:
    void languageChanged(const QString& langId);

private:
    CodeEditorPrivate* d_;
    QPlainTextEdit* editor_ = nullptr;
    LineNumberArea* numberArea_ = nullptr;
    QToolBar* toolbar_ = nullptr;

    void updateNumberAreaWidth();
    void applyHighlighting();
    void updateExtraSelections();
    QTextCharFormat formatFor(int style) const;
};

} // namespace HiBerGUI
