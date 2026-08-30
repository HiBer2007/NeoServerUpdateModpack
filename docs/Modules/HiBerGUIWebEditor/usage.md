# HiBerGUIWebEditor 使用文档

## 快速开始

### 1. CMake 链接

```cmake
# 根 CMakeLists.txt 已 add_subdirectory(modules/HiBerGUIWebEditor)
target_link_libraries(myapp PRIVATE
    HiBerGUILibrary      # ICodeEditor / createCodeEditor / registerCodeEditorFactory
    HiBerGUIWebEditor    # WebCodeEditor / createWebCodeEditor
)
```

参考宿主 `modules/EditorDemo/CMakeLists.txt`：`editor_demo` 同时 PRIVATE 链接 `HiBerGUILibrary` + `HiBerGUIWebEditor` + `Qt6::Core` + `Qt6::Widgets`。

### 2. 部署 WebView2Loader.dll

链接宿主才需要：把 `vcpkg_installed/x64-windows/(debug/)bin/WebView2Loader.dll` 拷入输出目录（EditorDemo 用 POST_BUILD `copy_if_different` 实现）；目标机需已安装 WebView2 Runtime。

### 3. 注册 Web 工厂（可选）

`createCodeEditor(CodeEditorKind::Web, parent)` 依赖工厂注册；注册代码逐字摘自 `modules/EditorDemo/src/main.cpp`：

```cpp
#include "code_editor_interface.h"    // registerCodeEditorFactory / CodeEditorKind / createCodeEditor
#include "web_code_editor.h"          // createWebCodeEditor

registerCodeEditorFactory(CodeEditorKind::Web,
    [](QWidget* parent) -> ICodeEditor* {
        return createWebCodeEditor(parent);
    });
```

也可以不经工厂，直接 `new HiBerGUI::WebCodeEditor(parent)`。

## 公共 API

全部签名逐字摘自 `modules/HiBerGUIWebEditor/include/web_code_editor.h`；接口定义逐字摘自 `modules/HiBerGUILibrary/include/code_editor_interface.h`。

### 类 `WebCodeEditor`

`class WebCodeEditor : public QWidget, public ICodeEditor`（`Q_OBJECT`，namespace `HiBerGUI`）。

| 成员 | 签名 | 说明 |
|------|------|------|
| 构造 | `explicit WebCodeEditor(QWidget* parent = nullptr)` | 创建编辑器；WebView2 初始化延后到首次 `showEvent` |
| 析构 | `~WebCodeEditor() override` | `controller->Close()` + ComPtr `Detach()`，不 `CoUninitialize`（异步关闭交由进程退出回收） |
| widget | `QWidget* widget() override { return this; }` | 返回编辑器自身（接口约定） |
| setLanguage | `void setLanguage(const QString& langId) override` | 设置语言标识，驱动 JS 侧高亮（见注意事项语言支持） |
| language | `QString language() const override` | 返回 `langId_`（初始 `"plain"`） |
| setPlainText | `void setPlainText(const QString& text) override` | 设置全文并**附带 `scrollToTop()`** |
| toPlainText | `QString toPlainText() const override` | 返回 C++ 侧镜像文本（JS 修改经 `change` 消息同步回） |
| setReadOnly | `void setReadOnly(bool ro) override` | 只读开关 |
| isReadOnly | `bool isReadOnly() const override` | 查询只读状态 |
| setLineNumbers | `void setLineNumbers(bool on) override` | 行号栏开关（初始开） |
| setFontSize | `void setFontSize(int pt) override` | 字号（**pt**，JS 按 96dpi 换算 px） |
| setTabWidth | `void setTabWidth(int spaces) override` | Tab 宽度，**钳位 1–8**（`qBound(1, spaces, 8)`） |
| setDarkMode | `void setDarkMode(bool dark) override` | 深/浅色主题（初始深色；WebView2 就绪时改为跟随宿主调色板） |
| setRegionHighlights | `void setRegionHighlights(const QVector<RegionHighlight>& regions) override` | 行区间背景标记；整行或列区间（`startColumn/endColumn` ≥0 时） |
| registerHighlighter | `void registerHighlighter(ICodeHighlighter* h) override` | **空操作**：Web 版高亮由内嵌 JS 驱动，外部高亮器不适用 |
| addAction | `void addAction(const EditorAction& action) override` | 追加工具条按钮（首个动作时创建 `QToolBar`），触发时调用 `action.handler(this)` |
| scrollToTop | `void scrollToTop() override` | 滚动到顶部 |
| usingWebView | `bool usingWebView() const` | 非接口方法；WebView2 是否已就绪（`impl_->ready`） |

