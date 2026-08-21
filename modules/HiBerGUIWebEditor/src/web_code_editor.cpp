#include "web_code_editor.h"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QToolBar>
#include <QToolButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QTimer>
#include <QDebug>

#include <windows.h>
#include <wrl/client.h>
#include <WebView2.h>

using Microsoft::WRL::ComPtr;

namespace HiBerGUI {

namespace {

// 简易 COM 基类 (参照 PowerHelper markdown_viewer)
template <typename T>
class ComBase : public T {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == __uuidof(T) || riid == IID_IUnknown) {
            *ppv = static_cast<T*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    STDMETHODIMP_(ULONG) Release() override
    {
        const ULONG r = --refs_;
        if (r == 0) {
            delete this;
        }
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
    explicit EnvHandler(QPointer<WebCodeEditor> editor)
        : editor_(editor) {}
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT, ICoreWebView2Environment* env) override
    {
        if (env) {
            env->AddRef();
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(),
            [editor = editor_, env]() {
                if (editor) {
                    editor->onEnvCreated(env);
                }
                if (env) {
                    env->Release();
                }
            }, Qt::QueuedConnection);
        return S_OK;
    }

private:
    QPointer<WebCodeEditor> editor_;
};

class ControllerHandler : public ComBase<
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
public:
    explicit ControllerHandler(QPointer<WebCodeEditor> editor)
        : editor_(editor) {}
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT,
        ICoreWebView2Controller* controller) override
    {
        if (controller) {
            controller->AddRef();
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(),
            [editor = editor_, controller]() {
                if (editor) {
                    editor->onControllerCreated(controller);
                }
                if (controller) {
                    controller->Release();
                }
            }, Qt::QueuedConnection);
        return S_OK;
    }

private:
    QPointer<WebCodeEditor> editor_;
};

class WebMsgHandler : public ComBase<ICoreWebView2WebMessageReceivedEventHandler> {
public:
    explicit WebMsgHandler(QPointer<WebCodeEditor> editor)
        : editor_(editor) {}
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*,
        ICoreWebView2WebMessageReceivedEventArgs* args) override
    {
        LPWSTR jsonRaw = nullptr;
        if (FAILED(args->get_WebMessageAsJson(&jsonRaw))) {
            return S_OK;
        }
        const QString json = QString::fromWCharArray(jsonRaw);
        CoTaskMemFree(jsonRaw);
        QMetaObject::invokeMethod(QCoreApplication::instance(),
            [editor = editor_, json]() {
                if (editor) {
                    editor->onMessageReceived(json);
                }
            }, Qt::QueuedConnection);
        return S_OK;
    }

private:
    QPointer<WebCodeEditor> editor_;
};

class NavCompletedHandler : public ComBase<
    ICoreWebView2NavigationCompletedEventHandler> {
public:
    explicit NavCompletedHandler(QPointer<WebCodeEditor> editor)
        : editor_(editor) {}
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*,
        ICoreWebView2NavigationCompletedEventArgs* args) override
    {
        BOOL ok = FALSE;
        args->get_IsSuccess(&ok);
        QMetaObject::invokeMethod(QCoreApplication::instance(),
            [editor = editor_, ok]() {
                if (editor) {
                    editor->onNavigationCompleted(!!ok);
                }
            }, Qt::QueuedConnection);
        return S_OK;
    }

private:
    QPointer<WebCodeEditor> editor_;
};

const char* kEditorHtml = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><style>
html,body{margin:0;padding:0;width:100%;height:100%;overflow:hidden;
  font-family:Consolas,monospace;font-size:13px;}
body.dark{background:#1e1e1e;}
body.light{background:#ffffff;}
#wrap{position:relative;width:100%;height:100%;}
#gutter{position:absolute;left:0;top:0;bottom:0;width:44px;overflow:hidden;
  text-align:right;padding:7px 0;box-sizing:border-box;user-select:none;}
body.dark #gutter{background:#1e1e1e;color:#858585;}
body.light #gutter{background:#f3f3f3;color:#a9a9a9;}
#gutter div{line-height:1.45;padding-right:8px;}
#hl{position:absolute;left:44px;top:0;right:0;bottom:0;overflow:hidden;
  padding:7px 8px;white-space:pre;pointer-events:none;box-sizing:border-box;
  line-height:1.45;font-family:inherit;font-size:inherit;}
