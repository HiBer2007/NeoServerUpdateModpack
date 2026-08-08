#include "markdown_viewer.h"

#include <windows.h>
#include <wrl/client.h>
#include <WebView2.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTextBrowser>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>

#include <markdown_renderer.h>

#include <atomic>
#include <string>

using namespace Microsoft::WRL;

namespace PowerHelper {

namespace {

const char* kViewerCss = R"(
:root {
  --bg: #ffffff; --fg: #1f2328; --muted: #57606a;
  --border: #d0d7de; --code-bg: #f6f8fa; --link: #0969da;
  --table-head: #f6f8fa; --quote-border: #d0d7de;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #1e1e1e; --fg: #e6e6e6; --muted: #9aa0a6;
    --border: #444c56; --code-bg: #2d333b; --link: #6cb4ee;
    --table-head: #2d333b; --quote-border: #444c56;
  }
}
html, body { margin: 0; padding: 0; }
body {
  background: var(--bg); color: var(--fg);
  font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
  font-size: 15px; line-height: 1.75;
  padding: 28px 36px 60px; max-width: 960px;
}
h1, h2, h3, h4 { line-height: 1.35; margin: 1.2em 0 0.5em; }
h1 { font-size: 1.9em; border-bottom: 1px solid var(--border); padding-bottom: 0.3em; }
h2 { font-size: 1.45em; border-bottom: 1px solid var(--border); padding-bottom: 0.25em; }
h3 { font-size: 1.18em; }
a { color: var(--link); text-decoration: none; }
a:hover { text-decoration: underline; }
img { max-width: 100%; border-radius: 6px; }
table { border-collapse: collapse; margin: 14px 0; display: block; overflow-x: auto; }
th, td { border: 1px solid var(--border); padding: 7px 13px; text-align: left; vertical-align: top; }
th { background: var(--table-head); font-weight: 600; white-space: nowrap; }
tr:nth-child(even) td { background: rgba(128,128,128,0.06); }
code {
  font-family: Consolas, "Courier New", monospace;
  background: var(--code-bg); border: 1px solid var(--border);
  border-radius: 4px; padding: 1px 5px; font-size: 0.9em;
}
pre {
  background: var(--code-bg); border: 1px solid var(--border);
  border-radius: 8px; padding: 14px; overflow-x: auto; line-height: 1.55;
}
pre code { background: transparent; border: none; padding: 0; font-size: 0.92em; }
blockquote {
  margin: 14px 0; padding: 2px 16px;
  border-left: 4px solid var(--quote-border); color: var(--muted);
}
hr { border: none; border-top: 1px solid var(--border); margin: 22px 0; }
ul, ol { padding-left: 26px; }
input[type="checkbox"] { margin-right: 6px; vertical-align: -2px; }
)";

const char* kClickInterceptorJs = R"(
(function() {
  document.addEventListener('click', function(e) {
    var a = e.target.closest ? e.target.closest('a') : null;
    if (!a) return;
    var href = a.getAttribute('href') || '';
    if (href.charAt(0) === '#') {
      var el = document.getElementById(href.slice(1));
      if (el) { e.preventDefault(); el.scrollIntoView({behavior: 'smooth', block: 'start'}); }
      return;
    }
    e.preventDefault();
    window.chrome.webview.postMessage(JSON.stringify({type: 'link', href: href}));
  }, true);
})();
)";

QString mimeForPath(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "webp") return "image/webp";
    if (ext == "bmp") return "image/bmp";
    if (ext == "ico") return "image/x-icon";
    return "application/octet-stream";
}

QString fileToDataUri(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QStringLiteral("data:%1;base64,%2")
        .arg(mimeForPath(path))
        .arg(QString::fromLatin1(f.readAll().toBase64()));
}

QString htmlUnescape(const QString& s)
{
    QString out = s;
    out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    out.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    out.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    out.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    out.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    return out;
}

