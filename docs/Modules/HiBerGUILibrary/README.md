# HiBerGUILibrary 说明文档

## 概述

HiBerGUILibrary 是 NeoServerUpdateModpack 项目中的**通用 GUI 组件库**（命名空间 `HiBerGUI`），集中提供进度显示（AnimatedProgress / ProgressCard / WorkCard / WorkCardStack）、通知（ToastNotification）、树面板（OutputTreePanel / RepoTreePanel）、代码编辑器套件（ICodeEditor / CodeEditor / GitIgnoreHighlighter）、merge 预览（MergePreviewDialog）以及全异步 Git 提交面板（GitPanel）等可复用 Qt Widgets 组件。其核心定位是**零领域依赖**：仅依赖 Qt6（Core + Widgets）与 nlohmann_json，不引用任何领域模块（NeoCore / NeoWorkspace / NeoBuild 等），因此可**独立发布为源码包**，供本项目任意宿主或外部 Qt 工程直接复用。

## 设计目标

### 为何独立成库

原先一批通用控件分散在 `GUIWorker`（原 NeoGUI）与编辑器（NeoWorkspaceEditor）中重复实现。2026-08-07 将其中与业务无关的通用组件统一迁入 `modules/HiBerGUILibrary`，领域组件保留在 `GUIWorker`。独立成库后：

- **消除重复**：向导页、编辑器、主程序共用同一套进度/通知/树组件；
- **边界清晰**：通用 UI 能力的演进不影响领域模块，反之亦然；
- **可独立复用**：目录自带 `include/` + `src/` + `CMakeLists.txt`，拷贝即得源码包。

### 设计原则

- **零领域依赖（铁律）**：组件不得 include / 链接任何领域模块头文件；git 面板的 Git 操作走 `QProcess` 命令行，不链接 git 库；日志使用 `qInfo` 而非领域 Logger（`CLogger`）。
- **依赖精简**：CMake 仅链 `Qt6::Core`、`Qt6::Widgets`、`nlohmann_json::nlohmann_json`（均为 PUBLIC，见 CMakeLists.txt）。
- **接口抽象**：编辑器套件定义 `ICodeEditor` 接口，宿主一律经工厂 `createCodeEditor(CodeEditorKind)` 创建，隐藏 Qt 版 / Web 版实现差异（Web 工厂由另一库 HiBerGUIWebEditor 注册）。
- **行为组件化**：树的「展开一层 / 折叠深折叠」交互抽象为 `DeepTreeBehavior`（QObject + viewport 事件过滤器），两棵树统一接入，不重复实现。
- **宿主协作**：组件只做展示与交互外壳，落盘 / 构建 / Git 领域逻辑一律经信号交给宿主（如 `contentSaveRequested`、`historyChanged`），保持零领域假设。

### 哪些组件在此（13 组件类 / 14 头文件）

| 组件 | 一句话 |
|------|--------|
| `AnimatedProgress` | 忙碌/进度动画条：确定值平滑动画、不确定脉冲、紧凑细条模式 |
| `ToastNotification` | 悬浮错误通知：倒计时 + 悬停暂停 + 滑入滑出动画 |
| `ProgressCard` | 进度卡片（内嵌 AnimatedProgress，透明度动画），用于仓库加载等大进度场景 |
| `WorkCard` | 紧凑工作卡片（标题 + 状态文字 + 底部贴边进度条） |
| `WorkCardStack` | 层叠工作卡片堆，自锚定宿主右上角，悬浮平移置顶 |
| `ICodeEditor` / `ICodeHighlighter` / 编辑器类型 | 编辑器套件接口、共享类型（HighlightStyle / RegionHighlight 等）与工厂 |
| `CodeEditor` | 套件的 Qt 实现：行号栏 + 语法高亮 + 当前行/区域标记 + 扩展动作工具条，零 WebView |
| `GitIgnoreHighlighter` | .gitignore 语法高亮驱动（实现 `ICodeHighlighter`） |
| `MergePreviewDialog` | merge 结果预览对话框（信息栏 + 可换 Qt/Web 版编辑器 + 关闭按钮） |
| `DeepTreeBehavior` | 树展开/折叠行为：点击三角或双击目录，展开仅一层、折叠深折叠 |
| `OutputTreePanel` | 构建输出文件树面板（格式下拉 + 状态栏 + UMD 标记 + 拖放） |
| `RepoTreePanel` | 仓库文件树面板（指针文件 / 继承文件 / 右键操作 / 拖放导入） |
| `FileContentEditor` | 文件内容编辑器：编辑框复用 CodeEditor，按扩展名选高亮语言 |
| `GitPanel` | 全异步 Git 提交面板（status/history/暂存/提交/推送/拉取/回退/squash） |