body.dark #hl{color:#d4d4d4;}
body.light #hl{color:#000000;}
#colbg{position:absolute;left:44px;top:0;right:0;bottom:0;overflow:hidden;
  padding:7px 8px;pointer-events:none;box-sizing:border-box;}
#colbg div{position:absolute;}
#ed{position:absolute;left:44px;top:0;right:0;bottom:0;border:none;outline:none;
  resize:none;background:transparent;color:transparent;padding:7px 8px;
  white-space:pre;overflow:auto;box-sizing:border-box;
  font-family:inherit;font-size:inherit;line-height:1.45;}
body.dark #ed{caret-color:#aeafad;}
body.light #ed{caret-color:#000000;}
#ed::-webkit-scrollbar{width:12px;height:12px;}
#ed::-webkit-scrollbar-track{background:transparent;}
#ed::-webkit-scrollbar-thumb{background:#4a4d55;border-radius:6px;
  border:2px solid transparent;background-clip:content-box;}
#ed::-webkit-scrollbar-thumb:hover{background:#5a5e68;}
body.light #ed::-webkit-scrollbar-thumb{background:#c1c1c1;}
body.light #ed::-webkit-scrollbar-thumb:hover{background:#a9a9a9;}
#loader{position:absolute;left:0;top:0;right:0;bottom:0;display:flex;
  flex-direction:column;align-items:center;justify-content:center;
  background:#1e1e1e;z-index:10;}
#loader .spin{width:32px;height:32px;border:3px solid #3a3f47;
  border-top-color:#569cd6;border-radius:50%;
  animation:spin 0.9s linear infinite;}
#loader span{color:#9aa0a8;margin-top:10px;font-size:12px;}
@keyframes spin{to{transform:rotate(360deg);}}
</style></head><body class="dark">
<div id="loader"><div class="spin"></div><span>加载中…</span></div>
<div id="wrap">
 <div id="gutter"></div>
 <div id="colbg"></div>
 <div id="hl"></div>
 <textarea id="ed" spellcheck="false" wrap="off"></textarea>
</div>
<script>
let lang='plain';let theme='dark';let fontSize=13;let tabWidth=4;
let regions=[];let curLine=0;
// 直接传对象: WebView2 自动 JSON 序列化; 若传 JSON.stringify 字符串,
// get_WebMessageAsJson 返回带引号的字符串字面量, C++ 解析为空对象
function send(m){if(window.chrome&&window.chrome.webview){
  window.chrome.webview.postMessage(m);}}
// VS Code 配色逻辑 (Dark+/Light+)
const C={dark:{kw:'#569cd6',str:'#ce9178',com:'#6a9955',num:'#b5cea8',
  typ:'#4ec9b0',con:'#dcdcaa',key:'#9cdcfe',txt:'#d4d4d4',cur:'#2a2d2e'},
 light:{kw:'#0000ff',str:'#a31515',com:'#008000',num:'#098658',
  typ:'#267f99',con:'#795e26',key:'#0451a5',txt:'#000000',cur:'#e8f0fe'}};
const KW_BASE=['true','false','null'];
function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;')
  .replace(/>/g,'&gt;');}