// 标题 id="h-N" + 标题文本锚点; 相对/本地图片转 data URI
QString postProcessHtml(const QString& html, const QString& baseDir)
{
    QString out = html;
    QRegularExpression headRe(QStringLiteral("<(h[1-6])>"));
    QList<std::pair<QString, int>> heads;
    auto iter = headRe.globalMatch(out);
    while (iter.hasNext()) {
        auto m = iter.next();
        heads.append({ m.captured(1), m.capturedStart() });
    }
    // 在每个 <hN> 标签的闭合 '>' 之前插入 id 属性 (doc 顺序, 与 TOC/scrollToHeading 对齐)
    for (int i = heads.size() - 1; i >= 0; --i) {
        const QString tag = heads[i].first;
        const int pos = heads[i].second;
        const int tagLen = tag.size() + 1; // '<'+tag+'>' 中 '>' 的偏移
        out.insert(pos + tagLen, QStringLiteral(" id=\"h-%1\"").arg(i));
    }

    // 同文档 #标题文本 跳转锚点: 提取标题纯文本插入 <a id="文本">
    QRegularExpression headingTextRe(
        QStringLiteral("<h([1-6]) id=\"h-\\d+\">(.*?)</h\\1>"),
        QRegularExpression::DotMatchesEverythingOption);
    std::vector<std::pair<int, QString>> anchorInserts;
    iter = headingTextRe.globalMatch(out);
    while (iter.hasNext()) {
        auto m = iter.next();
        QString text = htmlUnescape(m.captured(2));
        text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
        text = text.trimmed();
        if (text.isEmpty())
            continue;
        text.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
        anchorInserts.push_back(
            { m.capturedStart(2),
                QStringLiteral("<a id=\"%1\"></a>").arg(text) });
    }
    for (auto it = anchorInserts.rbegin(); it != anchorInserts.rend(); ++it)
        out.insert(it->first, it->second);

    QRegularExpression imgRe(
        QStringLiteral("<img src=\"([^\"]+)\""),
        QRegularExpression::CaseInsensitiveOption);
    std::vector<std::pair<QString, QString>> imgFix;
    iter = imgRe.globalMatch(out);
    while (iter.hasNext()) {
        auto m = iter.next();
        QString src = m.captured(1);
        if (src.startsWith(QStringLiteral("http://"))
            || src.startsWith(QStringLiteral("https://"))
            || src.startsWith(QStringLiteral("data:")))
            continue;
        if (src.startsWith(QStringLiteral("file:///")))
            src = src.mid(8);
        QString abs;
        if (QFileInfo(src).isAbsolute())
            abs = QDir::cleanPath(src);
        else if (!baseDir.isEmpty())
            abs = QDir::cleanPath(baseDir + QLatin1Char('/') + src);
        else
            continue;
        if (!QFileInfo::exists(abs))
            continue;
        const QString uri = fileToDataUri(abs);
        if (uri.isEmpty())
            continue;
        imgFix.push_back({ m.captured(0),
            QStringLiteral("<img src=\"%1\"").arg(uri) });
    }
    for (auto it = imgFix.rbegin(); it != imgFix.rend(); ++it)
        out.replace(it->first, it->second);

    return out;
}

QString wrapViewerHtml(const QString& bodyHtml)
{
    return QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<style>%1</style></head><body>%2</body></html>")
        .arg(QString::fromLatin1(kViewerCss), bodyHtml);
}

// COM 事件回调基类
template <typename I>
class ComBase : public I {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(I)) {
            *ppv = static_cast<I*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG r = --refs_;
        if (r == 0)
            delete this;
        return r;
    }

protected:
    ~ComBase() = default;

private:
    std::atomic<ULONG> refs_{ 1 };
};

class EnvHandler : public ComBase<
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
public:
    explicit EnvHandler(QPointer<MarkdownViewer> viewer)
        : viewer_(viewer)
    {
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT, ICoreWebView2Environment* env) override
    {
        if (env)
            env->AddRef(); // 异步队列期间保持存活
        QMetaObject::invokeMethod(QCoreApplication::instance(),
            [viewer = viewer_, env]() {
                if (viewer)
                    viewer->onEnvCreated(env);
                if (env)
                    env->Release();
            }, Qt::QueuedConnection);
        return S_OK;
    }

