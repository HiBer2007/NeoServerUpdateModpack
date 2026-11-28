---
title: HiBerGUILibrary 使用
description: 进度/卡片/树面板/代码编辑器/Git 面板的 API 与真实调用片段，含 WebView2 版编辑器切换。
lang: zh
status: complete
updated: 2026-11-27
---

# HiBerGUILibrary 使用

## 快速开始

```cpp
#include "progress_card.h"
#include "toast_notification.h"

auto* card = new HiBerGUI::ProgressCard(this);
card->showCard("正在加载仓库...", true);
card->setProgress(40, "读取 workspace.json");
connect(card, &HiBerGUI::ProgressCard::cancelRequested, this, &MyWindow::onCancel);
card->complete("加载完成");

auto* toast = new HiBerGUI::ToastNotification(this);
toast->showError("失败", "workspace.json 解析错误");
```

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `AnimatedProgress::setValue` | `void setValue(int percent);` | 目标值，内部平滑过渡 |
| `AnimatedProgress::setIndeterminate` | `void setIndeterminate(bool on);` | 不确定态（脉冲） |
| `AnimatedProgress::setText` | `void setText(const QString& text);` | 覆盖文字 |
| `AnimatedProgress::setCompact` | `void setCompact(bool on);` | 隐藏文字、细条常驻模式 |
| `AnimatedProgress::startAnimation` / `stopAnimation` | `void startAnimation();` / `void stopAnimation();` | 手动启停动画 |
| `ToastNotification::ToastNotification` | `explicit ToastNotification(QWidget* parent);` | 父窗口必填（用于定位） |
| `ToastNotification::showError` | `void showError(const QString& title, const QString& detail, int timeoutMs = 3000);` | 弹出错误通知 |
| `ToastNotification::setTopOffset` | `void setTopOffset(int y) { topOffset_ = y; }` | 让出卡片堆高度 |
| `ProgressCard::showCard` | `void showCard(const QString& title, bool cancelable);` | 显示大卡片 |
| `ProgressCard::setProgress` | `void setProgress(int percent, const QString& status);` | 进度 + 状态文本 |
| `ProgressCard::complete` / `fail` / `hideCard` | `void complete(const QString& status);` / `void fail(const QString& status);` / `void hideCard();` | 结束态 |
| `WorkCardStack::addCard` | `WorkCard* addCard(const QString& title, bool cancelable);` | 新增紧凑任务卡 |
| `WorkCardStack::reposition` | `void reposition();` | 宿主尺寸变化后重新锚定右上角 |
| `RepoTreePanel::setRootPath` | `void setRootPath(const QString& branchDir);` | 指定分支目录作为树根 |
| `RepoTreePanel::setPointerDir` | `void setPointerDir(const QString& branchConfigDir);` | 指针配置目录 |
| `RepoTreePanel::setInheritedFiles` | `void setInheritedFiles(const QStringList& rels, bool rebuild = true);` | 父链继承文件（灰显） |
| `RepoTreePanel::setBranchManifest` | `void setBranchManifest(const QMap<QString, QString>& markers, bool rebuild = true);` | `delete`/`override` 标记 |
| `RepoTreePanel::setPointerFiles` | `void setPointerFiles(const QMap<QString, QString>& relToSha, const QMap<QString, QString>& relToResolver, bool rebuild = true);` | 指针文件按真实位置显示 |
| `RepoTreePanel::setExtraConfigFiles` | `void setExtraConfigFiles(const QSet<QString>& rels);` | 追加「视为配置文件」的路径 |
| `RepoTreePanel::setSkipDirs` | `void setSkipDirs(const QStringList& dirs);` | 建树时跳过的目录名（默认仅 `.git`） |
| `RepoTreePanel::refresh` | `void refresh();` | 重建树（保留展开状态） |
| `RepoTreePanel::currentSelection` | `RepoObjectInfo currentSelection() const;` | 当前选中节点 |
| `OutputTreePanel::loadEntries` | `void loadEntries(const nlohmann::json& entries, const QSet<QString>& pointerRels = {});` | 载入构建产物条目（`path`/`dir`/`umd`） |
| `OutputTreePanel::setFormat` / `format` | `void setFormat(const QString& format);` / `QString format() const;` | 导出格式下拉 |
| `OutputTreePanel::setStatusText` | `void setStatusText(const QString& text);` | 底部状态栏文本 |
| `DeepTreeBehavior::DeepTreeBehavior` | `explicit DeepTreeBehavior(QTreeWidget* tree, QObject* parent = nullptr);` | 挂到树上即生效 |
| `DeepTreeBehavior::toggleItem` | `void toggleItem(QTreeWidgetItem* item);` | 展开一层 / 深折叠 |
| `ICodeEditor::setLanguage` | `virtual void setLanguage(const QString& langId) = 0;` | `json/yaml/properties/toml/snbt/txt/plain` |
| `ICodeEditor::setPlainText` / `toPlainText` | `virtual void setPlainText(const QString& text) = 0;` / `virtual QString toPlainText() const = 0;` | 文本读写 |
| `ICodeEditor::setRegionHighlights` | `virtual void setRegionHighlights(const QVector<RegionHighlight>& regions) = 0;` | 行/列区间背景标记（merge 追踪） |
| `ICodeEditor::registerHighlighter` | `virtual void registerHighlighter(ICodeHighlighter* h) = 0;` | 挂外部高亮驱动 |
| `ICodeEditor::addAction` | `virtual void addAction(const EditorAction& action) = 0;` | 添加工具条动作 |
| `registerCodeEditorFactory` | `void registerCodeEditorFactory(CodeEditorKind kind, CodeEditorFactoryFn fn);` | 注册 `Qt` / `Web` 实现 |
| `createCodeEditor` | `ICodeEditor* createCodeEditor(CodeEditorKind kind, QWidget* parent);` | 按类型创建，返回裸指针 |
| `builtinLanguages` | `QStringList builtinLanguages();` | 内置语言清单 |
| `editorThemeColors` | `EditorThemeColors editorThemeColors(bool dark);` | 深/浅色配色表 |
| `trackedRegionColor` | `QString trackedRegionColor(bool dark);` | merge 追踪默认背景色 |
| `CodeEditor::editor` | `QPlainTextEdit* editor() const { return editor_; }` | 行号栏等内部用途 |
| `FileContentEditor::loadContent` | `void loadContent(const QString& relPath, const QString& absPath, bool inherited, const QString& sourceAbs);` | 载入文件内容 |
| `MergePreviewDialog::setContent` | `void setContent(const QString& content, const QString& info, const QString& langId, const QVector<RegionHighlight>& highlights = {});` | 设置预览内容 |
| `GitPanel::setRepoPath` | `void setRepoPath(const std::string& path);` | 仓库目录（`std::string`） |
| `GitPanel::setGitPath` | `void setGitPath(const QString& path) { gitPath_ = path; }` | 注入 git 可执行文件（默认 `"git"`） |
| `GitPanel::refresh` | `void refresh();` | 异步刷新状态与历史 |
| `GitPanel::softResetTo` | `void softResetTo(const QString& hash);` | `reset --soft` 到目标提交 |
| `GitPanel::squashDialog` | `void squashDialog();` | 选择目标提交 + 汇总提交消息 |
| `GitPanel::hasChanges` | `bool hasChanges() const` | 是否有未提交变更 |
| `WebCodeEditor::usingWebView` | `bool usingWebView() const;` | WebView2 是否就绪（否则显示占位） |

