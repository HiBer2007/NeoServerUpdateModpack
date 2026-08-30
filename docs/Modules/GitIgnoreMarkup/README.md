# GitIgnoreMarkup 说明文档

## 概述

GitIgnoreMarkup 是项目独立的 `.gitignore` 标记/逆向标记系统 + 编辑 GUI 组件（2026-08-30 新增，独立于 GUI 库）。它定义了一套**标准行格式**，把 `.gitignore` 文件从「纯规则文本」升级为「可逆的结构化标记」，支持活动/临时取消规则、逐行标注、分组标注与普通注释：

| 行格式 | 含义 |
|--------|------|
| `<规则>` | 活动规则（勾选态） |
| `#! <规则>` | 临时取消忽略的规则（未勾选态，可重新勾选恢复） |
| `#> <文本>` | 下一行标注（标注紧跟其下的规则行） |
| `#{ <文本>` + `#}` | 下一组标注 · 首尾标记（成对输出） |
| `# <文本>` | 普通注释（不绑定任何行/组） |
| 空行 | 忽略（解析与序列化均不产生） |

纯逻辑部分（`gitignore_markup.{h,cpp}`）仅依赖 Qt6::Core，位于独立命名空间 `GitIgnoreMarkup`，可在编辑器、CLI 或任何宿主中复用。编辑 GUI 组件（`GitIgnoreDialog`）提供两 tab 界面：**图形化编辑**（列表 + 勾选 + 标注/分组 + 通配符插入 + 常用预设 + 自动总结）与**直接编辑**（HiBerGUILibrary 的 CodeEditor + GitIgnoreHighlighter 语法高亮），磁盘格式保持不变，仅渲染层替换显示。

## 设计目标

- **独立可复用**：`GitIgnoreMarkup` 是纯逻辑模块（非 GUI 组件/引擎），不依赖领域代码，CLI/其他宿主可直接复用以解析、修改、序列化 `.gitignore`，并保证往返一致。
- **磁盘格式稳定**：图形化编辑只是「渲染替换」——序列化仍输出标准 `#{ 标题` / `#}` 首尾标记，GUI 显示用的 `▶ `、`>>> `、组缩进等前缀在序列化前经 `stripDisplayPrefix` 还原，磁盘内容不受 GUI 显示影响。
- **逻辑与 GUI 分层**：`gitignore_dialog.cpp` 的解析/序列化全部走模块 interface，不手写行分类，避免两套解析逻辑漂移。
- **避免循环依赖**：语法高亮器 `GitIgnoreHighlighter`（属于 HiBerGUILibrary）采用**自包含前缀判定**（在自身源码内复制同款前缀常量，不 include `gitignore_markup.h`），保证依赖方向单向：GitIgnoreMarkup → HiBerGUILibrary →（无）。

## 模块边界

**做什么：**

- 行分类解析：`parseLines` 将 `.gitignore` 文本解析为 `Line` 列表（识别规则/临时取消/标注/组/注释，空行丢弃）。
- 序列化：`serialize` 输出标准格式（未勾选规则写 `#! ` 前缀；组首尾成对；孤立组尾丢弃；末尾未闭合组自动补 `#}`）。
- 行判定与便捷构造：`isComment/isLineLabel/isGroupBegin/isGroupEnd/isDisabledRule` 与 `makeRule/makeDisabledRule/makeLineLabel/makeGroupBegin/makeGroupEnd/makeComment`、`prefixFor`。
- 编辑 GUI：`GitIgnoreDialog`（两 tab：图形化编辑 + 直接编辑），保存后 `emit saved(absPath)` 交由宿主处理 git 集成。

**不做什么：**

- 不解释 gitignore **通配匹配语义**（规则是否命中某路径由 git 自身处理，本模块只做标记的结构化解析/序列化）。
- 不提供语法高亮（`GitIgnoreHighlighter` 属于 HiBerGUILibrary，非本模块文件）。
- 不负责 git add / 提交：保存只写文件 + 发信号，宿主监听 `saved` 后自行 `git add` 与刷新 Git 面板。
- 不做仓库内容分析（「自动总结」是 `GitIgnoreDialog` 内部实现，非模块逻辑 API）。

## 依赖关系

