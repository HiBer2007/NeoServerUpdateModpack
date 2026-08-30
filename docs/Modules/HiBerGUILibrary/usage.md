# HiBerGUILibrary 使用文档

> 本模块全部公共符号位于命名空间 `HiBerGUI`。所有签名逐字摘自 `modules/HiBerGUILibrary/include/` 头文件；如与代码不一致，以代码为准。

## 快速开始

```cmake
# 根 CMakeLists 已引入该子目录
add_subdirectory(modules/HiBerGUILibrary)

target_link_libraries(my_app PRIVATE HiBerGUILibrary)
# HiBerGUILibrary PUBLIC 传递: Qt6::Core Qt6::Widgets nlohmann_json::nlohmann_json
```

```cpp
#include "animated_progress.h"
#include "code_editor_interface.h"
#include "git_panel.h"
// 头文件经 target_include_directories PUBLIC include 导出, 直接按文件名 include
```

## 公共 API（按组件分组）

### 1. 编辑器套件（code_editor_interface.h / code_editor.h / gitignore_highlighter.h）

#### 1.1 枚举

| 枚举 | 值 |
|------|----|
| `enum HighlightStyle` | `HlNormal = 0`、`HlKeyword = 1`、`HlString = 2`、`HlComment = 3`、`HlNumber = 4`、`HlType = 5`、`HlConstant = 6` |
| `enum class CodeEditorKind` | `Qt`（纯 C++/Qt，无 WebView 依赖）、`Web`（WebView2 网页版，需 WebView2 运行时） |

#### 1.2 结构体

| 结构体 | 字段（默认值） |
|--------|----------------|
| `struct HighlightSpan` | `int start = 0`、`int length = 0`、`int style = HlNormal` |
| `struct EditorLanguageDef` | `QStringList keywords`、`QStringList types`、`QStringList constants`、`QString stringPattern`（空则用内置默认）、`QString commentPattern`、`QString numberPattern` |
| `struct EditorAction` | `QString id`、`QString text`、`QString tooltip`、`std::function<void(QWidget*)> handler`（参数 = 编辑器 widget） |
| `struct RegionHighlight` | `int startLine = 1`（1-based 起始行）、`int endLine = 1`（1-based 结束行，含）、`QString color`（#rrggbb 深色背景）、`QString tag`、`int startColumn = -1`（0-based 起始列，<0 整行）、`int endColumn = -1`（0-based 结束列，不含）、`QString colorLight`（浅色背景，空 = 跟随 color） |
| `struct EditorThemeColors` | `QString background`、`text`、`keyword`、`string`、`comment`、`number`、`type`、`constant`、`lineNumber`、`currentLine` |

> `RegionHighlight` 兼容旧调用：`{startLine, endLine, color, tag}` 四项聚合初始化；`color == "#2f6b31"` 时按 `trackedRegionColor(dark)` 自动适配追踪绿。

#### 1.3 接口 `class ICodeHighlighter`（code_editor_interface.h）

| 方法 | 说明 |
|------|------|
| `virtual ~ICodeHighlighter() = default;` | 虚析构 |
| `virtual void highlight(const QString& langId, const QString& text, QVector<HighlightSpan>& spans) = 0;` | 为 langId 的全文生成高亮片段（Qt 版与 Web 版共用同一扩展接口） |

#### 1.4 接口 `class ICodeEditor`（code_editor_interface.h）

| 方法 | 说明 |
|------|------|
| `virtual QWidget* widget() = 0;` | 取可布局的 widget |
| `virtual void setLanguage(const QString& langId) = 0;` | 设置高亮语言 |
| `virtual QString language() const = 0;` | 当前语言 |
| `virtual void setPlainText(const QString& text) = 0;` | 设置全文（含 scrollToTop 与区域标记刷新） |
| `virtual QString toPlainText() const = 0;` | 取全文 |
| `virtual void setReadOnly(bool ro) = 0;` | 只读开关 |
| `virtual bool isReadOnly() const = 0;` | 是否只读 |
| `virtual void setLineNumbers(bool on) = 0;` | 行号栏开关 |
| `virtual void setFontSize(int pt) = 0;` | 字号（pt） |
| `virtual void setTabWidth(int spaces) = 0;` | Tab 宽度（空格数，2/4，默认 4） |
| `virtual void setDarkMode(bool dark) = 0;` | 深色/浅色主题（VS Code 配色逻辑） |
| `virtual void setRegionHighlights(const QVector<RegionHighlight>& regions) = 0;` | 行区间背景标记（清除传空列表） |
| `virtual void registerHighlighter(ICodeHighlighter* h) = 0;` | 注册外部高亮驱动（优先于内置规则；单驱动） |
| `virtual void addAction(const EditorAction& action) = 0;` | 添加工具条扩展动作 |
| `virtual void scrollToTop() = 0;` | 滚动到顶部 |