private:
    QPointer<MarkdownViewer> viewer_;
};

class ControllerHandler : public ComBase<
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
public:
    explicit ControllerHandler(QPointer<MarkdownViewer> viewer)
        : viewer_(viewer)
    {
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT,
        ICoreWebView2Controller* controller) override
    {
        if (controller)
            controller->AddRef();
        QMetaObject::invokeMethod(QCoreApplication::instance(),
            [viewer = viewer_, controller]() {
                if (viewer)
                    viewer->onControllerCreated(controller);
                if (controller)
                    controller->Release();
            }, Qt::QueuedConnection);
        return S_OK;
    }

private:
    QPointer<MarkdownViewer> viewer_;
};

class NavStartingHandler : public ComBase<ICoreWebView2NavigationStartingEventHandler> {
public:
    explicit NavStartingHandler(QPointer<MarkdownViewer> viewer)
        : viewer_(viewer)
    {
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*,
        ICoreWebView2NavigationStartingEventArgs* args) override
    {
        LPWSTR uriRaw = nullptr;
        if (FAILED(args->get_Uri(&uriRaw)))
            return S_OK;
        const QString url = QString::fromWCharArray(uriRaw);
        CoTaskMemFree(uriRaw);
        if (url.startsWith(QStringLiteral("http://"))
            || url.startsWith(QStringLiteral("https://"))
            || url.startsWith(QStringLiteral("file://"))) {
            args->put_Cancel(TRUE);
            QMetaObject::invokeMethod(QCoreApplication::instance(),
                [viewer = viewer_, url]() {
                    if (viewer)
                        QDesktopServices::openUrl(QUrl(url));
                }, Qt::QueuedConnection);
        }
        return S_OK;
    }

private:
    QPointer<MarkdownViewer> viewer_;
};

class WebMsgHandler : public ComBase<ICoreWebView2WebMessageReceivedEventHandler> {
public:
    explicit WebMsgHandler(QPointer<MarkdownViewer> viewer)
        : viewer_(viewer)
    {
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*,
        ICoreWebView2WebMessageReceivedEventArgs* args) override
    {
        LPWSTR jsonRaw = nullptr;
        if (FAILED(args->get_WebMessageAsJson(&jsonRaw)))
            return S_OK;
        const QString json = QString::fromWCharArray(jsonRaw);
        CoTaskMemFree(jsonRaw);
        QMetaObject::invokeMethod(QCoreApplication::instance(),
            [viewer = viewer_, json]() {
                if (viewer)
                    viewer->onMessageReceived(json);
            }, Qt::QueuedConnection);
        return S_OK;
    }

private:
    QPointer<MarkdownViewer> viewer_;
};

} // namespace

struct MarkdownViewer::Impl {
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    bool ready = false;
    bool creating = false;
    QString pendingHtml;
};

MarkdownViewer::MarkdownViewer(QWidget* parent)
    : QWidget(parent)
    , impl_(new Impl)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    setAttribute(Qt::WA_NativeWindow);
}

MarkdownViewer::~MarkdownViewer()
{
    // WebView2 关闭为异步过程, 且其内部线程可能持有回调:
    // 释放 COM 引用交给进程退出清理, 避免退出期竞态崩溃
    if (impl_ && impl_->controller) {
        impl_->controller->Close();
        impl_->controller.Detach();
    }
    if (impl_ && impl_->webview)
        impl_->webview.Detach();
    // impl_ 与 COM 初始化不在此释放, 进程退出时由 OS 回收
}

bool MarkdownViewer::usingWebView() const
{
    return !usingFallback_;
}

void MarkdownViewer::activateFallback()
{
    if (usingFallback_)
        return;
    usingFallback_ = true;
    if (!fb_) {
        fb_ = new QTextBrowser(this);
        fb_->setOpenExternalLinks(false);
        fb_->setOpenLinks(false);
        fb_->viewport()->setAutoFillBackground(false);
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(fb_);
        connect(fb_, &QTextBrowser::anchorClicked, this,
            [this](const QUrl& u) {
                onLinkClicked(u.toString());
            });
    }
    fb_->show();
    if (!pendingMarkdown_.isEmpty())
        renderFallback();
}

