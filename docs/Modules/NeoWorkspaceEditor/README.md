# NeoWorkspaceEditor 说明文档

## 概述

NeoWorkspaceEditor 是 NeoServerUpdateModpack 的**工作区编辑器 EXE 应用**（产物 `NeoWorkspaceEditor.exe`），为整合包内容提供可视化 IDE：直接编辑仓库根 `workspace.json`、分支配置与整合包目录树，并把所有变更落回 Git。它复用 GUIWorker 的领域编辑器（`RepoEditor` / `BranchEditor` / `ModpackContentIde`）与 HiBerGUILibrary 的通用组件（`GitPanel`、`ProgressCard`、CodeEditor 等），自身只负责窗口装配、菜单/工具栏、会话状态持久化与 Git 落地。模块的全部头文件位于 `src/` 下（**无 `include/` 目录**）。

## 设计目标

- **配置即代码**：`workspace.json` 是整合包构建的唯一事实来源，提供 GUI 编辑 + 自动 `git add`/提交，消除手工改 JSON 与手敲 Git 命令。
- **仓库树 / 输出树双视角**：整合包内容 IDE 同时呈现仓库文件树与构建输出预览树，支持普通文件↔配置文件标记、serverconfig 同步规则、指针转换等。
- **Git 全流程内建**：打开/新建/克隆/分叉仓库、分支管理（创建/切换/属性配置/合并提交）、SSH 密钥管理、完整性检查、`.gitignore` 图形化编辑。
- **故障自愈**：目录缺 `workspace.json` 时从 Git 历史恢复或新建默认配置；陌生仓库（dubious ownership）询问信任。
- **开发期诊断**：控制台子系统 + 加载期叠层 TUI + 崩溃测试标签（CrashLabel）+ `_CrtCheckMemory` 堆检查点，便于复现与定位 Qt 布局失配类崩溃。

## 模块边界

**做什么**

- 加载/保存 `workspace.json`；维护每分支配置文件 `<repo>/branch_config/<name>.json`。
- 三标签页装配：仓库设置（`RepoEditor`）、分支管理（`BranchEditor`）、整合包内容（`ModpackContentIde`）+ 左侧 Git 面板（`GitPanel`）。
- 菜单/工具栏/状态栏；最近打开列表；窗口分割与两树列布局持久化（`config/custom/editor.ini`）。
- Git 领域操作：克隆/新建/分叉/切换分支/分支属性（描述与隐藏）/合并提交（squash）/完整性检查；`.gitignore` 图形化（GitIgnoreMarkup::GitIgnoreDialog）与直接编辑（HiBerGUI CodeEditor）。
- 崩溃捕获（HiBerCTM）与日志（CLogger，`workspace_editor.log`，logger 名 `editor`）。
- 帮助文档入口：`帮助 → 帮助文档` 经 `PowerHelper::Bridge::launchReader(defaultDocsDir())` 拉起 PowerHelper。

**不做什么**

- 不做构建/导出/下载：这些属于主程序（`NeoServerUpdateModpack`）与 NeoBuild/NeoCLI 的职责。
- 不解析配置内容、不替用户做功能验收：编辑器只负责编辑与 Git 落地。
- 不参与主配置的插件扫描之外的东西：编辑器扩展（parser/pointer 扩展 DLL）通过 `ModpackContentIde` 的扩展注册表加载与重建「扩展」菜单，模块自身不定义插件契约。

## 依赖关系

| 依赖 | 类型 | 用途 |
|------|------|------|
| GitIgnoreMarkup | STATIC 库 | `GitIgnoreDialog`（.gitignore 图形化/直接编辑对话框，`GitIgnoreMarkup::` 命名空间） |
| NeoCore | STATIC 库 | 基础库；CLogger 经 CommonLoggerCPP 传递 |
| NeoWorkspace | STATIC 库 | `GitOperations`（clone/init/checkout/SSH/信任等 Git 封装） |
| NeoBuild | STATIC 库 | 构建/预览相关引擎（内容 IDE 依赖） |
| GUIWorker | STATIC 库 | `RepoEditor` / `BranchEditor` / `ModpackContentIde`（领域编辑器） |
| HiBerGUILibrary | STATIC 库 | `GitPanel`、`ProgressCard`、CodeEditor（.gitignore 直接编辑）、DeepTreeBehavior 等 |
| PowerHelperBridge | STATIC 库 | `PowerHelper::Bridge::launchReader` 帮助文档入口 |
| CrashTrackerHandleLib | STATIC 库 | `HiBerCTM::` 崩溃捕获/CRT 钩子 |
| Qt6::Core / Qt6::Widgets | vcpkg/系统 Qt | GUI 与应用基础 |
| libzippp::libzippp + zip.lib | vcpkg | ZIP 相关（内容 IDE 预览/导入依赖） |

