# HiBerGUIWebEditor 说明文档

## 概述

HiBerGUIWebEditor 是 HiBerGUILibrary 编辑器套件的 **Web 版**实现：基于系统 WebView2（Edge 内核）以网页（内嵌 HTML/JS）渲染的代码编辑器，与 Qt 版 `CodeEditor` 实现同一个 `ICodeEditor` 接口（定义于 HiBerGUILibrary 的 `code_editor_interface.h`），宿主可在不改变调用方代码的前提下替换编辑器后端。模块仅产出一个 STATIC 库 `HiBerGUIWebEditor`；只有链接该库的程序才依赖 WebView2（`WebView2Loader.dll`），不链接则零依赖；系统缺少 WebView2 运行时或初始化失败时显示错误占位而非崩溃。

## 设计目标 — 与 Qt 版的关系/取舍

- **同一抽象、两种后端**：`CodeEditorKind::Qt`（纯 C++/Qt，`CodeEditor`）与 `CodeEditorKind::Web`（`WebCodeEditor`）实现同一 `ICodeEditor` 接口，宿主经 `createCodeEditor(kind, parent)` 编排，切换后端不改业务代码。
- **前端渲染取舍**：语法高亮、行号、滚动同步等视觉逻辑全部内嵌在 JS 侧（VS Code 配色逻辑），换来与 Qt 版近乎一致的观感；代价是**外部 `ICodeHighlighter` 扩展在 Web 版不适用**（`registerHighlighter` 为空操作），语言规则硬编码于内嵌 JS，不提供语言定义注册。
- **依赖隔离取舍**：Web 依赖（`unofficial::webview2::webview2`）为 PRIVATE 链接，且走**系统 WebView2 运行时**而非 Qt WebEngine——不链接本库的目标不引入任何 WebView2 负担；链接宿主部署时需随带 `WebView2Loader.dll` 与运行时。
- **异步初始化取舍**：WebView2 环境/控制器/导航均为异步回调（`showEvent` 触发创建），状态一致性靠 JS `ready` 握手后的 `replayState()` 全量重放保证，而非同步就绪。
- **当前用途**：样例宿主 `EditorDemo` 将其与 Qt 版并列对比（`--web` 启动即切 Web 页）；宿主编排默认走 `createCodeEditor(CodeEditorKind::Qt)`，不引入 Web 依赖。

## 模块边界

**做什么**

- 提供 `WebCodeEditor`（`QWidget` + `ICodeEditor`）与工厂函数 `createWebCodeEditor`。
- 内嵌 JS 语法高亮：`json` / `yaml` / `toml` / `snbt` / `properties`（+ `plain` 基线关键字 `true/false/null`）。
- 行号栏、只读、深/浅色主题、字号（pt）、Tab 宽度、区域标记（整行背景 + 列区间 overlay）、工具条动作、光标行高亮、滚动同步。
- 缺失 WebView2 运行时（或环境/控制器创建失败）时显示错误占位，不崩溃。
- 供宿主经 `registerCodeEditorFactory(CodeEditorKind::Web, …)` 注册 Web 工厂。

**不做什么**

- 不执行外部高亮器扩展：`registerHighlighter` 在 Web 版为空操作（扩展接口一致，但外部驱动不适用）。
- 不提供语言定义注册：语言高亮规则硬编码于内嵌 JS（对应 HiBerGUILibrary 的 `registerLanguageDef` 亦为占位）。
- 不依赖 Qt WebEngine / Qt WebChannel：渲染走系统 WebView2 运行时。
- 不自行注册全局工厂：工厂注册必须由链接本库的宿主显式调用（见 `EditorDemo/src/main.cpp`）。
- 不参与宿主编排默认路径：默认 `createCodeEditor(CodeEditorKind::Qt)` 不触发本模块代码。

## 依赖关系

以 `modules/HiBerGUIWebEditor/CMakeLists.txt` 为准：