#### 1.5 工厂与自由函数（code_editor_interface.h）

| 符号 | 说明 |
|------|------|
| `using CodeEditorFactoryFn = ICodeEditor* (*)(QWidget* parent);` | 工厂函数类型 |
| `void registerCodeEditorFactory(CodeEditorKind kind, CodeEditorFactoryFn fn);` | 注册工厂（HiBerGUI 内置 Qt 实现；Web 实现由 HiBerGUIWebEditor 库注册） |
| `ICodeEditor* createCodeEditor(CodeEditorKind kind, QWidget* parent);` | 创建编辑器；`Qt` 无注册工厂时回退 `new CodeEditor(parent)`；`Web` 未注册时返回 `nullptr` |
| `QStringList builtinLanguages();` | `json`/`yaml`/`properties`/`toml`/`snbt`/`txt`/`plain` |
| `QString trackedRegionColor(bool dark);` | merge 追踪标记默认背景色（深色深绿 `#2f6b31`，浅色浅绿） |
| `const QVector<EditorLanguageDef>& defaultLanguageDefs();` | 内置语言定义（json/properties/txt/yaml/toml/snbt） |
| `void registerLanguageDef(const QString& langId, const EditorLanguageDef& def);` | 注册自定义语言（2026-08-30 已实现：存入注册表，`setLanguage` 优先精确查询，可覆盖内置语言） |
| `EditorThemeColors editorThemeColors(bool dark);` | 深浅色主题配色 |

#### 1.6 `class CodeEditor`（code_editor.h，`QWidget` + `ICodeEditor` 双继承）

`CodeEditor` 完整实现 `ICodeEditor`（1.4 全部方法 + `Q_OBJECT`）。额外公共符号：

| 符号 | 说明 |
|------|------|
| `explicit CodeEditor(QWidget* parent = nullptr);` / `~CodeEditor() override;` | 构造/析构 |
| `QPlainTextEdit* editor() const;` | 底层编辑区（供行号栏绘制使用） |
| `const EditorThemeColors& themeColors() const;` | 当前主题配色 |
| `signals: void languageChanged(const QString& langId);` | 语言变化信号 |

布局：左侧固定宽度行号栏（独立 `LineNumberArea` 控件，随滚动同步）+ 右侧编辑区，组合布局保证行号/文本不错位；顶部可选扩展动作工具条（`addAction` 触发时创建）。高亮采用全文重置 + 逐行正则/外部驱动，`beginEditBlock` + 抑制重绘防布局风暴；当前行与区域标记合并到 `ExtraSelection`（背景 25% 透明，列区间可用）。

#### 1.7 `class GitIgnoreHighlighter`（gitignore_highlighter.h，实现 `ICodeHighlighter`，非 QObject）