void MarkdownViewer::onLinkClicked(const QString& href)
{
    if (href.startsWith(QStringLiteral("#")))
        return;
    if (href.startsWith(QStringLiteral("http://"))
        || href.startsWith(QStringLiteral("https://"))
        || href.startsWith(QStringLiteral("mailto:"))) {
        QDesktopServices::openUrl(QUrl(href));
        return;
    }
    // 相对 .md / 文件链接
    QString abs = href;
    if (!QFileInfo(abs).isAbsolute() && !baseDir_.isEmpty())
        abs = QDir::cleanPath(baseDir_ + QLatin1Char('/') + href);
    if (QFileInfo(abs).suffix().toLower() == QStringLiteral("md"))
        emit openFileRequested(abs, QString());
    else
        QDesktopServices::openUrl(QUrl::fromLocalFile(abs));
}

void MarkdownViewer::renderFallback()
{
    renderToDocument(fb_->document(), pendingMarkdown_);
    fb_->setStyleSheet(QStringLiteral(
        "QTextBrowser { background: palette(base); color: palette(text); }"));
    fb_->document()->setDefaultFont(QFont(QStringLiteral("Microsoft YaHei"), 10));
}

void MarkdownViewer::ensureWebView()
{
    if (usingFallback_) {
        activateFallback();
        return;
    }
    if (impl_->ready || impl_->creating)
        return;
    impl_->creating = true;
    // 探测 WebView2 运行时是否可用; 不可用则直接回退 Qt 内置渲染
    LPWSTR availableVer = nullptr;
    if (FAILED(GetAvailableCoreWebView2BrowserVersionString(nullptr, &availableVer))
        || !availableVer) {
        impl_->creating = false;
        activateFallback();
        return;
    }
    CoTaskMemFree(availableVer);

    const QString userData = QDir::tempPath()
        + QStringLiteral("/PowerHelper-webview2-%1")
              .arg(QCoreApplication::applicationPid());
    const auto cb = ComPtr<
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            new EnvHandler(this));
    CreateCoreWebView2EnvironmentWithOptions(nullptr,
        reinterpret_cast<LPCWSTR>(userData.utf16()), nullptr, cb.Get());
}

void MarkdownViewer::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    ensureWebView();
    updateWebViewBounds();
}

void MarkdownViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateWebViewBounds();
}

void MarkdownViewer::updateWebViewBounds()
{
    if (!impl_->controller)
        return;
    const qreal dpr = devicePixelRatioF();
    RECT r{ 0, 0, static_cast<LONG>(width() * dpr),
        static_cast<LONG>(height() * dpr) };
    impl_->controller->put_Bounds(r);
}

void MarkdownViewer::onEnvCreated(ICoreWebView2Environment* env)
{
    if (!env) {
        // WebView2 运行时无法创建 (运行时缺失等) -> 回退 Qt 内置渲染
        impl_->creating = false;
        activateFallback();
        return;
    }
    const auto cb = ComPtr<
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            new ControllerHandler(this));
    env->CreateCoreWebView2Controller(
        reinterpret_cast<HWND>(winId()), cb.Get());
}