## 模块边界

### 做什么

- 通用、与业务无关的 Qt Widgets 组件与交互行为；
- 面向宿主暴露信号化协作点（保存请求、Git 历史变化、对象激活、删除请求等），由宿主接入领域逻辑。

### 不做什么

- **不做领域组件**：配置编辑器（ConfigFileEditor）、指针转换编辑器（PointerEditorPanel）、serverconfig 规则编辑器、BatchEditorPanel、向导页面等均属 `GUIWorker`（`ModpackContentIde` 编排）；
- **不做引擎/业务**：仓库管理、构建、同步、Git 领域封装属 NeoWorkspace / NeoBuild；GitPanel 只做 `QProcess` 命令行外壳；
- **不做文件持久化**：FileContentEditor 的保存仅发 `contentSaveRequested` 信号，落盘由宿主完成；
- **不承担 GUIWorker 集成细节**：GUIWorker 引用 HiBerGUI 组件的约定为「include 真实头 + `using HiBerGUI::X;`」，属宿主侧规范（见 AGENTS.md）。

## 依赖关系

### 依赖（以 `modules/HiBerGUILibrary/CMakeLists.txt` 为准）

| 依赖 | 生效方式 | 用途 |
|------|----------|------|
| `Qt6::Core` | PUBLIC | 基础类型、QProcess（GitPanel 命令行）、QTimer、事件 |
| `Qt6::Widgets` | PUBLIC | 全部组件基类与控件 |
| `nlohmann_json::nlohmann_json` | PUBLIC | OutputTreePanel/RepoTreePanel 的 entries JSON 数据 |

> 注意：**无 `Qt6::Network`**；git 面板的网络/远程交互交给 `git` 可执行文件自身。

### 反向依赖（宿主）

| 宿主 | 链接方式 | 使用的组件 |
|------|----------|-----------|
| `GUIWorker` | LIB（STATIC 链入） | AnimatedProgress、ToastNotification、OutputTreePanel、RepoTreePanel、FileContentEditor 等 |
| `GitIgnoreMarkup` | PRIVATE | GitIgnoreDialog 直接编辑页：CodeEditor + GitIgnoreHighlighter |
| `NeoWorkspaceEditor` | LIB | GitPanel（编辑器主窗口，`setGitPath` 注入领域 git 路径） |
| `EditorDemo` | LIB | 编辑器套件演示程序 |
| `HiBerGUIWebEditor` | LIB/PUBLIC include | 注册 `CodeEditorKind::Web` 工厂（WebCodeEditor） |

> 依赖方向铁律：`GitIgnoreMarkup → HiBerGUILibrary → 无`；HiBerGUILibrary **不得**链接 GitIgnoreMarkup（GitIgnoreHighlighter 自包含前缀判定，不 include `gitignore_markup.h`）。

## 文件组成

头文件位于 `include/`，实现位于 `src/`：

| 文件 | 组件/内容 | 说明 |
|------|-----------|------|
| `include/animated_progress.h` | `AnimatedProgress` | 进度动画条（`Q_PROPERTY animatedValue`） |
| `include/toast_notification.h` | `ToastNotification` | 悬浮错误通知 |
| `include/progress_card.h` | `ProgressCard` | 进度卡片（内嵌 AnimatedProgress + 透明度动画） |
| `include/work_card.h` | `WorkCard` | 紧凑工作卡片 |
| `include/work_card_stack.h` | `WorkCardStack` | 层叠卡片堆（`RevealHeight = 22`） |
| `include/code_editor_interface.h` | 编辑器套件接口层 | HighlightStyle / HighlightSpan / EditorLanguageDef / EditorAction / RegionHighlight / EditorThemeColors / ICodeHighlighter / ICodeEditor / CodeEditorKind / 工厂与语言注册函数 |
| `include/code_editor.h` | `CodeEditor` | 套件 Qt 实现（行号栏独立控件 + 编辑区左右布局） |
| `include/gitignore_highlighter.h` | `GitIgnoreHighlighter` | .gitignore 高亮驱动（非 QObject，无 .moc） |
| `include/merge_preview_dialog.h` | `MergePreviewDialog` | merge 预览对话框（持有 `ICodeEditor*`） |
| `include/deep_tree_behavior.h` | `DeepTreeBehavior` | 树展开/折叠行为 |
| `include/output_tree_panel.h` | `OutputTreePanel` | 输出文件树面板 |
| `include/repo_tree_panel.h` | `RepoTreePanel` + `RepoObjectType`/`RepoObjectInfo` + `createFolderInteractive` | 仓库文件树面板与对象模型 |
| `include/file_content_editor.h` | `FileContentEditor` | 文件内容编辑器（内嵌 CodeEditor + GitIgnoreHighlighter） |
| `include/git_panel.h` | `GitPanel` + `GitFileEntry` | 全异步 Git 提交面板 |