`void highlight(const QString& langId, const QString& text, QVector<HighlightSpan>& spans) override;`
- `langId != "gitignore"` 时不输出任何片段（交由内置规则）；
- 词法：`#` 注释 → `HlComment`；`#>` 下一行标注 / `#{` 组开始 / `#}` 组结束 → 前缀 `HlType`；`#!` 临时取消规则 → 前缀 `HlKeyword` + 规则片段 `HlNormal`；`!` 否定 → `HlKeyword`；`* ? [..]` 通配符 → `HlConstant`；尾部 `/` 目录标记 → `HlType`；`\` 转义序列 → `HlNumber`。

### 2. FileContentEditor（file_content_editor.h）

| 符号 | 说明 |
|------|------|
| `explicit FileContentEditor(QWidget* parent = nullptr);` | 构造（内部创建 `CodeEditorKind::Qt` 编辑器，深色跟随宿主调色板） |
| `~FileContentEditor() override;` | 析构（delete 内部 `GitIgnoreHighlighter*`） |
| `void loadContent(const QString& relPath, const QString& absPath, bool inherited, const QString& sourceAbs);` | 加载文件：`relPath` 显示相对路径；`sourceAbs` 非空时从它读取（继承文件场景）；`inherited=true` 显示「继承自父分支…」提示；另注册 Ctrl+S 保存快捷键 |
| `ICodeEditor* editor() const;` | 取内嵌编辑器（可继续配置） |
| `signals: void contentSaveRequested(const QString& relPath, const QString& content, bool inherited);` | 保存按钮 / Ctrl+S 触发，宿主负责落盘 |

**按扩展名/文件名选语言**（`loadContent` 内）：`.gitignore` → `"gitignore"` + `registerHighlighter(gitIgnoreHl_)`；`json` → `"json"`；`yaml`/`yml` → `"yaml"`；`toml` → `"toml"`；`snbt` → `"snbt"`；`properties`/`ini`/`cfg` → `"properties"`；`txt`/`log` → `"txt"`；其它 → 默认 `"txt"`。除 .gitignore 外每个分支都会 `registerHighlighter(nullptr)` 清理外部驱动。

### 3. GitPanel（git_panel.h）

#### 3.1 数据模型

| 符号 | 字段 |
|------|------|
| `struct GitFileEntry` | `QString path`、`char xStatus = ' '`、`char yStatus = ' '`、`bool staged = false` |

#### 3.2 公共方法

| 方法 | 说明 |
|------|------|
| `explicit GitPanel(QWidget* parent = nullptr);` | 构造（内部 5 秒自动刷新定时器） |
| `~GitPanel();` | 析构（停定时器，含堆健康检查日志） |
| `void setRepoPath(const std::string& path);` | 设置仓库目录（**参数为 `std::string`**）并立即 `refresh()` |
| `std::string repoPath() const;` | 当前仓库目录 |
| `void refresh();` | 异步重扫 status + history（见「刷新语义」） |
| `void setGitPath(const QString& path);` | 设置 git 可执行文件路径（**默认 `"git"` 走 PATH**，宿主应注入内置/自定 git） |
| `QString gitPath() const;` | 当前 git 路径 |
| `void softResetTo(const QString& hash);` | 软回退 `reset --soft`（工作区与暂存区不变，合并提交用） |
| `void squashDialog();` | 合并提交对话框：选择目标提交（闭区间）+ 输入最终提交消息 |
| `bool hasChanges() const;` | `repoPath_` 非空且 staged/unstaged 非空 |

#### 3.3 信号

| 信号 | 触发时机 |
|------|----------|
| `void statusChanged(const QString& branch, int totalChanges);` | 每次 status 扫描完成（含 updateCounts 末尾） |
| `void commitFinished(const QString& branch);` | 提交成功 |
| `void historyChanged(const QString& branch);` | commit/revert/reset/squash 等历史变更后（宿主用于刷新工作区） |
| `void gitOutput(const QString& line);` | git 进程输出实时转发（宿主可挂终端/日志） |
| `void gitOperationFinished(bool ok, const QString& errMsg);` | 异步 git 操作完成：`ok=false` 时 `errMsg` 含错误详情 |

#### 3.4 刷新语义与行为约定

- **全异步**：commit/push/pull/fetch/stage/unstage/双击暂存切换/revert/reset/squash 全部经 `runGitAsync`（QProcess 异步，不阻塞 GUI），输出实时 emit `gitOutput`；`gitBusy_` 防重入（忙时禁用操作按钮）；
- **忙碌条**：耗时操作 `beginBusy("...")` / `endBusy()`（`endBusy` 内含 `refresh()`）；顶部常驻 `AnimatedProgress::setCompact(true)` 细条；
- **异步刷新**：`refresh()` 并行跑 `git status --porcelain -b -z` 与 `git log --oneline --graph --all -30 --decorate`，各自带 `statusGen_`/`historyGen_` 代际号，完成时代际不符即丢弃过期结果；刷新期间**保留旧列表不清空**（防闪烁），clear+填充包 `setUpdatesEnabled(false/true)`；
- **onCommit 自动暂存**：存在未暂存更改时先 `git add` 再提交；提交消息留空则按文件列表自动生成；
- **无 HEAD 初始仓库**：unstage 用 `git rm --cached`（`restore --staged` 会 exit 128）；
- **历史右键菜单**：撤回此提交（revert --no-edit）/ 回退到此提交（reset --hard）/ 软回退到此提交（reset --soft）。

### 4. 树面板（output_tree_panel.h / repo_tree_panel.h / deep_tree_behavior.h）

#### 4.1 共享对象模型与行为

| 符号 | 说明 |
|------|------|
| `enum class RepoObjectType` | `Root`、`Folder`、`ConfigFile`、`PlainFile`、`Pointer` |
| `struct RepoObjectInfo` | `RepoObjectType type = RepoObjectType::Root`、`QString path`、`QString displayName`、`QString pointerSha`、`bool isInherited = false`、`QString marker`（`"delete"`/`"override"`/空） |
| `bool createFolderInteractive(QWidget* parent, const QString& absParent);` | 弹窗输入文件夹名并在 absParent 下创建，成功返回 true |

两棵树构造时统一接入 `new DeepTreeBehavior(tree_, tree_)`：折叠三角点击与双击目录走同一 viewport 事件过滤器——**展开仅一层**（`item->setExpanded(true)`），**折叠深折叠**（`collapseDeep`：先收起整棵子树，再清空全部后代展开标记，使再次展开仅显示一层）；文件（无子节点）无操作；动画由构造内 `setAnimated(true)` 提供，行为不改动该设置（构造内 `setExpandsOnDoubleClick(false)`）。

#### 4.2 OutputTreePanel（output_tree_panel.h）

| 方法 | 说明 |
|------|------|
| `explicit OutputTreePanel(QWidget* parent = nullptr);` | 构造（顶部格式下拉：`hmcl`/`mcbbs`/`modrinth` + 刷新按钮） |
| `void loadEntries(const nlohmann::json& entries, const QSet<QString>& pointerRels = {});` | 重建树并恢复展开路径；`entries` 为对象数组，每项字段 `path`（相对路径，`/` 分隔）、`dir`（bool）、`umd`（`"U"`/`"M"`/`"D"`/空）；`pointerRels` 中的相对路径标为指针文件（青色） |
| `void setExtraConfigFiles(const QSet<QString>& rels);` | 额外视为配置文件的相对路径集合（参与 ConfigFile 类型判定） |
| `void setStatusText(const QString& text);` | 底部状态文字 |
| `void setFormat(const QString& format);` / `QString format() const;` | 格式下拉联动 |
| `QTreeWidget* tree() const;` | 取内部树 |
| `RepoObjectInfo currentSelection() const;` / `QList<RepoObjectInfo> selectedObjects() const;` | 当前选中对象 |
| `bool eventFilter(QObject* obj, QEvent* event) override;` | 内部拖放悬停处理 |

信号：`formatChanged(const QString&)`、`refreshRequested()`、`filesDropped(const QStringList&, const QString& targetRel)`、`deleteRequested(const RepoObjectInfo&)`、`batchDeleteRequested(const QList<RepoObjectInfo>&)`、`pasteRequested(const QStringList&, const QString&, bool isCut)`、`newFolderRequested(const QString& parentRel)`、`objectActivated(const RepoObjectInfo&)`、`createServerConfigRequested()`、`markAsConfigFileRequested(const RepoObjectInfo&)`、`unmarkConfigFileRequested(const RepoObjectInfo&)`、`dropTargetChanged(const QString& targetRel, bool hovering)`。

#### 4.3 RepoTreePanel（repo_tree_panel.h）

| 方法 | 说明 |
|------|------|
| `explicit RepoTreePanel(QWidget* parent = nullptr);` | 构造 |
| `void setRootPath(const QString& branchDir);` | 仓库分支目录根 |
| `void setPointerDir(const QString& branchConfigDir);` | 指针文件实际存储目录 |
| `void setInheritedFiles(const QStringList& rels, bool rebuild = true);` | 继承文件列表（`rebuild=false` 仅更新数据，宿主统一重建避免多次全量刷新） |
| `void setBranchManifest(const QMap<QString, QString>& markers, bool rebuild = true);` | 分支清单标记（delete/override） |
| `void setPointerFiles(const QMap<QString, QString>& relToSha, const QMap<QString, QString>& relToResolver, bool rebuild = true);` | 指针文件按真实位置显示（rel → sha256），resolver 用于提示 |
| `void setExtraConfigFiles(const QSet<QString>& rels);` | 额外配置文件集合 |
| `void refresh();` | 重建树（保留展开路径） |
| `QString rootPath() const;` / `QString pointerDir() const;` | 当前根/指针目录 |
| `QTreeWidget* tree() const;` | 取内部树 |
| `RepoObjectInfo currentSelection() const;` / `QList<RepoObjectInfo> selectedObjects() const;` | 当前选中对象 |

信号（全部为请求型，领域操作由宿主承接）：`objectActivated`、`batchConvertJarsRequested(const QString& folderPath)`、`convertToPointerRequested(const RepoObjectInfo&)`、`restorePointerRequested(const QString& sha)`、`filesDropped(const QStringList&, const QString& targetFolderRel)`、`deleteRequested`、`batchDeleteRequested`、`restoreInheritedRequested`、`contentEditRequested`、`folderPolicyEditRequested`、`filePolicyEditRequested`、`importOverwriteRequested`、`copyRequested`、`cutRequested`、`pasteRequested`、`batchRestorePointersRequested`、`moveItemsRequested`、`createServerConfigRequested`、`markAsConfigFileRequested`、`unmarkConfigFileRequested`、`dropTargetChanged`。

#### 4.4 DeepTreeBehavior（deep_tree_behavior.h，独立用法）

```cpp
explicit DeepTreeBehavior(QTreeWidget* tree, QObject* parent = nullptr);
void toggleItem(QTreeWidgetItem* item);   // 手动切换: 展开一层 / 折叠深折叠
```

构造即接管：`setExpandsOnDoubleClick(false)` + viewport 事件过滤器（左键三角区 `MouseButtonPress`、目录项 `MouseButtonDblClick`，均 consume 防止二次路由）。

### 5. 进度与通知

#### 5.1 AnimatedProgress（animated_progress.h）

| 符号 | 说明 |
|------|------|
| `explicit AnimatedProgress(QWidget* parent = nullptr);` | 构造 |
| `void setValue(int percent);` | 设置确定值：100 直接置满；否则 400ms InOutCubic 平滑动画；自动退出不确定模式 |
| `void setIndeterminate(bool on);` | 不确定忙碌模式（循环脉冲；关闭后 range 归 0-100、值归 0） |
| `void setText(const QString& text);` | 状态文字 |
| `void setCompact(bool on);` | 紧凑模式：隐藏文字、进度条 22px → 4px（常驻细条显示用） |
| `int value() const;` / `QString text() const;` | 取值 |
| `void startAnimation();` / `void stopAnimation();` | 手动启停确定值动画 |
| `int animatedValue() const;` / `void setAnimatedValue(int val);` | Q_PROPERTY `animatedValue`（供 QPropertyAnimation 绑定） |
| `signals:（无）` | — |
| `protected: bool event(QEvent*) override;` | PaletteChange / ThemeChange 时按调色板重刷样式（暗色/浅色自适应） |

#### 5.2 ProgressCard（progress_card.h）

| 符号 | 说明 |
|------|------|
| `explicit ProgressCard(QWidget* parent = nullptr);` | 构造（内嵌 AnimatedProgress + 透明度动画） |
| `void showCard(const QString& title, bool cancelable);` | 显示卡片 |
| `void setProgress(int percent, const QString& status);` | 更新进度与状态 |
| `void complete(const QString& status);` / `void fail(const QString& status);` | 完成/失败 |
| `void hideCard();` | 隐藏 |
| `bool isActive() const;` | 是否活动 |
| `signals: void cancelRequested();` | 取消按钮 |

尺寸建议（编辑器加载卡片/内容导入卡片共用）：`setMinimumHeight(128)` / `setMaximumHeight(190)`。

#### 5.3 WorkCard（work_card.h）

紧凑工作卡片（右上角任务进度）：尖角、标题行 + 状态文字 + 底部贴边进度条；与 ProgressCard（仓库加载大卡）分离；**无论是否可视都持续刷新**。

| 符号 | 说明 |
|------|------|
| `explicit WorkCard(QWidget* parent = nullptr);` | 构造 |
| `void showCard(const QString& title, bool cancelable);` | 显示 |
| `void setProgress(int percent, const QString& status);` | 进度与状态 |
| `void complete(const QString& status);` / `void fail(const QString& status);` | 完成/失败 |
| `bool isActive() const;` | 是否活动 |
| `signals: void cancelRequested();` | 取消 |

#### 5.4 WorkCardStack（work_card_stack.h）

层叠工作卡片堆：卡片保持完整展开形态，最顶层完整显示，其余被压在下层露出下半部分（进度条）；悬浮某卡 = 平移到顶层（其余连带平移，平滑动画）；组件自锚定宿主右上角（尺寸/宿主变化自动重新定位）。

| 符号 | 说明 |
|------|------|
| `explicit WorkCardStack(QWidget* parent = nullptr);` | 构造 |
| `WorkCard* addCard(const QString& title, bool cancelable);` | 添加卡片并返回 |
| `void removeCard(WorkCard* card);` | 移除卡片 |
| `void updateLayout();` | 重排 |
| `void reposition();` | 重新锚定宿主右上角（尺寸或宿主变化后调用） |
| `bool isEmpty() const;` / `int count() const;` | 状态查询 |
| `static const int RevealHeight = 22;` | 被盖住的卡露出高度 |

#### 5.5 ToastNotification（toast_notification.h）

| 符号 | 说明 |
|------|------|
| `explicit ToastNotification(QWidget* parent);` | 构造（**parent 必填**，作为浮动通知的宿主） |
| `void showError(const QString& title, const QString& detail, int timeoutMs = 3000);` | 显示错误通知（目前仅错误场景公开） |
| `void dismiss();` | 立即关闭 |
| `void setTopOffset(int y);` | 顶部偏移：使 toast 显示在卡片堆下方/上方（宿主按卡片堆高度设置） |

悬停（enterEvent）暂停倒计时，离开（leaveEvent）恢复；滑入/滑出动画 + 底部倒计时条。

### 6. MergePreviewDialog（merge_preview_dialog.h）

| 符号 | 说明 |
|------|------|
| `explicit MergePreviewDialog(CodeEditorKind kind, QWidget* parent = nullptr);` | 构造（选择编辑器实现：Qt/Web） |
| `~MergePreviewDialog() override;` | 析构 |
| `void setContent(const QString& content, const QString& info, const QString& langId, const QVector<RegionHighlight>& highlights = {});` | 设置 merge 结果文本、顶部说明、高亮语言与区域背景标记 |
| `ICodeEditor* editor() const;` | 取内嵌编辑器（可继续配置） |

## 典型用法

### 1. 构建 CodeEditor + 高亮器（GitIgnoreHighlighter）

```cpp
#include "code_editor_interface.h"
#include "gitignore_highlighter.h"