function langKw(){
  if(lang==='yaml'){return ['true','false','null','yes','no','on','off'];}
  if(lang==='toml'){return ['true','false'];}
  return KW_BASE;
}
// 各语言 key 高亮: 先提取为带样式占位符 (避免 span 嵌套覆盖), 最后统一还原
function hlLine(line){
  const col=C[theme];
  if(line.match(/^(\s*)([#!]|\/\/)/)){
    return '<span style="color:'+col.com+'">'+esc(line)+'</span>';
  }
  let s=esc(line);
  const marks=[];
  const put=function(t,st){marks.push({t:t,st:st});
    return '\u0001z'+(marks.length-1).toString(36)+'\u0002';};
  // 1) key (按语言): json 带引号 key; yaml/snbt 裸 key+冒号; toml 段头+key+等号; properties key+[=:]
  if(lang==='json'){
    s=s.replace(/(^\s*)("(?:[^"\\]|\\.)*")(\s*:)/gm,function(m,p1,p2,p3){
      return p1+put(p2,'key')+p3;});
  } else if(lang==='yaml'||lang==='snbt'){
    s=s.replace(/(^\s*)([A-Za-z_][\w.-]*)(\s*:)/gm,function(m,p1,p2,p3){
      return p1+put(p2,'key')+p3;});
  } else if(lang==='toml'){
    s=s.replace(/(^\s*)(\[[^\]]*\])(\s*$)/gm,function(m,p1,p2,p3){
      return p1+put(p2,'typ')+p3;});
    s=s.replace(/(^\s*)([A-Za-z_][\w.-]*)(\s*=)/gm,function(m,p1,p2,p3){
      return p1+put(p2,'key')+p3;});
  } else if(lang==='properties'){
    s=s.replace(/(^\s*)([^#!=\s][^=:]*?)(\s*[=:])/gm,function(m,p1,p2,p3){
      return p1+put(p2,'key')+p3;});
  }
  // 2) 字符串
  s=s.replace(/("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')/g,function(m){
    return put(m,'str');});
  // 3) 数字 (占位符含 z+36进制, 不会与 \b\d+ 冲突)
  s=s.replace(/(-?\b\d+(?:\.\d+)?\b)/g,function(m){
    return put(m,'num');});
  // 4) 关键字
  for(const k of langKw()){
    const r=new RegExp('\\b'+k.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+'\\b','g');
    s=s.replace(r,function(m){return put(m,'kw');});
  }
  // 5) 还原
  s=s.replace(/\u0001z([0-9a-z]+)\u0002/g,function(m,n){
    const mk=marks[parseInt(n,36)];
    return '<span style="color:'+col[mk.st]+'">'+mk.t+'</span>';});
  return s;
}
function render(){
  const ed=document.getElementById('ed');
  const lines=ed.value.split('\n');
  // 行高统一取 CSS 计算值 (textarea 与 div 渲染行高一致; 勿用 scrollHeight 反推:
  // 内容不足一屏时 scrollHeight 被钳制为 clientHeight, 行数少会得到虚大行高)
  const lh=parseFloat(getComputedStyle(ed).lineHeight)||16;
  curLine=cursorLine();   // 同步当前光标行 (0-based)
  const g=document.getElementById('gutter');
  g.innerHTML='';
  for(let i=0;i<lines.length;i++){
    const d=document.createElement('div');d.textContent=i+1;g.appendChild(d);
  }
  g.style.lineHeight=lh+'px';
  for(let i=0;i<g.children.length;i++){
    g.children[i].style.lineHeight=lh+'px';
    g.children[i].style.minHeight=lh+'px';   // 空行也占满行高
  }
  const hl=document.getElementById('hl');
  hl.style.lineHeight=lh+'px';
  let html='';
  for(let i=0;i<lines.length;i++){
    html+='<div style="min-height:'+lh+'px;'+(lineBg(i)?'background:'+lineBg(i)+';':'')
      +'">'+hlLine(lines[i])+'</div>';
  }
  hl.innerHTML=html;
  // 列区间标记 (一行含多个项时只框选指定列): 等宽字符定位的绝对定位 overlay
  const cw=charW();
  const cb=document.getElementById('colbg');
  cb.innerHTML='';
  // 绝对定位子元素相对 padding box 原点定位, 需加上容器自身 padding 偏移
  const cbcs=getComputedStyle(cb);
  const padL=parseFloat(cbcs.paddingLeft)||0;
  const padT=parseFloat(cbcs.paddingTop)||0;
  for(const r of regions){
    if(r.sc===undefined||r.sc===null||r.sc<0||r.ec===undefined||r.ec===null||r.ec<=r.sc){continue;}
    const first=Math.max(1,r.start),last=Math.max(first,r.end);
    for(let i=first;i<=last;i++){
      const idx=i-1;
      if(idx<0||idx>=lines.length){continue;}
      const col=regionColor(r);
      const d=document.createElement('div');
      d.style.left=(padL+r.sc*cw)+'px';
      d.style.top=(padT+idx*lh)+'px';
      d.style.width=((r.ec-r.sc)*cw)+'px';
      d.style.height=lh+'px';
      d.style.background=rgba(col,0.75);   // 25% 透明, 偏实心
      cb.appendChild(d);
    }
  }
  syncScroll();
}
// 等宽字符宽度 (Consolas 等宽, 用于列区间定位; 缓存, 字号变化时失效)
let cwCache=-1;
// #rrggbb -> rgba(r,g,b,alpha)
function rgba(c,a){
  const n=parseInt(c.slice(1),16);
  return 'rgba('+((n>>16)&255)+','+((n>>8)&255)+','+(n&255)+','+a+')';
}
function charW(){
  if(cwCache>0){return cwCache;}
  const sp=document.createElement('span');
  sp.style.cssText='position:absolute;visibility:hidden;white-space:pre;font:inherit;';
  sp.textContent='MMMMMMMMMM';
  document.body.appendChild(sp);
  cwCache=sp.getBoundingClientRect().width/10;
  document.body.removeChild(sp);
  return cwCache;
}
// 当前光标行 (0-based): selectionStart 前的换行数
function cursorLine(){
  const ed=document.getElementById('ed');
  const p=ed.selectionStart;
  let n=0;
  for(let i=0;i<p;i++){if(ed.value.charCodeAt(i)===10){n++;}}
  return n;
}
// 行背景: region 整行标记优先 (列区间标记除外), 否则光标所在行。
// 返回 rgba (25% 透明) 供 div 背景使用
function lineBg(i){
  for(const r of regions){
    if(r.sc!==undefined&&r.sc!==null&&r.sc>=0){continue;}   // 列区间走 colbg, 不进整行
    if(i+1>=r.start&&i+1<=r.end){
      const c=regionColor(r);
      return rgba(c,0.75);
    }
  }
  if(i===curLine){return rgba(C[theme].cur,0.75);}
  return '';
}
// 颜色对解析: 深色主题用 color (追踪绿自动适配), 浅色主题优先 light
function regionColor(r){
  if(theme!=='light'){return (r.color==='#2f6b31')?'#2f6b31':r.color;}
  if(r.light&&r.light!==''){return r.light;}
  return (r.color==='#2f6b31')?'#b7e4c7':r.color;
}
// 光标移动 (点击/方向键/聚焦): 仅更新新旧两行背景, 避免整页重渲染
function updateCurLine(){
  const ed=document.getElementById('ed');
  const n=cursorLine();
  if(n===curLine){return;}
  const hl=document.getElementById('hl');
  const old=curLine;
  curLine=n;
  if(old>=0&&old<hl.children.length){
    hl.children[old].style.background=lineBg(old);
  }
  if(n>=0&&n<hl.children.length){
    hl.children[n].style.background=lineBg(n);
  }
}
function syncScroll(){
  const ed=document.getElementById('ed');
  document.getElementById('hl').scrollTop=ed.scrollTop;
  document.getElementById('hl').scrollLeft=ed.scrollLeft;
  document.getElementById('gutter').scrollTop=ed.scrollTop;
  document.getElementById('colbg').scrollTop=ed.scrollTop;
}
function applyTheme(){
  document.body.className=theme;
  render();
}
const ed=document.getElementById('ed');
ed.addEventListener('input',()=>{
  render();
  send({t:'change',text:ed.value});
});
ed.addEventListener('scroll',syncScroll);
// 光标行高亮跟随 (与 Qt 版一致): selectionchange 在选区/光标变化时同步触发,
// 覆盖点击/方向键/拖选/全选, 无 click(mouseup 后)/keyup(松键后) 的延迟
document.addEventListener('selectionchange',updateCurLine);
ed.addEventListener('keydown',e=>{
  if(e.key==='Tab'){
    e.preventDefault();
    const s=ed.selectionStart,en=ed.selectionEnd;
    const pad=' '.repeat(tabWidth);
    ed.value=ed.value.slice(0,s)+pad+ed.value.slice(en);
    ed.selectionStart=ed.selectionEnd=s+tabWidth;
    render();
  }
});
// 消息处理: 命名为全局 handler, 供 HTML 立即注册 / 轮询重试 / C++ 注入 三方复用
window.__nsumHandler = function(e){
  // C++ 侧 PostWebMessageAsJson 发送后 e.data 已是对象 (JSON 解析);
  // 页面自身 postMessage 传字符串时 e.data 为字符串 -> 兼容两种
  let m = (typeof e.data === 'string') ? JSON.parse(e.data) : e.data;
  if(!m) return;
  if(m.t==='setText'){ed.value=m.text;render();send({t:'change',text:ed.value});}
  else if(m.t==='setLang'){lang=m.lang;render();}
  else if(m.t==='setReadOnly'){ed.readOnly=!!m.ro;}
  else if(m.t==='setTheme'){theme=m.theme==='light'?'light':'dark';applyTheme();}
  else if(m.t==='setLineNumbers'){
    document.getElementById('gutter').style.display=m.on?'block':'none';
    document.getElementById('hl').style.left=m.on?'44px':'0';
    document.getElementById('colbg').style.left=m.on?'44px':'0';
    document.getElementById('ed').style.left=m.on?'44px':'0';}
  else if(m.t==='setFontSize'){
    // pt -> px 换算 (96dpi), 与 Qt 版字号全局统一; body/gutter/hl 随 ed 一致
    var px=Math.round(m.pt*96/72);
    ed.style.fontSize=px+'px';document.body.style.fontSize=px+'px';
    cwCache=-1;}   // 字号变化后字符宽需重测
  else if(m.t==='setRegions'){regions=m.regions||[];render();}
  else if(m.t==='setTabWidth'){tabWidth=m.spaces||4;ed.style.tabSize=tabWidth;}
  else if(m.t==='scrollTop'){ed.scrollTop=0;syncScroll();}
  else if(m.t==='hideLoader'){document.getElementById('loader').style.display='none';}
};
function tryReg(){
  if(window.chrome&&window.chrome.webview){
    if(!window.__nsumReg){
      window.__nsumReg=true;
      window.chrome.webview.addEventListener('message',window.__nsumHandler);
    }
    send({t:'ready'});
    return true;
  }
  return false;
}
// chrome.webview 注入可能晚于本脚本执行: 立即尝试 + 轮询重试
if(!tryReg()){
  var tries=0;
  var timer=setInterval(function(){
    tries++;
    if(tryReg()||tries>100){clearInterval(timer);}
  },100);
}
// 载入动画: 持续播放直至 C++ 内容加载完成发送 hideLoader 才关闭
render();
</script></body></html>)HTML";

} // namespace