实现文件（13 个 .cpp）：`animated_progress.cpp`、`toast_notification.cpp`、`progress_card.cpp`、`work_card.cpp`、`work_card_stack.cpp`、`code_editor.cpp`、`gitignore_highlighter.cpp`、`merge_preview_dialog.cpp`、`deep_tree_behavior.cpp`、`output_tree_panel.cpp`、`repo_tree_panel.cpp`、`file_content_editor.cpp`、`git_panel.cpp`。

## 构建集成

```cmake
# 根 CMakeLists 已 add_subdirectory(modules/HiBerGUILibrary)
add_library(HiBerGUILibrary STATIC
    src/*.cpp ...
    include/*.h ...)
target_include_directories(HiBerGUILibrary
    PUBLIC  include
    PRIVATE src)
target_link_libraries(HiBerGUILibrary
    PUBLIC Qt6::Core Qt6::Widgets nlohmann_json::nlohmann_json)
```

- **target 类型**：`STATIC`（静态库，宿主链接 `.lib`）；
- **链接方式**：宿主 `target_link_libraries(<app> PRIVATE HiBerGUILibrary)` 即可，Qt6 / nlohmann_json 依赖经 PUBLIC 自动传递，无需宿主重复声明；
- **头文件可见性**：`PUBLIC include`，宿主直接按文件名 include（如 `#include "git_panel.h"`）；
- **Qt MOC**：启用 `CMAKE_AUTOMOC ON`（项目既有约定，`#include "xxx.moc"` 模式）；
- **独立发布 = 源码包**：整个 `modules/HiBerGUILibrary/` 目录（include + src + CMakeLists.txt）拷贝到宿主工程即可直接构建，无外部配置、无生成文件依赖；非 Qt 宿主自行提供 Qt6 与 nlohmann_json 即可。

## 命名空间与公共符号

全部公共符号位于命名空间 `HiBerGUI`。组件概览：

| 符号 | 头文件 | 一句话 |
|------|--------|--------|
| `AnimatedProgress` | animated_progress.h | 忙碌/进度动画条（确定 + 不确定 + 紧凑） |
| `ToastNotification` | toast_notification.h | 错误通知（倒计时 + 悬停暂停） |
| `ProgressCard` | progress_card.h | 进度卡片（`cancelRequested` 信号） |
| `WorkCard` | work_card.h | 紧凑工作卡片（`cancelRequested` 信号） |
| `WorkCardStack` | work_card_stack.h | 层叠卡片堆（`addCard`/`removeCard`） |
| `HighlightStyle` / `HighlightSpan` / `EditorLanguageDef` / `EditorAction` / `RegionHighlight` / `EditorThemeColors` | code_editor_interface.h | 编辑器共享类型 |
| `ICodeHighlighter` | code_editor_interface.h | 外部高亮驱动扩展接口 |
| `ICodeEditor` | code_editor_interface.h | 编辑器抽象接口（Qt/Web 两版一致） |
| `CodeEditorKind` / `CodeEditorFactoryFn` / `registerCodeEditorFactory` / `createCodeEditor` | code_editor_interface.h | 编辑器工厂 |
| `builtinLanguages` / `defaultLanguageDefs` / `registerLanguageDef` / `trackedRegionColor` / `editorThemeColors` | code_editor_interface.h | 语言与主题辅助 |
| `CodeEditor` | code_editor.h | 套件 Qt 实现 |
| `GitIgnoreHighlighter` | gitignore_highlighter.h | .gitignore 高亮驱动 |
| `MergePreviewDialog` | merge_preview_dialog.h | merge 预览对话框 |
| `DeepTreeBehavior` | deep_tree_behavior.h | 树深折叠行为 |
| `OutputTreePanel` | output_tree_panel.h | 输出文件树面板 |
| `RepoTreePanel` / `RepoObjectType` / `RepoObjectInfo` / `createFolderInteractive` | repo_tree_panel.h | 仓库文件树面板与对象模型 |
| `FileContentEditor` | file_content_editor.h | 文件内容编辑器 |
| `GitPanel` / `GitFileEntry` | git_panel.h | 全异步 Git 提交面板 |

详细 API 与方法签名见 [usage.md](usage.md)。