void MarkdownViewer::onControllerCreated(ICoreWebView2Controller* controller)
{
    impl_->creating = false;
    if (!controller)
        return;
    impl_->controller = controller;
    controller->get_CoreWebView2(&impl_->webview);
    if (!impl_->webview)
        return;
    impl_->ready = true;

    controller->put_IsVisible(TRUE);
    updateWebViewBounds();

    ComPtr<ICoreWebView2Settings> settings;
    impl_->webview->get_Settings(&settings);
    if (settings) {
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
    }

    const QString js = QString::fromLatin1(kClickInterceptorJs);
    impl_->webview->AddScriptToExecuteOnDocumentCreated(
        reinterpret_cast<LPCWSTR>(js.utf16()), nullptr);
    impl_->webview->add_NavigationStarting(
        ComPtr<ICoreWebView2NavigationStartingEventHandler>(
            new NavStartingHandler(this))
            .Get(), nullptr);
    impl_->webview->add_WebMessageReceived(
        ComPtr<ICoreWebView2WebMessageReceivedEventHandler>(
            new WebMsgHandler(this))
            .Get(), nullptr);

    if (!impl_->pendingHtml.isEmpty()) {
        const QString html = impl_->pendingHtml;
        impl_->pendingHtml.clear();
        navigateToHtml(html);
    }
}

void MarkdownViewer::setMarkdown(const QString& markdown)
{
    pendingMarkdown_ = markdown;
    baseDir_.clear();
    toc_ = extractToc(markdown);
    emit tocChanged();
    if (usingFallback_) {
        renderFallback();
        return;
    }
    const QString html = wrapViewerHtml(
        postProcessHtml(renderToHtml(markdown), QString()));
    navigateToHtml(html);
}

void MarkdownViewer::loadFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QString md = QString::fromUtf8(f.readAll());
    pendingMarkdown_ = md;
    baseDir_ = QFileInfo(path).absolutePath();
    toc_ = extractToc(md);
    emit tocChanged();
    if (usingFallback_) {
        renderFallback();
        return;
    }
    const QString html = wrapViewerHtml(
        postProcessHtml(renderToHtml(md), baseDir_));
    navigateToHtml(html);
}

void MarkdownViewer::navigateToHtml(const QString& html)
{
    if (!impl_->ready || !impl_->webview) {
        impl_->pendingHtml = html;
        ensureWebView();
        return;
    }
    impl_->webview->NavigateToString(
        reinterpret_cast<LPCWSTR>(html.utf16()));
}

void MarkdownViewer::scrollToHeading(int index)
{
    if (usingFallback_) {
        if (fb_)
            fb_->setSource(QUrl(QLatin1Char('#') + anchorName(index)));
        return;
    }
    if (!impl_->webview)
        return;
    const QString js = QStringLiteral(
        "var el=document.getElementById('h-%1');"
        "if(el){el.scrollIntoView({block:'start'});"
        "el.style.background='rgba(255,213,79,0.25)';}"
        "undefined;").arg(index);
    impl_->webview->ExecuteScript(
        reinterpret_cast<LPCWSTR>(js.utf16()), nullptr);
}

void MarkdownViewer::scrollToHeadingText(const QString& text)
{
    if (text.isEmpty())
        return;
    for (int i = 0; i < toc_.size(); ++i) {
        if (toc_[i].text.contains(text)) {
            scrollToHeading(i);
            return;
        }
    }
}

void MarkdownViewer::onMessageReceived(const QString& json)
{
    const QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    if (obj.value(QStringLiteral("type")).toString()
        != QStringLiteral("link"))
        return;
    QString href = obj.value(QStringLiteral("href")).toString();
    if (href.isEmpty())
        return;
    if (href.startsWith(QStringLiteral("http://"))
        || href.startsWith(QStringLiteral("https://"))
        || href.startsWith(QStringLiteral("mailto:"))) {
        QDesktopServices::openUrl(QUrl(href));
        return;
    }
    QString anchor;
    const int hash = href.indexOf(QLatin1Char('#'));
    if (hash >= 0) {
        anchor = href.mid(hash + 1);
        href = href.left(hash);
    }
    if (href.isEmpty()) {
        scrollToHeadingText(anchor);
        return;
    }
    QString abs = href;
    if (!QFileInfo(abs).isAbsolute() && !baseDir_.isEmpty())
        abs = QDir::cleanPath(baseDir_ + QLatin1Char('/') + href);
    if (QFileInfo(abs).suffix().toLower() == QStringLiteral("md"))
        emit openFileRequested(abs, anchor);
    else
        QDesktopServices::openUrl(QUrl::fromLocalFile(abs));
}

} // namespace PowerHelper