| 依赖 | 链接类型 | 用途 |
|------|---------|------|
| `Qt6::Core` | PUBLIC | Qt 基础（`QCoreApplication` 队列调度 COM 回调等） |
| `Qt6::Widgets` | PUBLIC | `QWidget` / `QToolBar` / `QLabel` 等控件 |
| `HiBerGUILibrary` | PRIVATE | `code_editor_interface.h`：`ICodeEditor`、`RegionHighlight`、`EditorAction`、`ICodeHighlighter`、`CodeEditorKind` 等接口与类型 |
| `unofficial::webview2::webview2` | PRIVATE | WebView2 SDK（vcpkg `webview2` 包）：`WebView2.h`、`WebView2Loader.dll` |

- CMake 要求 `find_package(unofficial-webview2 CONFIG REQUIRED)`（`WebView2Loader.dll` 由宿主按需部署，参考 `EditorDemo` CMakeLists 的 POST_BUILD `copy_if_different`）。
- 构建集成：根 `CMakeLists.txt:137` 经 `add_subdirectory(modules/HiBerGUIWebEditor)` 纳入构建。
- 注意：`HiBerGUILibrary` 虽为 PRIVATE 链接，其 `include/` 目录经 `target_include_directories` PUBLIC 暴露（`web_code_editor.h` 需包含 `code_editor_interface.h`）；宿主若要调用该接口下的自由函数（`createCodeEditor` 等）仍需自行链接 `HiBerGUILibrary`。
- 运行时依赖：目标机需安装 WebView2 Runtime；用户数据目录为 `%TEMP%/NSUM-webeditor-<pid>`（`QDir::tempPath()` + `applicationPid()`）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `include/web_code_editor.h` | `WebCodeEditor` 类声明（含公开 COM 回调入口）、`createWebCodeEditor` 工厂、内部状态成员 |
| `src/web_code_editor.cpp` | 实现：WebView2 COM 封装与回调、内嵌编辑器 HTML/JS（`kEditorHtml`）、`sendToJs`/`replayState` 状态同步、Qt 侧状态镜像；末尾 `#include "web_code_editor.moc"` |
| `CMakeLists.txt` | STATIC 目标 `HiBerGUIWebEditor` 定义与依赖声明 |

## 构建集成

- **目标类型**：STATIC 库（`add_library(HiBerGUIWebEditor STATIC …)`），非 DLL。
- **链接方式**：宿主 `target_link_libraries(<host> PRIVATE HiBerGUIWebEditor)`；由于是静态归档，WebView2/`HiBerGUILibrary` 依赖不会自动传递给不链接本库的目标。样例宿主 `editor_demo` 同时 PRIVATE 链接 `HiBerGUILibrary` + `HiBerGUIWebEditor` + `Qt6::Core` + `Qt6::Widgets`。
- **部署**：仅链接本库的程序才依赖 `WebView2Loader.dll`（从 `vcpkg_installed/x64-windows/(debug/)bin/` 拷入输出目录）；目标机需安装 WebView2 Runtime。
- **MOC**：模块启用 `CMAKE_AUTOMOC`（`Q_OBJECT`），实现文件按项目惯例 `#include "web_code_editor.moc"`。

## 命名空间与公共符号

命名空间：**`HiBerGUI`**（与 HiBerGUILibrary 共用，无独立命名空间）。

| 符号 | 类别 | 说明 |
|------|------|------|
| `class WebCodeEditor` | 类（`QWidget` + `ICodeEditor`，`Q_OBJECT`） | Web 版编辑器主体 |
| `ICodeEditor* createWebCodeEditor(QWidget* parent)` | 自由函数 | Web 版工厂，供宿主注册/直接实例化 |
| `bool WebCodeEditor::usingWebView() const` | 成员 | WebView2 是否已就绪（非接口方法） |
| `onEnvCreated / onControllerCreated / onMessageReceived / onNavigationCompleted` | 公开 COM 回调入口 | 供内部处理器跨线程回调，宿主不应直接调用 |

接口类型（`RegionHighlight`、`EditorAction`、`ICodeHighlighter`、`CodeEditorKind` 等）与 `registerCodeEditorFactory` / `createCodeEditor` 均来自 HiBerGUILibrary 的 `code_editor_interface.h`，详见 usage.md 与 HiBerGUILibrary 文档。