### COM 回调入口（public，内部使用，宿主勿直接调用）

| 成员 | 签名 | 说明 |
|------|------|------|
| onEnvCreated | `void onEnvCreated(ICoreWebView2Environment* env)` | 环境创建完成回调 |
| onControllerCreated | `void onControllerCreated(ICoreWebView2Controller* controller)` | 控制器创建完成回调（随后导航 HTML） |
| onMessageReceived | `void onMessageReceived(const QString& json)` | JS → C++ 消息（`ready/change/log/echo/pong`） |
| onNavigationCompleted | `void onNavigationCompleted(bool ok)` | 导航完成回调，负责注入兜底监听器与重放状态 |

### 工厂函数（`web_code_editor.h`）

| 符号 | 签名 | 说明 |
|------|------|------|
| createWebCodeEditor | `ICodeEditor* createWebCodeEditor(QWidget* parent)` | Web 版工厂，供宿主注册/直接实例化 |

### 相关类型（`code_editor_interface.h`，本模块复用）

`ICodeEditor`（抽象接口，上文 15 项）、`CodeEditorKind`（`Qt` / `Web`）、`RegionHighlight`（`startLine/endLine/color/tag/startColumn/endColumn/colorLight`）、`EditorAction`（`id/text/tooltip/handler`）、`ICodeHighlighter`、`HighlightSpan`、`editorThemeColors(bool)` 等，与 Qt 版共用；`createCodeEditor(CodeEditorKind, QWidget*)` 未注册工厂时：`Qt` 回退内置实现、`Web` 返回 `nullptr`。

## 典型用法

### 1. 经接口编排（与 Qt 版同构，可切换后端）

```cpp
registerCodeEditorFactory(CodeEditorKind::Web,
    [](QWidget* parent) -> ICodeEditor* {
        return createWebCodeEditor(parent);
    });

ICodeEditor* ed = createCodeEditor(CodeEditorKind::Web, this);
if (!ed) { /* Web 工厂未注册 */ }
ed->setLanguage(QStringLiteral("json"));
ed->setPlainText(QStringLiteral("{\n  \"a\": 1\n}"));
ed->setReadOnly(false);
ed->setDarkMode(true);
ui->splitter->addWidget(ed->widget());
```

### 2. 直接实例化 + 初始化状态/动作

```cpp
auto* ed = new HiBerGUI::WebCodeEditor(centralWidget());
ed->setLanguage(QStringLiteral("toml"));
ed->setTabWidth(4);
ed->setFontSize(11);
ed->setRegionHighlights(QVector<HiBerGUI::RegionHighlight>{
    { 1, 3, QStringLiteral("#2f6b31"), QStringLiteral("tracked") },
});
ed->addAction({ QStringLiteral("save"), QStringLiteral("保存"),
    QStringLiteral("保存当前文件"),
    [](QWidget* w) { /* 处理保存 */ } });
layout->addWidget(ed);
```

### 3. 读取内容与就绪探测

```cpp
QString text = ed->toPlainText();       // JS 编辑同步回 C++ 镜像
if (ed->usingWebView()) {
    // WebView2 可用；缺失运行时此值为 false，界面显示错误占位
}
```

## 注意事项