struct WebCodeEditor::Impl {
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    bool ready = false;
    bool creating = false;
};

WebCodeEditor::WebCodeEditor(QWidget* parent)
    : QWidget(parent)
    , impl_(new Impl)
{
    setAttribute(Qt::WA_StyledBackground, true);
    placeholder_ = new QLabel(
        QString::fromUtf8("\u65e0\u6cd5\u521b\u5efa WebView2 \u7f16\u8f91\u5668\uff1a"
            "\u7cfb\u7edf\u672a\u5b89\u88c5 WebView2 \u8fd0\u884c\u65f6\u3002"),
        this);
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setWordWrap(true);
    placeholder_->hide();
}

WebCodeEditor::~WebCodeEditor()
{
    // WebView2 关闭为异步过程: Close + Detach, 交由进程退出回收
    if (impl_->controller) {
        impl_->controller->Close();
        impl_->controller.Detach();
    }
    if (impl_->webview) {
        impl_->webview.Detach();
    }
    delete impl_;
}

bool WebCodeEditor::usingWebView() const
{
    return impl_->ready;
}

void WebCodeEditor::ensureWebView()
{
    qDebug() << "WebEditor: ensureWebView";
    if (impl_->ready || impl_->creating) {
        return;
    }
    LPWSTR ver = nullptr;
    if (FAILED(GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver))) {
        qDebug() << "WebEditor: runtime probe FAILED";
        activatePlaceholder();
        return;
    }
    if (ver) {
        qDebug() << "WebEditor: runtime version"
                 << QString::fromWCharArray(ver);
        CoTaskMemFree(ver);
    }
    impl_->creating = true;
    const QString userData = QDir::tempPath()
        + QStringLiteral("/NSUM-webeditor-%1")
              .arg(QCoreApplication::applicationPid());
    const auto cb = ComPtr<
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            new EnvHandler(QPointer<WebCodeEditor>(this)));
    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr,
        reinterpret_cast<LPCWSTR>(userData.utf16()), nullptr, cb.Get());
    qDebug() << "WebEditor: CreateCoreWebView2Environment hr" << Qt::hex << (quint32)hr;
}