// 宿主持有高亮器生命周期（ICodeEditor 不删除注册的高亮器）
auto* hl = new HiBerGUI::GitIgnoreHighlighter(this);   // 非 QObject, 普通成员/QObject 父均可
HiBerGUI::ICodeEditor* editor =
    HiBerGUI::createCodeEditor(HiBerGUI::CodeEditorKind::Qt, this);
if (editor) {
    editor->setLanguage(QStringLiteral("gitignore"));
    editor->registerHighlighter(hl);
    editor->setDarkMode(palette().color(QPalette::Window).lightness() < 128);
    editor->setPlainText(content);
    layout->addWidget(editor->widget(), 1);
}
// 换语言/换文件时必须清理外部驱动, 否则旧高亮器继续被调用:
editor->registerHighlighter(nullptr);
```

### 2. 创建 GitPanel 并注入 git 路径

```cpp
#include "git_panel.h"

auto* panel = new HiBerGUI::GitPanel(mainSplitter);
// 环境可能无 PATH git, 必须注入与领域层一致的 git 路径
panel->setGitPath(QString::fromStdString(GetDefaultGitPath()));   // 由 InstallConfig 解析
panel->setRepoPath(dirPath.toStdString());                        // 注意参数是 std::string

connect(panel, &HiBerGUI::GitPanel::historyChanged, this,
    [this](const QString&) { refreshWorkspace(); });
