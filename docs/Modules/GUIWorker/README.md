# GUIWorker 说明文档

## 概述

GUIWorker 是项目的 **GUI 功能子系统**（原 NeoGUI，2026-08-07 拆分改名），命名空间 `GUIWorker`，CMake target 为 `GUIWorker`（STATIC）。它由两大部分组成：**主程序 9 页构建向导**（`WizardWindow` 及 `RepoPage/BranchPage/ModpackPage/ExportTypePage/ExportDirPage/ExtraInfoPage/BuildChecklistPage/BuildPage/DonePage`，另支持 CLI `flow gui` 的无界面驱动模式）与**领域编辑器**（整合包内容 IDE `ModpackContentIde`、分支/仓库编辑器、指针转换编辑器、serverconfig 规则编辑器、同步策略/配置文件编辑器等），外加 `BuildToolWindow` 构建工具窗口与 `EditorExtensionRegistry` 编辑器扩展注册表。它把「向导流程 + 领域编辑业务」编排在 Qt6 Widgets 之上，通用控件一律复用 HiBerGUILibrary 组件（头文件内 `using HiBerGUI::X;` 引用模式），自身不实现任何通用控件。

## 设计目标

- **领域与通用分离**：2026-08-07 将通用组件（`animated_progress` / `toast_notification` / `progress_card` / `output_tree_panel` / `repo_tree_panel` / `file_content_editor` / `git_panel` 等）迁入独立的 `HiBerGUILibrary`（namespace `HiBerGUI`，零领域依赖，仅依赖 Qt + nlohmann-json，可独立发布）。GUIWorker 只保留**领域**内容：向导状态机、构建流程编排、分支/仓库/指针/服务器配置等编辑业务。分工边界 = **领域逻辑在 GUIWorker，通用组件在 HiBerGUILibrary**。
- **静态链接为可嵌入库**：GUIWorker 是 STATIC 库，被主程序与 NeoWorkspaceEditor 两个 EXE 直接链接（无 DLL 分发），页面/编辑控件可以作为子控件嵌入宿主布局。
- **向导可无界面驱动**：`WizardWindow` 内置 Flow 模式（`FlowConfig` + `setFlowMode`），配合预填（prefill）与终点收集，支撑 CLI `flow gui` 在纯 GUI 窗口内完成参数收集并以 JSON 输出。
- **编辑器可插拔**：通过 `EditorExtensionRegistry` 在运行时扫描 `editor/extension/*.meta.json` 加载编辑器扩展 DLL（配置解析器扩展 `IConfigEditorExtension` / 指针解析器扩展 `IPointerEditorExtension`），编辑器能力不编译期耦合。

## 模块边界

**做什么：**

- 主程序 9 页构建向导（仓库来源选择/克隆与缓存同步 → 分支 → 整合包 → 导出类型 → 导出目录 → 额外字段 → 构建清单 → 构建执行 → 完成页），含进度遮罩、toast、淡入切换等交互；Flow 模式驱动。
- 领域编辑器：`ModpackContentIde`（整合包内容 IDE：仓库树/输出树双面板 + 预览 + 文件路由到各规则编辑器 + 批量操作/撤销重做 + 拖入导入）、`BranchEditor`（分支配置）、`RepoEditor`（仓库配置）、`PointerEditorPanel`（指针编辑 P3：resolvers 多解析器 + 普通文件↔指针互转 + 批量转换）、`ServerConfigRulesEditor`（serverconfig 规则 P4）、`ConfigFileEditor`（配置文件同步规则 P2）、`FolderPolicyEditor`（文件夹同步策略 P2）、`SyncPoliciesEditor` / `SyncPoliciesDialog`（同步策略表）、`BatchEditorPanel`（多选批量编辑）、`PointerManager` / `PointerEditor`（旧版指针管理对话框/单指针编辑器，保留兼容）。
- `BuildToolWindow` 构建工具窗口（命令行式交互面板）。
- `EditorExtensionRegistry` 编辑器扩展注册表（扫描/加载/按类型查询）。

**不做什么：**