void WebCodeEditor::onEnvCreated(ICoreWebView2Environment* env)
{
    qDebug() << "WebEditor: onEnvCreated" << (env ? "ok" : "null");
    impl_->creating = false;
    if (!env) {
        activatePlaceholder();
        return;
    }
    const auto cb = ComPtr<
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            new ControllerHandler(QPointer<WebCodeEditor>(this)));
    const HRESULT hr = env->CreateCoreWebView2Controller(
        reinterpret_cast<HWND>(winId()), cb.Get());
    qDebug() << "WebEditor: CreateController hr" << Qt::hex << (quint32)hr;
}

void WebCodeEditor::onControllerCreated(ICoreWebView2Controller* controller)
{
    qDebug() << "WebEditor: onControllerCreated" << (controller ? "ok" : "null");
    if (!controller) {
        activatePlaceholder();
        return;
    }
    impl_->controller = controller;
    if (FAILED(controller->get_CoreWebView2(&impl_->webview))) {
        activatePlaceholder();
        return;
    }
    impl_->ready = true;

    // 深色模式兼容: 初始跟随宿主调色板
    dark_ = palette().color(QPalette::Window).lightness() < 128;

    ComPtr<ICoreWebView2Settings> settings;
    impl_->webview->get_Settings(&settings);
    if (settings) {
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
    }
    impl_->webview->add_WebMessageReceived(
        ComPtr<ICoreWebView2WebMessageReceivedEventHandler>(
            new WebMsgHandler(QPointer<WebCodeEditor>(this)))
            .Get(), nullptr);
    // 页面导航完成回调: 只有完成后再发送状态消息, 否则 postMessage 丢失 (内容无法载入)
    impl_->webview->add_NavigationCompleted(
        ComPtr<ICoreWebView2NavigationCompletedEventHandler>(
            new NavCompletedHandler(QPointer<WebCodeEditor>(this)))
            .Get(), nullptr);

    navigateToHtml();
    updateWebViewBounds();
    controller->put_IsVisible(TRUE);
    placeholder_->hide();
    raise();
}