**部署形态**：`NeoWorkspaceEditor.exe`（控制台子系统，非 WIN32 标志），由根 `neo_deploy` 目标复制进 `build/deploy/`，并通过 windeployqt 附 Qt DLL。独立可运行，不依赖 GUIWorker/HiBerGUILibrary DLL（均为 STATIC 静态链入）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | target 定义；`NSUM_ASAN` 选项；windeployqt POST_BUILD；`nsum_add_version_info` |
| `src/main.cpp` | 入口：QApplication + 崩溃捕获 + CLogger::Init（`workspace_editor.log`, `editor`）+ 终端策略（setupConsoleUtf8/shouldKeepConsole）+ 加载期 TUI（`nsum_tui::EditorTui`）+ QCommandLineParser（位置参数 `[file]`）+ 装配 `EditorWindow` |
| `src/editor_window.h` / `.cpp` | 主窗口：三标签 + GitPanel 布局、全部菜单/工具栏/状态栏、workspace 加载/保存、git 操作槽、完整性检查、.gitignore 维护、扩展菜单重建、布局持久化 |
| `src/editor_tui.h` / `.cpp` | 加载期叠层终端 UI（命名空间 `nsum_tui`）：`TuiLogSink`（spdlog sink 缓冲）+ `EditorTui`（拼接字卡片/日志滚动/进度条渲染） |
| `src/branch_meta_dialog.h` / `.cpp` | 分支属性配置对话框：编辑各分支 workspace.json 顶层 `description`/`hidden`，checkout→改→commit→push→切回 |
| `src/app.rc` | 版本资源（配合 nsum_add_version_info 生成的 version.rc） |

## 构建集成

- **CMake target**：`NeoWorkspaceEditor`，`add_executable`（**控制台子系统**，便于终端日志调试，与主程序同款 `holdOrReleaseConsole` 策略）。
- **链接**：PRIVATE 链接 `GitIgnoreMarkup`、`NeoCore`、`NeoWorkspace`、`NeoBuild`、`GUIWorker`、`HiBerGUILibrary`、`PowerHelperBridge`、`CrashTrackerHandleLib`、`Qt6::Core`、`Qt6::Widgets`、`libzippp::libzippp`、`zip.lib`。
- **包含目录**：`src`、`../NeoCore/include`、根 `../../src`（`install_config.h`）、`../GUIWorker/include`、`../HiBerGUILibrary/include`、`../PowerHelper/bridge/include`、`../NeoWorkspace/include`、`../NeoBuild/include`。
- **版本资源**：`nsum_add_version_info(NeoWorkspaceEditor "NSUM 仓库管理器 - 工作区配置编辑器" "NSUM构建工具")`。
- **部署产物**：由根 `neo_deploy` 复制 `NeoWorkspaceEditor.exe` 至 `build/deploy/`，windeployqt 附 Qt 运行库；发布包内还含 `parsers/`、`pointers/`、`exporters/`、`editor/extension/` 等插件目录（编辑器扩展经「扩展」菜单枚举）。
- **ASAN**：`NSUM_ASAN=ON` 时对 NeoCore/NeoBuild/NeoWorkspace/GUIWorker/HiBerGUILibrary/CrashTrackerHandleLib 与本 target 追加 `/fsanitize=address`。

## 命名空间与公共符号

| 符号 | 位置 | 说明 |
|------|------|------|
| `EditorWindow` | 全局（editor_window.h） | 主窗口类，`public: loadWorkspace/saveWorkspace/saveWorkspaceAs/isModified` |
| `enum class RepoSource { Local, RemoteCache, Clone, New }` | 全局（editor_window.h） | 工作区加载来源枚举 |
| `BranchMetaDialog` | 全局（branch_meta_dialog.h） | 分支属性配置对话框 |
| `nsum_tui::EditorTui` / `nsum_tui::TuiLogSink` | editor_tui.h | 加载期叠层 TUI 组件 |
| `main(int, char**)` | main.cpp | 入口；`--help/--version` 由 QCommandLineParser 提供 |

> 领域层符号（`GUIWorker::RepoEditor` 等）属 GUIWorker 模块，本模块仅消费；Git 领域符号（`NeoWorkspace::GitOperations`）属 NeoWorkspace 模块。