- **不实现通用 GUI 组件**：树面板、进度条、toast、代码编辑器、Git 面板等一律使用 `HiBerGUI::` 组件；任何"可复用、零领域依赖"的新控件应放入 HiBerGUILibrary。
- **不实现构建/工作区引擎**：构建执行委托 `NeoBuild`（`ModpackExporter`/`IBuildProgress`/`BuildEngine`），工作区/Git 操作委托 `NeoWorkspace`（`GitOperations`/`workspace_manager`），解析/下载/导出插件契约在 `NeoCore`。
- **不负责日志初始化与崩溃处理**：宿主（主程序/编辑器）负责 `CLogger::Init` 与 `HiBerCTM::InstallCrashHandler`；模块内仅消费 `CLogger`。
- **不做 CLI 参数解析**：CLI 子命令在 `NeoCLI`；GUIWorker 仅暴露 Flow 模式接口供 `flow gui` 复用。

## 依赖关系

依赖以 `modules/GUIWorker/CMakeLists.txt` 为准：

| 依赖 | 链接类型 | 说明 |
|------|----------|------|
| HiBerGUILibrary | PUBLIC | 通用组件（`toast_notification.h`/`animated_progress.h` 等被向导头文件引用；IDE 引用树面板/进度卡片/内容编辑器/WorkCard） |
| NeoCore | PUBLIC | 插件契约（`IPluginPointer.h`/`IPointerEditorExtension.h`/`IConfigEditorExtension.h`）、`CancelToken`、`IBuildProgress`、`PluginLoader`；并传递链接 CommonLoggerCPP（`CLogger`） |
| NeoBuild | PUBLIC | `ModpackExporter`（BuildPage 构建）、`BranchLayer`（IDE 预览）、构建引擎 |
| Qt6::Core / Qt6::Widgets | PUBLIC | Qt 基础与控件 |
| PowerHelperBridge | PRIVATE | 帮助文档打开链路（done_page 帮助按钮 / 编辑器帮助菜单） |
| libzippp::libzippp | PRIVATE | 导入/zip 相关实现 |
| tomlplusplus::tomlplusplus | PRIVATE | TOML 文件处理实现 |
| NeoWorkspace | include 路径 PRIVATE | 头文件未引用；src 直接 `#include <git_operations.h>`（wizard_window.cpp）、`#include <workspace_manager.h>`（build_page.cpp）；链路经 NeoBuild PUBLIC 传递获得，CMakeLists 未显式链接。✅ 已确认（2026-08-30）：现 NeoWorkspace 已 PUBLIC 补链 `nlohmann_json::nlohmann_json`；GUIWorker 仍仅 PRIVATE include 不直接链接，符号由最终 EXE（主程序/编辑器均链接 NeoWorkspace）解析 |
| CommonLoggerCPP | 间接 | 经 NeoCore PUBLIC 传递，src 直接调用 `CLogger::Info/Error/Warn` |

**反向依赖（消费者）：**

| 消费者 | 链接方式 | 用途 |
|--------|----------|------|
| 主程序 `NeoServerUpdateModpack`（`src/main.cpp`） | `CMakeLists.txt:166` PRIVATE 链接 | `runGuiMode` 创建 `WizardWindow` 走 9 页向导；`runFlowGuiMode` 构造 `FlowConfig` + `setFlowMode` + 接收 `flowDataReady` JSON（CLI `flow gui`） |
| NeoWorkspaceEditor（`modules/NeoWorkspaceEditor/CMakeLists.txt:38`） | PRIVATE 链接 | `new GUIWorker::ModpackContentIde(tabWidget_)` 嵌入内容 IDE，`setRepository/setBranch`，连接 IDE 各信号（保存/修改/Git 追踪/扩展变更），扩展菜单经 `extensionRegistry()` 展示 |
| NeoCLI | 不链接 | `flow gui` 由主程序（链接 GUIWorker）实现；NeoCLI 的 `flow console` 走独立文本引导，不依赖本模块 |

## 文件组成

`include/` 头文件 25 个全部纳入 CMake 构建；另有 `include/export_page.h` + `src/export_page.cpp` 为导出页**历史遗留文件**、未列入 CMakeLists（属当前实现现状：未纳入构建，保留待归档）。

**向导页（wizard_window + 9 页）：**