void WebCodeEditor::onNavigationCompleted(bool ok)
{
    qDebug() << "WebEditor: navigation completed ok=" << ok;
    if (!ok || !impl_->ready) {
        return;
    }
    // 通道兜底: C++ 主动注入监听器 (脚本执行时 chrome.webview 可能尚未注入),
    // 注入成功后回调里立即重放状态 (此时监听器必已注册, 消息必达)
    if (impl_->webview) {
        class ExecHandler : public ComBase<
            ICoreWebView2ExecuteScriptCompletedHandler> {
        public:
            explicit ExecHandler(QPointer<WebCodeEditor> e) : editor_(e) {}
            HRESULT STDMETHODCALLTYPE Invoke(HRESULT, LPCWSTR) override
            {
                QMetaObject::invokeMethod(QCoreApplication::instance(),
                    [editor = editor_]() {
                        if (editor) {
                            editor->replayState();
                        }
                    }, Qt::QueuedConnection);
                return S_OK;
            }
        private:
            QPointer<WebCodeEditor> editor_;
        };
        impl_->webview->ExecuteScript(
            L"if(!window.__nsumReg&&window.chrome&&window.chrome.webview){"
            L"window.__nsumReg=true;"
            L"window.chrome.webview.addEventListener('message',window.__nsumHandler);"
            L"window.chrome.webview.postMessage({t:'pong'});"
            L"document.title='INJECTED';}",
            ComPtr<ICoreWebView2ExecuteScriptCompletedHandler>(
                new ExecHandler(QPointer<WebCodeEditor>(this))).Get());
    }
    // 兜底重放 (ready 握手通常已覆盖; 若 JS 已注册则消息不丢失)
    replayState();
}