connect(panel, &HiBerGUI::GitPanel::gitOutput, this,
    [this](const QString& line) { appendLog(line); });
connect(panel, &HiBerGUI::GitPanel::gitOperationFinished, this,
    [](bool ok, const QString& err) {
        if (!ok) CLogger::Warn("Git op failed: {}", err.toStdString());
    });
```

### 3. 把 FileContentEditor 放进对话框/编辑器栈

```cpp
#include "file_content_editor.h"

auto* editor = new HiBerGUI::FileContentEditor(editorStack);
editor->setObjectName(QStringLiteral("fileContentEditor"));   // 宿主可用 findChild 定位
connect(editor, &HiBerGUI::FileContentEditor::contentSaveRequested,
    this, [this](const QString& rel, const QString& content, bool inherited) {
        // 宿主落盘; inherited=true 时需在本分支创建 override 实体
    });
// 加载文件: relPath 用于显示; sourceAbs 非空时从它读取(继承文件); inherited 控制提示文案
editor->loadContent(relPath, absPath, inherited, sourceAbs);
```

### 4. OutputTreePanel 填充数据

```cpp
#include "output_tree_panel.h"

auto* out = new HiBerGUI::OutputTreePanel(tabs);
nlohmann::json entries = nlohmann::json::array();
entries.push_back({ {"path", "config/server.properties"}, {"dir", false}, {"umd", "M"} });
entries.push_back({ {"path", "mods"}, {"dir", true}, {"umd", ""} });
out->loadEntries(entries);
out->setFormat(QStringLiteral("modrinth"));
connect(out, &HiBerGUI::OutputTreePanel::objectActivated, this,
    [this](const HiBerGUI::RepoObjectInfo& info) { onTreeObjectActivated(info); });