| 依赖 | 类型 | 说明 |
|------|------|------|
| Qt6::Core | 第三方库（PUBLIC） | 纯逻辑部分仅需 Qt Core（`QString`/`QVector`） |
| Qt6::Widgets | 第三方库（PRIVATE） | `GitIgnoreDialog`（QDialog 界面） |
| HiBerGUILibrary | 项目模块（PRIVATE） | `GitIgnoreDialog` 使用 CodeEditor 套件（`HiBerGUI::createCodeEditor(CodeEditorKind::Qt, ...)`）与 `HiBerGUI::GitIgnoreHighlighter` |

**反向依赖：**

| 消费者 | 链接方式 | 用途 |
|--------|----------|------|
| NeoWorkspaceEditor | 直接链接 | `EditorWindow::openGitIgnoreDialog` 构造 `GitIgnoreMarkup::GitIgnoreDialog`，监听 `saved` 信号 → `gitAddPaths` + `gitPanel_->refresh()` |

> ⚠️ **循环依赖铁律**：HiBerGUILibrary **不得**链接 GitIgnoreMarkup。`GitIgnoreHighlighter` 自包含前缀判定（其源码内复制同款前缀常量，不 include `gitignore_markup.h`），依赖方向必须保持单向。

## 文件组成

| 文件 | 说明 |
|------|------|
| `include/gitignore_markup.h` | 纯逻辑 API：`LineKind` 枚举、`Line` 结构、`parseLines/serialize`、`isXxx` 判定、`makeXxx` 构造、`prefixFor` |
| `include/gitignore_dialog.h` | `GitIgnoreDialog` 声明（QDialog，`saved` 信号） |
| `src/gitignore_markup.cpp` | 纯逻辑实现（前缀常量、解析、序列化） |
| `src/gitignore_dialog.cpp` | 对话框实现（两 tab、列表 delegate、磁盘读写、自动总结） |
| `CMakeLists.txt` | STATIC target 定义：`PUBLIC Qt6::Core` + `PRIVATE Qt6::Widgets / HiBerGUILibrary` |

## 构建集成

- CMake target：`GitIgnoreMarkup`，类型 **STATIC**（`add_library(GitIgnoreMarkup STATIC ...)`）。
- 根 `CMakeLists.txt:93` 已 `add_subdirectory(modules/GitIgnoreMarkup)`。
- 链接方式：

  ```cmake
  target_link_libraries(MyTarget PRIVATE GitIgnoreMarkup)
  ```

  纯逻辑场景只引入 Qt6::Core；GUI（`GitIgnoreDialog`）随 STATIC 库编译进宿主，依赖的 Qt6::Widgets 与 HiBerGUILibrary 为 PRIVATE，不对外传递（宿主自身仍需满足 HiBerGUILibrary 链接以使用 CodeEditor 头文件）。
- 头文件通过 `target_include_directories(GitIgnoreMarkup PUBLIC include)` 暴露，`#include <gitignore_markup.h>` / `#include <gitignore_dialog.h>`。

## 命名空间与公共符号

模块命名空间：**`GitIgnoreMarkup`**（纯逻辑与 GUI 组件同处该命名空间）。

| 符号 | 类别 | 说明 |
|------|------|------|
| `LineKind` | enum class | `Comment` / `LineLabel` / `GroupBegin` / `GroupEnd` / `DisabledRule` / `Rule` |
| `Line` | struct | `kind`、`prefix`、`text`、`checked` 四字段（见 usage.md） |
| `parseLines / serialize` | 函数 | 解析文本 ↔ 序列化行列表（往返一致） |
| `isComment / isLineLabel / isGroupBegin / isGroupEnd / isDisabledRule` | 函数 | trimmed 行首判定 |
| `makeRule / makeDisabledRule / makeLineLabel / makeGroupBegin / makeGroupEnd / makeComment` | 函数 | 便捷构造 `Line` |
| `prefixFor(LineKind)` | 函数 | 标准标记前缀（`LineKind::Rule` 返回空串） |
| `GitIgnoreDialog` | class (QDialog) | 编辑 GUI：`GitIgnoreDialog(const QString& repoRoot, int initialTab = 0, QWidget* parent = nullptr)` + `saved(const QString& absPath)` 信号 |

外部协作符号（不属于本模块）：`HiBerGUI::ICodeEditor`、`HiBerGUI::createCodeEditor`、`HiBerGUI::GitIgnoreHighlighter`（均在 HiBerGUILibrary）。详细签名与用法见 [usage.md](usage.md)。