## 典型用法

### 进度卡片 + 紧凑任务卡组合

```cpp
auto* stack = new HiBerGUI::WorkCardStack(this);
HiBerGUI::WorkCard* task = stack->addCard("同步目标目录", true);
task->setProgress(30, "config/*.json");
connect(task, &HiBerGUI::WorkCard::cancelRequested, this, &MyWindow::cancelSync);
task->complete("已同步 128 个文件");
stack->reposition();      // 窗口 resize 后重新锚定
```

### 仓库树（含继承与指针）

```cpp
repoTree_ = new HiBerGUI::RepoTreePanel(this);
repoTree_->setRootPath(QString::fromStdString(branchDir));          // 分支数据目录
repoTree_->setPointerDir(QString::fromStdString(branchConfigDir));  // branch_config
repoTree_->setInheritedFiles(inheritedRels, false);
repoTree_->setBranchManifest(markerMap, false);
repoTree_->setPointerFiles(relToSha, relToResolver, false);
repoTree_->setExtraConfigFiles(configRels);
repoTree_->refresh();                    // 上面几次传 rebuild=false，最后一次统一重建
new HiBerGUI::DeepTreeBehavior(repoTree_->tree(), repoTree_->tree());
connect(repoTree_, &HiBerGUI::RepoTreePanel::objectActivated,
        this, &MyWindow::openObject);
```

