#pragma once

#include <QMainWindow>
#include <QTreeWidget>
#include <QVector>

#include <doc_group.h>
#include <markdown_renderer.h>

namespace PowerHelper {

class MarkdownViewer;

class ReaderWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ReaderWindow(QWidget* parent = nullptr);

    void openFile(const QString& path);
    void openGroup(const QString& dir);

    // 打开单文档后按标题文本跳转 (TOC 匹配, 找不到忽略)
    void scrollToHeadingText(const QString& text);

private:
    void buildUI();
    void fillTree();
    void loadDoc(int docIndex);
    void loadHeading(int docIndex, int headingIndex);
    void openDocPath(const QString& path, const QString& anchor);

    MarkdownViewer* viewer_;
    QTreeWidget* tocTree_;

    QVector<DocFileInfo> docs_;
    QString currentFile_;
};

} // namespace PowerHelper