void WebCodeEditor::onMessageReceived(const QString& json)
{
    const QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    const QString t = obj.value(QStringLiteral("t")).toString();
    if (t == QStringLiteral("log")) {
        qDebug() << "WebEditor JS:" << obj.value(QStringLiteral("msg")).toString();
    } else if (t == QStringLiteral("echo")) {
        Q_UNUSED(obj);
    } else if (t == QStringLiteral("pong")) {

    } else if (t == QStringLiteral("change")) {
        // JS 修改同步回 C++ 镜像
        text_ = obj.value(QStringLiteral("text")).toString();
    } else if (t == QStringLiteral("ready")) {
        qDebug() << "WebEditor: ready handshake";
        // JS 监听器就绪: 此时重放状态才可靠送达
        replayState();
    }
}

void WebCodeEditor::replayState()
{
    setDarkMode(dark_);
    setLanguage(langId_);
    setPlainText(text_);
    setReadOnly(readOnly_);
    setLineNumbers(showLineNumbers_);
    setFontSize(fontSize_);
    setRegionHighlights(regions_);
    // 内容加载完成: 指示页面关闭载入动画 (动画从网页载入起持续播放)
    QJsonObject m;
    m[QStringLiteral("t")] = QStringLiteral("hideLoader");
    sendToJs(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact)));
}

void WebCodeEditor::navigateToHtml()
{
    if (!impl_->webview) {
        return;
    }
    // data URL 导航 (实测 NavigateToString 页面中 postMessage 不触发
    // WebMessageReceived, 消息通道失效; data URL 导航可正常双向通讯)
    const QByteArray html = QByteArray(kEditorHtml);
    const QString uri = QStringLiteral("data:text/html;charset=utf-8;base64,")
        + QString::fromLatin1(html.toBase64());
    impl_->webview->Navigate(uri.toStdWString().c_str());
}

void WebCodeEditor::updateWebViewBounds()
{
    if (impl_->controller) {
        const int top = toolbar_ ? toolbar_->height() : 0;
        RECT r;
        r.left = 0;
        r.top = top;
        r.right = width();
        r.bottom = height();
        impl_->controller->put_Bounds(r);
    }
}

void WebCodeEditor::activatePlaceholder()
{
    if (placeholder_) {
        placeholder_->setGeometry(rect());
        placeholder_->show();
    }
}

void WebCodeEditor::sendToJs(const QString& json)
{
    if (impl_->ready && impl_->webview) {
        impl_->webview->PostWebMessageAsJson(json.toStdWString().c_str());
    }
}

void WebCodeEditor::setLanguage(const QString& langId)
{
    langId_ = langId;
    QJsonObject m;
    m[QStringLiteral("t")] = QStringLiteral("setLang");
    m[QStringLiteral("lang")] = langId;
    sendToJs(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact)));
}

QString WebCodeEditor::language() const
{
    return langId_;
}

void WebCodeEditor::setPlainText(const QString& text)
{
    text_ = text;
    QJsonObject m;
    m[QStringLiteral("t")] = QStringLiteral("setText");
    m[QStringLiteral("text")] = text;
    sendToJs(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact)));
    scrollToTop();
}