### 编辑器工厂 + 自定义高亮

```cpp
HiBerGUI::registerCodeEditorFactory(HiBerGUI::CodeEditorKind::Web,
    &HiBerGUI::createWebCodeEditor);      // 需要链接 HiBerGUIWebView2

HiBerGUI::ICodeEditor* ed = HiBerGUI::createCodeEditor(
    HiBerGUI::CodeEditorKind::Qt, this);
ed->setLanguage("json");
ed->setDarkMode(true);
ed->setPlainText(QString::fromStdString(content));

class MyHighlighter : public HiBerGUI::ICodeHighlighter {
public:
    void highlight(const QString& langId, const QString& text,
                   QVector<HiBerGUI::HighlightSpan>& spans) override
    {
        if (langId != "json") return;
        // 自行定位关键片段，push 到 spans
    }
};
ed->registerHighlighter(new MyHighlighter());   // 注册后优先于内置规则
```

### 代码编辑器工具条动作与 merge 标记

```cpp
HiBerGUI::EditorAction act;
act.id = "format";
act.text = "格式化";
act.tooltip = "按解析器格式化当前内容";
act.handler = [ed](QWidget*) { /* 处理 */ };
ed->addAction(act);

HiBerGUI::RegionHighlight region;
region.startLine = 12;
region.endLine = 12;
region.color = HiBerGUI::trackedRegionColor(true);
region.tag = "tracked";
ed->setRegionHighlights({region});
```

### Git 面板（宿主注入 git 路径）

```cpp
auto* gitPanel = new HiBerGUI::GitPanel(this);
gitPanel->setGitPath(QString::fromStdString(InstallConfig::load().gitPath));
gitPanel->setRepoPath(repoDir);
connect(gitPanel, &HiBerGUI::GitPanel::gitOperationFinished,
        this, [](bool ok, const QString& err) { if (!ok) showError(err); });
gitPanel->refresh();
```

## 注意事项

- **改组件类成员后必须全量重建使用它的每个 target**：`GUIWorker` / 主程序 / 编辑器共享这些头文件，旧 `.obj` 按旧 `sizeof` 分配、新实现按新偏移写入 → 关闭时 `HEAP CORRUPTION`。改头文件后 `--clean-first`，并核对 `new Xxx()` 所在 TU 的 `.obj` 时间戳。
- **`DeepTreeBehavior` 不要与 `setExpandsOnDoubleClick(true)` 并用**：双击与折叠三角都由它的 viewport 事件过滤器接管；同时开默认双击会「单击选中 → 再展开」，动画被吞掉。
- **`AnimatedProgress` 不要压 `setMinimumHeight`**：默认高度约 42px（含文字），压到 18px 会把文字截半；紧凑形态用 `setCompact(true)`。
- **`ProgressCard` / `WorkCard` 要给足高度**：卡片有标题行 + 状态行 + 进度条，宿主布局里压太小会挤压文字。
- **`GitPanel` 全异步**：提交/推送/拉取刷新都走 `QProcess` 异步与代际号丢弃过期结果，宿主不要在同一线程同步 `waitForFinished()` 等待面板刷新。
- **`GitPanel` 不提供 `--force` 推送**：UI 只做安全操作（软回退、squash），强推/hard reset 这类动作留给 CLI。
- **`OutputTreePanel` / `RepoTreePanel` 的条目 JSON 结构固定**：输出树为 `[{"path","dir","umd"}]`，`umd ∈ {"", "U", "M", "D"}`；上下游改动要同时改这里。
- **`createCodeEditor` 返回裸指针**：所有权交给父 widget（`parent`），不要 delete 后继续使用。
- **WebView2 是可选的**：只用 `HiBerGUILibrary` 时零 WebView2 依赖；链接 `HiBerGUIWebView2` 才需要 WebView2Loader 与系统运行时。缺运行时时 `WebCodeEditor` 显示占位而不崩溃，`usingWebView()` 返回 false。
- **主题切换**：组件在 `QEvent::PaletteChange` / `ThemeChange` 时自刷样式，宿主改 `QApplication::palette()` 即可，不必逐个调样式函数。

## 相关文档

- [README.md](./README.md)
- [总体架构](../../architecture/overview.md)
- [编辑器扩展](../../../extensions/editor/index.md)