connect(out, &HiBerGUI::OutputTreePanel::refreshRequested, this,
    [this]() { rePreviewStructure(); });
```

### 5. MergePreviewDialog 预览 merge 结果

```cpp
#include "merge_preview_dialog.h"

auto* dlg = new HiBerGUI::MergePreviewDialog(HiBerGUI::CodeEditorKind::Qt, this);
dlg->setContent(mergedText,
    QStringLiteral("合并结果预览 (追踪值传播行以绿色标记)"),
    QStringLiteral("json"),
    { {1, 3, QStringLiteral("#2f6b31"), QStringLiteral("tracked")} });  // 旧四项兼容写法
dlg->resize(800, 600);
dlg->exec();
```

### 6. 给自己的 QTreeWidget 挂 DeepTreeBehavior

```cpp
#include "deep_tree_behavior.h"

auto* tree = new QTreeWidget(this);
tree->setAnimated(true);                        // 动画由自身提供, 行为不改动
new HiBerGUI::DeepTreeBehavior(tree, tree);     // 父 = 树, 无需新增成员
// 注意: 接入后勿再连 itemDoubleClicked / 勿手工实现折叠三角逻辑
```

## 注意事项

- **CodeEditor 必须经工厂创建**：宿主编排一律 `createCodeEditor(CodeEditorKind::Qt, parent)` 取 `ICodeEditor*`，勿直接 `new CodeEditor` 裸用；`CodeEditorKind::Web` 在本库内默认返回 `nullptr`（Web 工厂由 HiBerGUIWebEditor 注册后才可用）。
- **外部高亮器是单驱动且生命周期归宿主**：`registerHighlighter` 只保留一个（内部单指针覆盖）；换语言/换文件必须先 `registerHighlighter(nullptr)` 清理（FileContentEditor 每个分支都做）；`ICodeEditor` 不会 delete 注册的高亮器，GitIgnoreHighlighter 非 QObject 需宿主手动释放（FileContentEditor 析构 `delete gitIgnoreHl_` 即此模式）。
- **`registerLanguageDef` 为注册表语义（2026-08-30 实现）**：自定义语言存入静态 `customLanguageDefs()`，`CodeEditor::setLanguage` 优先精确查注册表（可覆盖内置语言），未命中再走内置 `contains` 映射；内置语言定义仍在 `defaultLanguageDefs()` 硬编码。
- **DeepTreeBehavior 接管双击/三角后**：面板/宿主勿再连 `itemDoubleClicked`（会与行为冲突，双击被拆成「选中→再次展开」且动画被吞）、勿再手工写 collapseDeep 递归或依赖默认三角行为；行为不改动 `setAnimated`，动画由树的 `setAnimated(true)` 提供。
- **GitPanel 必须注入 git 路径**：默认 `"git"` 走 PATH；宿主环境可能无 PATH git（如编辑器内置 MinGit），须 `setGitPath(GetDefaultGitPath())` 显式注入；`setRepoPath` 参数是 `std::string`（非 QString），传 `QString::toStdString()`。
- **GitPanel 全异步约定**：操作忙时按钮禁用（`gitBusy_`），`endBusy()` 内含 `refresh()`；刷新有代际号防过期覆盖、刷新期间保留旧列表防闪烁，宿主无需自行加定时器；提交自动暂存未暂存更改。
- **AnimatedProgress 勿压 `setMinimumHeight`**：默认 ~42px（含文字标签），压到 18px 会截半文字；需要细条用 `setCompact(true)`（隐藏文字、22px→4px）；ProgressCard 建议 `setMinimumHeight(128)` / `setMaximumHeight(190)`。
- **布局失配 0xC0000374（冒烟必做）**：改动 HiBerGUILibrary 共享头文件的类成员后，**所有 `new 组件` 的 TU 必须重编**（不信 ninja "no work to do"，核对 .obj 时间戳；`--clean-first` 最稳），否则旧 obj 按旧 `sizeof` 分配、新代码写成员越界 → 关闭时 HEAP CORRUPTION。冒烟 = 启动 → 等 3-4s 确认存活 → 关闭 → 期待 exit 0 且 crash-report 无新增。
- **Qt 子对象 delete 陷阱**：`new QObject(this)` 已注册为父 children，析构中勿再手动 delete 成员（GitPanel timer 等历史 bug 已按「stop/close + 置空」修复）；GitIgnoreHighlighter 非 QObject 是例外，可安全手动 delete。
- **配置文件识别清单需同步维护**：`isConfigPath`（output_tree_panel.cpp）与 `isConfigFile`（repo_tree_panel.cpp）共用 `.json/.yaml/.yml/.toml/.snbt/.txt/.properties/.ini` 清单，新增扩展名（如 `.cfg/.conf`）须同时改两处 + 配置解析器插件支持。
- **依赖方向铁律**：HiBerGUILibrary **不得**链接 GitIgnoreMarkup（GitIgnoreHighlighter 自包含前缀判定，不 include `gitignore_markup.h`）；依赖方向单向 `GitIgnoreMarkup → HiBerGUILibrary → 无`。
- **GUIWorker 引用约定**：include 真实头 + `namespace GUIWorker {` 内 `using HiBerGUI::X;`，勿在 GUIWorker 内嵌 `namespace HiBerGUI` 前置声明块（会造出 `GUIWorker::HiBerGUI::X` 编译错误；前置声明须放全局作用域）。
- **OutputTreePanel 数据格式**：`loadEntries` 的 `entries` 为对象数组，字段 `path`（相对路径）、`dir`（bool）、`umd`（`"U"`/`"M"`/`"D"`/空）；`pointerRels` 中的路径显示为指针文件；`[save]` 目录为通配目录（特殊 tooltip + 高亮，UserRole+3 标记位，勿占用 UserRole+1=umd 位）。

## 相关文档

- `docs/Modules/README.md` — 模块总索引（按核心库/GUI 库/可执行/插件分组登记全部模块）
- `docs/Modules/GitIgnoreMarkup/README.md` + `usage.md` — GitIgnoreMarkup 模块；其 GitIgnoreDialog 直接编辑页复用本库 CodeEditor + GitIgnoreHighlighter（依赖方向 GitIgnoreMarkup → HiBerGUILibrary）
- `docs/Modules/GUIWorker/README.md` + `usage.md` — GUIWorker 领域编辑器与向导，引用本库组件（OutputTreePanel / RepoTreePanel / FileContentEditor / AnimatedProgress / ToastNotification）
- `docs/Modules/NeoWorkspaceEditor/README.md` + `usage.md` — NeoWorkspaceEditor 编辑器宿主（GitPanel 集成：setGitPath 注入领域 git 路径）
- `AGENTS.md` 关键陷阱章节 — HiBerGUILibrary 组件归属、GitPanel 全异步、DeepTreeBehavior、布局失配 0xC0000374 守则、配置文件识别清单