QString WebCodeEditor::toPlainText() const
{
    return text_;
}

void WebCodeEditor::setReadOnly(bool ro)
{
    readOnly_ = ro;
    QJsonObject m;
    m[QStringLiteral("t")] = QStringLiteral("setReadOnly");
    m[QStringLiteral("ro")] = ro;
    sendToJs(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact)));
}

bool WebCodeEditor::isReadOnly() const
{
    return readOnly_;
}

void WebCodeEditor::setLineNumbers(bool on)
{
    showLineNumbers_ = on;
    QJsonObject m;
    m[QStringLiteral("t")] = QStringLiteral("setLineNumbers");
    m[QStringLiteral("on")] = on;
    sendToJs(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact)));
}

void WebCodeEditor::setFontSize(int pt)
{
    fontSize_ = pt;
    QJsonObject m;
    m[QStringLiteral("t")] = QStringLiteral("setFontSize");
    m[QStringLiteral("pt")] = pt;
    sendToJs(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact)));
}

void WebCodeEditor::setTabWidth(int spaces)
{
    QJsonObject m;
    m[QStringLiteral("t")] = QStringLiteral("setTabWidth");
    m[QStringLiteral("spaces")] = qBound(1, spaces, 8);
    sendToJs(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact)));
}

void WebCodeEditor::setDarkMode(bool dark)
{
    dark_ = dark;
    QJsonObject m;
    m[QStringLiteral("t")] = QStringLiteral("setTheme");
    m[QStringLiteral("theme")] = dark ? QStringLiteral("dark")
                                      : QStringLiteral("light");
    sendToJs(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact)));
}

void WebCodeEditor::setRegionHighlights(const QVector<RegionHighlight>& regions)
{
    regions_ = regions;
    QJsonArray arr;
    for (const auto& r : regions) {
        QJsonObject o;
        o[QStringLiteral("start")] = r.startLine;
        o[QStringLiteral("end")] = r.endLine;
        o[QStringLiteral("color")] = r.color;
        o[QStringLiteral("sc")] = r.startColumn;
        o[QStringLiteral("ec")] = r.endColumn;
        o[QStringLiteral("light")] = r.colorLight;
        arr.append(o);
    }
    QJsonObject m;
    m[QStringLiteral("t")] = QStringLiteral("setRegions");
    m[QStringLiteral("regions")] = arr;
    sendToJs(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact)));
}

void WebCodeEditor::registerHighlighter(ICodeHighlighter* h)
{
    // Web 版高亮由内嵌 JS 驱动 (扩展接口一致, 外部驱动在 Web 版不适用)
    Q_UNUSED(h);
}

void WebCodeEditor::addAction(const EditorAction& action)
{
    actions_.append(action);
    if (!toolbar_) {
        toolbar_ = new QToolBar(this);
        toolbar_->setMovable(false);
        toolbar_->setIconSize(QSize(16, 16));
        toolbar_->setStyleSheet(QStringLiteral(
            "QToolBar { background: #2b2f36; border: none; spacing: 2px; }"
            "QToolButton { color: #d8dce2; padding: 2px 8px; }"));
        toolbar_->show();
        updateWebViewBounds();
    }
    auto* btn = toolbar_->addAction(action.text);
    btn->setToolTip(action.tooltip);
    connect(btn, &QAction::triggered, this,
        [this, action]() { if (action.handler) action.handler(this); });
}

void WebCodeEditor::scrollToTop()
{
    QJsonObject m;
    m[QStringLiteral("t")] = QStringLiteral("scrollTop");
    sendToJs(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact)));
}

void WebCodeEditor::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateWebViewBounds();
    if (placeholder_) {
        placeholder_->setGeometry(rect());
    }
}

void WebCodeEditor::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    ensureWebView();
}

ICodeEditor* createWebCodeEditor(QWidget* parent)
{
    return new WebCodeEditor(parent);
}

} // namespace HiBerGUI

#include "web_code_editor.moc"
