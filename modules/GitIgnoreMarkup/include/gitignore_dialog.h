#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;
class QTabWidget;
class QListWidgetItem;
class QLineEdit;

namespace HiBerGUI {
class ICodeEditor;
class GitIgnoreHighlighter;
}

namespace GitIgnoreMarkup {
enum class LineKind;

// 编辑/查看仓库根 .gitignore 的对话框:
//   Tab 1 图形化编辑:
//     解析/序列化全部经本模块 (GitIgnoreMarkup) interface, 行类型按 LineKind:
//       Rule            活动规则 (勾选) / 取消勾选保存为 #! (临时取消, 可恢复)
//       DisabledRule    临时取消忽略 (重载仍为未勾选, 可重新勾选)
//       LineLabel       下一行标注 (标注紧跟其下的规则行, 独立于组标注)
//       GroupBegin/End  下一组标注 · 首尾标记 (组开始/结束成对, 图形化列表缩进隔离)
//       Comment         普通注释 (不绑定行/组)
//       空行            忽略 (不显示不保存)
//     添加: 手输 + 通配符快捷插入 + 常用预设 + 自动总结仓库建议 + 标记 (行标注/组)
//   Tab 2 直接编辑:   HiBerGUI CodeEditor (Qt 版) + GitIgnoreHighlighter
// 任一保存 = 写回文件 + emit saved(absPath), 宿主负责 git add / 刷新 Git 面板
class GitIgnoreDialog : public QDialog {
    Q_OBJECT

public:
    explicit GitIgnoreDialog(const QString& repoRoot, int initialTab = 0,
        QWidget* parent = nullptr);
    ~GitIgnoreDialog() override;

signals:
    void saved(const QString& absPath);

private slots:
    void onAddRule();
    void onInsertWildcard(const QString& token);
    void onPreset(const QString& pattern);
    void onAutoSummarize();
    void onRemoveRule();
    void onMoveUp();
    void onMoveDown();
    void onSaveGui();
    void onSaveCode();
    void onInsertLineLabel();
    void onInsertGroupBegin();
    void onInsertGroupEnd();

private:
    void loadFromDisk();
    void rebuildGuiList(const QString& content);
    QString guiContent() const;
    QString stripDisplayPrefix(const QString& shown) const;
    void writeFile(const QString& content);
    void addRuleRow(const QString& pattern, bool checked);
    void syncCodeEditor();

    QString repoRoot_;
    QString absPath_;

    QTabWidget* tabs_ = nullptr;
    QListWidget* ruleList_ = nullptr;
    QLineEdit* inputEdit_ = nullptr;
    HiBerGUI::ICodeEditor* codeEditor_ = nullptr;
    HiBerGUI::GitIgnoreHighlighter* highlighter_ = nullptr;
};

} // namespace GitIgnoreMarkup