# PowerHelper 说明文档

## 概述

PowerHelper 是 NeoServerUpdateModpack 的 Markdown 文档阅读器：GUI 以 **WebView2**（系统 Microsoft Edge 运行时）渲染真实 HTML/CSS（表格/图片/链接/深色模式），系统缺失 WebView2 时自动回退 Qt 内置 `QTextBrowser`。既可独立发布（GUI 阅读器 + CLI 终端渲染），也可经 `PowerHelper::Bridge::launchReader` 作为宿主程序的帮助文档驱动器，或内嵌 `MarkdownViewer` 组件到现有 GUI。渲染基于 cmark-gfm（CommonMark + GFM：表格/删除线/自动链接/任务列表/标签过滤）。模块位于 `modules/PowerHelper/`，分 core / app / bridge 三部分。

## 设计目标

三部分结构的意图：**渲染核心、外壳、集成工具分离**，让「独立发布」与「宿主集成」两条消费路径各取所需、互不拖累。

| 部分 | CMake 目标 | 职责 | 形态 |
|------|-----------|------|------|
| core | `PowerHelperCore` | 渲染引擎（MD → HTML / ANSI / QTextDocument）+ 可嵌入阅读器视图 MarkdownViewer + 文档组扫描 | 库：默认 **SHARED**（DLL），独立模式 **STATIC** |
| app | `PowerHelper` | 外壳可执行：GUI 阅读器窗口（ReaderWindow）+ CLI 命令（render/toc/group） | **EXE**（控制台子系统） |
| bridge | `PowerHelperBridge` | 集成工具库：定位外壳 EXE、拉起阅读器、默认文档目录、目标类型分类 | **STATIC** 库 |

- **独立发布 vs 集成**：core 承载全部渲染逻辑，app 只是可选外壳。集成方（主程序、编辑器）**不链 core**，只链零依赖的 STATIC bridge，以 QProcess 拉起独立进程中的外壳 EXE——渲染与 UI 与宿主进程隔离，崩溃/COM 生命周期问题不影响宿主；需要真正内嵌时才直接链 `PowerHelperCore` 使用 `MarkdownViewer`。
- **独立模式**：`POWERHELPER_STANDALONE_ONLY=ON` 时 core 编译为静态库并入 EXE（`POWERHELPER_STATIC` 定义），产物为单个自包含 EXE（静态 Qt），适合独立分发。

## 模块边界

**做什么**
- Markdown 三种渲染：`renderToHtml`（WebView2 主管线）、`renderToTerminal`（ANSI 终端文本）、`renderToDocument`（QTextDocument，回退渲染）
- 文档组扫描：递归收集 `.md`、提取标题与 TOC（`scanDocGroup`）
- WebView2 GUI 阅读器（含缺失回退）+ 可嵌入 `MarkdownViewer` QWidget
- CLI：`render` / `toc` / `group` 命令、`--json` 标记块协议、`--anchor` 锚点定位
- Bridge：拉起外壳阅读器、定位 exe / 默认 docs 目录、目标类型分类

**不做什么**
- 不做 Markdown 编辑/保存（纯阅读器）
- 不做全文搜索/索引（仅标题级 TOC）
- 不捆绑 WebView2 运行时——依赖系统组件，缺失时回退 Qt 渲染
- 不提供多语言切换与主题定制 API（深色模式自动跟随系统）

## 依赖关系

目标直接依赖（以 `modules/PowerHelper/CMakeLists.txt` 为准）：

| 目标 | 直接依赖 | 说明 |
|------|---------|------|
| `PowerHelperCore` | `Qt6::Core`、`Qt6::Widgets`、`libcmark-gfm`、`libcmark-gfm-extensions`；WIN32 另加 `unofficial::webview2::webview2` | cmark-gfm 渲染引擎；webview2 SDK（vcpkg，自身依赖 wil） |
| `PowerHelperBridge` | `Qt6::Core`、`Qt6::Widgets` | 仅 Qt，零渲染依赖，集成方链入代价最低 |
| `PowerHelper` (EXE) | `PowerHelperCore`、`PowerHelperBridge`、`NeoCore`、`CrashTrackerHandleLib`、`Qt6::Core`、`Qt6::Widgets` | NeoCore（基础库）、CrashTrackerHandleLib（CLI 崩溃以 `CrashTracker --cli` 文字报告接管） |

`find_package` 要求：`cmark-gfm`、`cmark-gfm-extensions`（REQUIRED）；WIN32 时 `unofficial-webview2`（CONFIG REQUIRED）。

**反向依赖（PowerHelper 被谁消费）**：

| 消费方 | 集成点 | 方式 |
|--------|--------|------|
| 主程序 | 完成页（done_page）警告/失败时「打开帮助文档」按钮 | `PowerHelper::Bridge::launchReader(defaultDocsDir())` |
| 主程序 | 状态栏左下角「帮助文档」标签（按当前页面/状态定位章节） | 同上 |
| 编辑器 | 「帮助 → 帮助文档」菜单 | 同上 |
| 其他 GUI | 内嵌 Markdown 渲染 | 链 `PowerHelperCore`，直接用 `MarkdownViewer` |