| 文件 | 说明 |
|------|------|
| `wizard_window.h` | `WizardWindow`（QMainWindow）：9 页堆栈 + Flow 模式；`FlowConfig` 结构 |
| `repo_page.h` | 仓库来源页：远程/本地/缓存三源 + 最近历史 |
| `branch_page.h` | Git 分支选择页（`GitBranchInfo` 卡片） |
| `modpack_page.h` | 整合包分支选择页（`ModpackBranchInfo` 卡片） |
| `export_type_page.h` | 导出格式选择页（扫描 exporters 插件） |
| `export_dir_page.h` | 导出目录设置页 |
| `extra_info_page.h` | 额外字段页（按 exporter meta `fields` 动态生成） |
| `build_checklist_page.h` | 构建清单页（`CollapsibleSection` 折叠区 + 文件树预览） |
| `build_page.h` | 构建执行页（后台线程 + `IBuildProgress`） |
| `done_page.h` | 完成页（成功/失败 + 打开输出目录 + 帮助） |

**领域编辑器：**

| 文件 | 说明 |
|------|------|
| `modpack_content_ide.h` | 整合包内容 IDE：仓库树/输出树 + 路由到规则编辑器 + 预览/导入/批量/撤销重做 |
| `branch_editor.h` | 分支属性编辑器（含同步策略、继承树） |
| `repo_editor.h` | 仓库配置编辑器（含连接测试、顶层同步策略） |
| `pointer_editor.h` | 旧版单指针编辑器（sha256 + resolver + metadata 表） |
| `pointer_manager.h` | 旧版指针管理对话框（QDialog，扩展加载） |
| `pointer_editor_panel.h` | 指针编辑器 P3（resolvers 多解析器卡片 + 转回原文件/普通文件转指针 + 批量转换 + 撤销/重做） |
| `batch_editor_panel.h` | 批量编辑器（多选统一编辑：批量同步策略/转指针/删除） |
| `batch_convert_card.h` | 批量转换模态卡片（遮罩 + 条形分布图 + 报告） |
| `config_file_editor.h` | 配置文件同步编辑器 P2（full/force/partial/ignore + tracked_keys/lines + merge 预览） |
| `folder_policy_editor.h` | 文件夹同步策略编辑器 P2（四选一 + 跟随默认） |
| `sync_policies_editor.h` | 同步策略表编辑器 + `SyncPoliciesDialog` |
| `sync_policy_display.h` | 下拉显示映射函数（中文显示文本 ↔ 英文存储 ID） |
| `serverconfig_rules_editor.h` | serverconfig 规则编辑器 P4（`.rule/globle.json` + `.rule/list.json`） |

**工具窗口 / 扩展：**

| 文件 | 说明 |
|------|------|
| `build_tool_window.h` | 构建工具窗口（命令输入 + 输出面板 + 历史） |
| `editor_extension_registry.h` | 编辑器扩展注册表：扫描 `editor/extension/*.meta.json` 加载 DLL，按扩展名/resolver 类型查询 |

## 构建集成

- CMake target：`GUIWorker`，类型 **STATIC**（`add_library(GUIWorker STATIC ...)`，25 个 cpp + 25 个 include 头）。
- 根 `CMakeLists.txt:139` 已 `add_subdirectory(modules/GUIWorker)`；模块 `CMakeLists.txt` 位于 `modules/GUIWorker/CMakeLists.txt`。
- 头文件通过 `target_include_directories(GUIWorker PUBLIC include)` 对外暴露；链接方式：

  ```cmake
  target_link_libraries(GUIWorker
      PUBLIC  HiBerGUILibrary NeoCore NeoBuild Qt6::Core Qt6::Widgets
      PRIVATE PowerHelperBridge libzippp::libzippp tomlplusplus::tomlplusplus)
  ```

- 宿主链接（主程序/编辑器均 PRIVATE）：

  ```cmake
  target_link_libraries(MyApp PRIVATE GUIWorker)
  ```

