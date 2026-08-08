#pragma once

#include <QWidget>
#include <QVector>

#include <markdown_renderer.h>
#include <powerhelper_export.h>

class QTextBrowser;
class QTextDocument;
struct ICoreWebView2Environment;
struct ICoreWebView2Controller;

namespace PowerHelper {

// 可嵌入的 Markdown 阅读器视图 (WebView2 渲染: 真实 HTML/CSS 表格/图片/链接)
// 系统缺少 WebView2 运行时(旧版 Windows/未装组件)时自动回退 Qt 内置 QTextBrowser 渲染
class PH_API MarkdownViewer : public QWidget {
    Q_OBJECT

public:
    explicit MarkdownViewer(QWidget* parent = nullptr);
    ~MarkdownViewer() override;

    void setMarkdown(const QString& markdown);
    void loadFile(const QString& path);

    QVector<TocEntry> toc() const { return toc_; }
    void scrollToHeading(int index);
    void scrollToHeadingText(const QString& text);

    // 当前是否为 Qt 内置回退渲染 (WebView2 不可用)
    bool usingWebView() const;

signals:
    void tocChanged();
    // 点击文档内相对链接 (指向其他 .md/文件), absPath 已解析为绝对路径
    void openFileRequested(const QString& absPath, const QString& anchor);

public: // COM 回调入口 (WebView2 事件)
    void onEnvCreated(ICoreWebView2Environment* env);
    void onControllerCreated(ICoreWebView2Controller* controller);
    void onMessageReceived(const QString& json);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct Impl;
    Impl* impl_;

    QTextBrowser* fb_ = nullptr;
    QString baseDir_;
    QString pendingMarkdown_;
    QVector<TocEntry> toc_;
    bool usingFallback_ = false;

    void ensureWebView();
    void navigateToHtml(const QString& html);
    void updateWebViewBounds();
    void activateFallback();
    void renderFallback();
    void onLinkClicked(const QString& href);
};

} // namespace PowerHelper