帮助内容为部署目录下的 `docs/` 文档组（`docs/main/`、`docs/CLI/`、`docs/deploy/` 等）。

## 文件组成

| 部分 | 文件 | 内容 |
|------|------|------|
| core | `core/include/markdown_renderer.h` | `TocEntry` 结构 + 7 个渲染 API（renderToHtml/renderToTerminal/renderToDocument/extractToc/displayWidth/expandTabs/anchorName） |
| core | `core/include/markdown_viewer.h` | `MarkdownViewer` 可嵌入组件（WebView2 + 回退） |
| core | `core/include/doc_group.h` | `DocFileInfo` 结构 + `scanDocGroup` |
| core | `core/include/powerhelper_export.h` | `PH_API` 导出宏（dllexport/dllimport/空） |
| core | `core/src/markdown_renderer.cpp` / `markdown_viewer.cpp` / `doc_group.cpp` | 实现 |
| bridge | `bridge/include/powerhelper_bridge.h` + `bridge/src/powerhelper_bridge.cpp` | Bridge 集成工具 |
| app | `app/main.cpp` / `powerhelper_cli.{h,cpp}` / `reader_window.{h,cpp}` | 外壳 EXE（入口 + CLI + 阅读器主窗口） |
| test | `test/render_test.cpp` | `ph_rendertest` 手动渲染测试（`EXCLUDE_FROM_ALL`） |

## 构建集成

- **core**：默认 `SHARED`（`PH_CORE_LINKAGE`）；`POWERHELPER_STANDALONE_ONLY=ON`（独立预设）时 `STATIC` 并 `add_compile_definitions(POWERHELPER_STATIC)`。导出宏 `powerhelper_export.h`：`POWERHELPER_STATIC` → `PH_API` 置空；否则按 `PowerHelperCore_EXPORTS` 展开 `__declspec(dllexport)` / `__declspec(dllimport)`。
- **bridge**：恒 `STATIC`，头文件 `bridge/include` PUBLIC。
- **app**：恒 **EXE**；**控制台子系统**（CMake 不加 `WIN32` 标志，与主程序同款）——CLI 模式 stdout 原生连接控制台，GUI 模式在 main 内按控制台归属决定保持/释放终端。经 `nsum_add_version_info` 嵌入版本信息与 `PR.ico` 图标。
- **部署产物（常规模式）**：`PowerHelper.exe` + `PowerHelperCore.dll` + `libcmark-gfm.dll` + `libcmark-gfm-extensions.dll`（POST_BUILD `copy_if_different` 到 exe 目录），并运行 `windeployqt` 收集 Qt 运行时。
- **部署产物（独立模式）**：单个自包含 EXE（静态 Qt + 静态 core）；WebView2 为系统组件，无需分发。
- **手动测试**：`ph_rendertest`（`EXCLUDE_FROM_ALL`，链 `PowerHelperCore` + `Qt6::Core`）；构建后若引用了 `PowerHelperCore` 的 DLL/EXE，会同步拷贝 core 与 cmark DLL（WIN32 非独立模式）。

> 头部共享注意：改动 `core/include/*.h` 中的类布局（如 `MarkdownViewer` 成员）后，所有 `new MarkdownViewer` / 链接 `PowerHelperCore` 的目标必须重建（见 usage.md「注意事项」第 7 条）。

## 命名空间与公共符号

全部公共符号位于 **`namespace PowerHelper`**（Bridge 为其子命名空间 `PowerHelper::Bridge`）：

| 类别 | 符号 | 说明 |
|------|------|------|
| 结构 | `TocEntry { int level = 0; QString text; }` | 目录项 |
| 结构 | `DocFileInfo { QString relPath, absPath, title; QVector<TocEntry> toc; }` | 文档组单文档信息 |
| 函数 | `renderToHtml` / `renderToTerminal` / `renderToDocument` / `extractToc` / `displayWidth` / `expandTabs` / `anchorName` | 渲染与文本工具（均 `PH_API`） |
| 函数 | `scanDocGroup(const QString& dir)` | 文档组扫描 |
| 类 | `MarkdownViewer : QWidget` | 可嵌入阅读器（setMarkdown/loadFile/toc/scrollToHeading/scrollToHeadingText/usingWebView；信号 tocChanged/openFileRequested） |
| 子命名空间 | `PowerHelper::Bridge` | `findPowerHelperExe` / `launchReader` / `defaultDocsDir` / `classifyTarget` + `enum class TargetKind { File, Dir, Unknown }` |
| app 侧 | `ReaderWindow : QMainWindow` | 外壳阅读器主窗口（openFile/openGroup/scrollToHeadingText） |
| app 侧 | `int runCli(int argc, char* argv[])` | CLI 入口 |

完整签名见 [usage.md](./usage.md)「公共 API」。