- **WebView2 运行时**：目标机缺失或环境/控制器创建失败 → 显示占位 `QLabel`（「无法创建 WebView2 编辑器：系统未安装 WebView2 运行时。」），不崩溃；用 `usingWebView()` 探测就绪。仅链接本库的程序才依赖 `WebView2Loader.dll`。
- **异步初始化**：`showEvent` 才触发 `ensureWebView()`（`GetAvailableCoreWebView2BrowserVersionString` 探测 → `CreateCoreWebView2EnvironmentWithOptions` → `CreateCoreWebView2Controller` → 导航）；就绪前 `sendToJs` 静默丢弃消息。**最终一致性靠 JS `ready` 握手后的 `replayState()` 全量重放**（重建 environment/controller 成功路径均有重放兜底），因此构造后立刻发起的设置会在就绪后生效，但不要在就绪前依赖 JS 侧反馈（如 `toPlainText` 只能读到 C++ 镜像，`change` 消息就绪后才回传）。
- **外部高亮器不生效**：`registerHighlighter` 为空操作；自定义高亮在 Web 版不可用（Qt 版才有）。语言规则硬编码于内嵌 JS：`json`（带引号 key）、`yaml/snbt`（裸 key+冒号）、`toml`（段头+key=）、`properties`（key[=:]）；字符串/数字/关键字（`true/false/null`，yaml 另含 `yes/no/on/off`）。**`txt` 无 key 高亮特判**，仅走基线关键字规则，与 Qt 版语言支持（含 `txt` 内置高亮）存在差异。
- **消息协议陷阱**：JS 侧 `postMessage` 必须传对象而非 `JSON.stringify` 字符串——`get_WebMessageAsJson` 会把后者读成带引号的字符串字面量，C++ 侧 `QJsonDocument::fromJson` 得到空对象。
- **初始化主题**：`onControllerCreated` 时 `dark_` 被重置为 `palette().color(QPalette::Window).lightness() < 128`（跟随宿主调色板），之后显式 `setDarkMode` 覆盖。
- **字号单位**：`setFontSize` 传 pt；JS 按 `px = round(pt*96/72)` 换算，字号变化后字符宽缓存失效重测。
- **Tab 宽度钳位**：`setTabWidth` 钳位 1–8；JS 端默认 4。
- **setPlainText 附带滚动**：`setPlainText` 末尾自动 `scrollToTop()`，与 Qt 版行为存在差异，按需注意。
- **资源/生命周期**：每个实例的用户数据目录为 `%TEMP%/NSUM-webeditor-<pid>`；析构为异步 `Close` + `Detach`，不 `CoUninitialize`、不手动 `delete impl_` 之外的 COM 清理——退出期竞态交由进程退出回收（与 AGENTS.md 第二十五章 PowerHelper 同款约定；回调 lambda 捕获宿主用 `QPointer`，跨线程经 `Qt::QueuedConnection` 调度）。
- **WebView2 SDK 环境**：vcpkg `webview2` 包（target `unofficial::webview2::webview2`）；本 SDK 的 `wrl/client.h` 无 `Make` 模板，回调对象须 `ComPtr<I>(new Handler(this))` 显式构造（模块内 `ComBase` 基类已封装引用计数）。

## 相关文档

- **接口契约**：`modules/HiBerGUILibrary/include/code_editor_interface.h`（`ICodeEditor` / `CodeEditorKind` / `RegionHighlight` / `EditorAction` / `ICodeHighlighter` / 工厂函数）。
- **Qt 版实现**：`modules/HiBerGUILibrary/src/code_editor.cpp`（`CodeEditor` 与工厂注册/回退逻辑）。
- **样例宿主**：`modules/EditorDemo/`（`main.cpp` 注册 Web 工厂、`--web` 启动即切 Web 页、Qt/Web 双 Tab 对比；CMakeLists 含 `WebView2Loader.dll` 部署）。
- **模块说明**：`docs/Modules/HiBerGUIWebEditor/README.md`（设计目标/模块边界/依赖/构建集成）。
- **WebView2 通用陷阱**：`AGENTS.md`「WebView2 渲染（第二十五章）」（PowerHelper 同款 SDK 的回调、析构、运行时回退经验）。