#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace HiBerGUI {
class ICodeEditor;
class GitIgnoreHighlighter;

// 文件内容编辑器: 核心编辑框复用 HiBerGUI CodeEditor (Qt 版, 语法高亮),
// 外层提供文件路径/状态提示与保存按钮; 保存经 contentSaveRequested 交由宿主落盘。
class FileContentEditor : public QWidget {
    Q_OBJECT

public:
    explicit FileContentEditor(QWidget* parent = nullptr);
    ~FileContentEditor() override;

    void loadContent(const QString& relPath, const QString& absPath,
        bool inherited, const QString& sourceAbs);

    ICodeEditor* editor() const { return editor_; }

signals:
    void contentSaveRequested(const QString& relPath, const QString& content,
        bool inherited);

private:
    QLabel* pathLabel_;
    QLabel* stateLabel_;
    ICodeEditor* editor_ = nullptr;
    QPushButton* saveBtn_;
    GitIgnoreHighlighter* gitIgnoreHl_ = nullptr;

    QString relPath_;
    bool inherited_ = false;
};

} // namespace HiBerGUI