#pragma once

#include <QWidget>

#include "code_editor_interface.h"

struct ICoreWebView2Environment;
struct ICoreWebView2Controller;

class QLabel;
class QToolBar;

namespace HiBerGUI {

// WebView2 网页版代码编辑器: 与 Qt 版 CodeEditor 实现同一 ICodeEditor 接口。
// 仅链接本库的程序才依赖 WebView2 (WebView2Loader.dll); 不使用则零依赖。
// 系统缺少 WebView2 运行时 (或初始化失败) 时显示错误占位, 不崩溃。
class WebCodeEditor : public QWidget, public ICodeEditor {
    Q_OBJECT

public:
    explicit WebCodeEditor(QWidget* parent = nullptr);
    ~WebCodeEditor() override;

    // ICodeEditor (接口与 Qt 版一致)
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

    // WebView2 是否已就绪
    bool usingWebView() const;

public: // COM 回调入口
    void onEnvCreated(ICoreWebView2Environment* env);
    void onControllerCreated(ICoreWebView2Controller* controller);
    void onMessageReceived(const QString& json);
    void onNavigationCompleted(bool ok);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct Impl;
    Impl* impl_;

    // JS ready 握手后重放全部状态 (导航期消息会丢失)
    void replayState();

    QLabel* placeholder_ = nullptr;    // WebView2 不可用时的错误占位
    QString langId_ = QStringLiteral("plain");
    QString text_;                     // C++ 侧镜像 (JS 修改同步回)
    bool readOnly_ = false;
    bool showLineNumbers_ = true;
    bool dark_ = true;
    int fontSize_ = 10;
    QVector<EditorAction> actions_;
    QToolBar* toolbar_ = nullptr;
    QVector<RegionHighlight> regions_;

    void ensureWebView();
    void navigateToHtml();
    void updateWebViewBounds();
    void activatePlaceholder();
    void sendToJs(const QString& json);
};

// 工厂: 由 HiBerGUIWebEditor 库调用注册
ICodeEditor* createWebCodeEditor(QWidget* parent);

} // namespace HiBerGUI