- 依赖 Qt AUTOMOC（`CMAKE_AUTOMOC ON`，各 cpp 尾部 `#include "xxx.moc"`）。
- 部署依赖：插件目录（`exporters/`、`editor/extension/` 等）由部署流程在 exe 目录/工作区构建目录提供；改 GUIWorker 类布局后**必须全量重建引用它的每个 target**（见 usage.md 注意事项「布局失配」）。

## 命名空间与公共符号

所有公共符号位于 `namespace GUIWorker`；对 HiBerGUI 组件采用「头文件顶部 include 真实头 + 命名空间内 `using HiBerGUI::X;`」的引用模式（`using HiBerGUI::ToastNotification;`、`using HiBerGUI::AnimatedProgress;`、`using HiBerGUI::OutputTreePanel;` 等，勿在 GUIWorker 内嵌 `namespace HiBerGUI` 前置声明块）。

| 符号 | 类别 | 一句话概览 |
|------|------|-----------|
| `FlowConfig` | struct | 向导 Flow 模式配置：`startPage`/`endPage`/`collectOnly`/`prefill` |
| `WizardWindow` | QMainWindow | 主程序 9 页构建向导（repo→branch→modpack→export-type→export-dir→extra-info→checklist→build→done），支持 Flow 无界面驱动 |
| `RepoPage` | QWidget | 仓库来源页（`SourceType` 枚举：远程/本地/缓存 + 最近历史） |
| `BranchPage` | QWidget | Git 分支选择（`GitBranchInfo`） |
| `ModpackPage` | QWidget | 整合包分支选择（`ModpackBranchInfo`） |
| `ExportTypePage` | QWidget | 导出格式选择（扫描 exporters 插件） |
| `ExportDirPage` | QWidget | 导出目录设置 |
| `ExtraInfoPage` | QWidget | 额外字段（按 exporter meta `fields` 动态生成 + 必填校验） |
| `BuildChecklistPage` / `CollapsibleSection` | QWidget | 构建清单 + 可折叠区 |
| `BuildPage` | QWidget | 构建执行（后台线程 + `NeoCore::IBuildProgress` 回调 + 取消） |
| `DonePage` | QWidget | 完成页（成功/失败 + 打开输出目录 + 帮助） |
| `ModpackContentIde` | QWidget | 整合包内容 IDE：双树 + 路由各规则编辑器 + 预览/导入/批量/撤销重做（最大最复杂的领域组件） |
| `BranchEditor` / `RepoEditor` | QWidget | 分支/仓库配置编辑（含同步策略） |
| `PointerEditor` / `PointerManager` | QWidget / QDialog | 旧版单指针编辑器 / 指针管理对话框（保留兼容） |
| `PointerEditorPanel` | QWidget | 指针编辑器 P3：resolvers 多解析器 + 文件↔指针互转 + 批量转换 + 撤销/重做 |
| `BatchEditorPanel` | QWidget | 多选批量编辑（同步策略/转指针/删除） |
| `BatchConvertCard` | QWidget | 批量转换模态卡片（遮罩 + 进度条形图 + 报告） |
| `ConfigFileEditor` | QWidget | 配置文件同步编辑器 P2（模式 + tracked_keys/tracked_lines + merge 预览） |
| `FolderPolicyEditor` | QWidget | 文件夹同步策略编辑器 P2 |
| `SyncPoliciesEditor` / `SyncPoliciesDialog` | QWidget / QDialog | 同步策略表编辑 |
| `folderPolicyDisplayItems` 等 3 函数 | 函数 | 下拉显示映射（显示中文 ↔ 存储英文 ID） |
| `ServerConfigRulesEditor` | QWidget | serverconfig 规则编辑器 P4（`.rule/globle.json` + `list.json`） |
| `BuildToolWindow` | QWidget | 构建工具窗口（命令行式交互） |
| `EditorExtensionRegistry` / `EditorExtensionKind` / `EditorExtensionInfo` | class / enum class / struct | 编辑器扩展注册表：扫描 `*.meta.json` 加载 DLL（Parser/Pointer 两类），按扩展名/resolver 查询 |
| `ConvertedItem` / `BatchConvertResult` | struct | 批量转换结果条目（sha/relPath/cacheAbs/pointerJson；name/ok/reason） |

完整签名、信号与用法见 [usage.md](usage.md)。