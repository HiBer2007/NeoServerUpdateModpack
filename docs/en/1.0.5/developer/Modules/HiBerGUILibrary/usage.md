---
title: HiBerGUILibrary usage
description: API and real call snippets for progress, cards, tree panels, code editors and the Git panel, including switching to the WebView2 editor.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/developer/Modules/HiBerGUILibrary/usage.md
---

# HiBerGUILibrary usage

## Quick start

```cpp
#include "progress_card.h"
#include "toast_notification.h"

auto* card = new HiBerGUI::ProgressCard(this);
card->showCard("Loading repository...", true);
card->setProgress(40, "Reading workspace.json");
connect(card, &HiBerGUI::ProgressCard::cancelRequested, this, &MyWindow::onCancel);
card->complete("Load complete");

auto* toast = new HiBerGUI::ToastNotification(this);
toast->showError("Failed", "workspace.json could not be parsed");
```

## Public API

| API | Signature | Description |
|---|---|---|
| `AnimatedProgress::setValue` | `void setValue(int percent);` | target value, interpolated smoothly inside |
| `AnimatedProgress::setIndeterminate` | `void setIndeterminate(bool on);` | indeterminate (pulsing) state |
| `AnimatedProgress::setText` | `void setText(const QString& text);` | overrides the label text |
| `AnimatedProgress::setCompact` | `void setCompact(bool on);` | hides the text and keeps a thin bar always visible |
| `AnimatedProgress::startAnimation` / `stopAnimation` | `void startAnimation();` / `void stopAnimation();` | start and stop the animation manually |
| `ToastNotification::ToastNotification` | `explicit ToastNotification(QWidget* parent);` | parent is mandatory (used for positioning) |
| `ToastNotification::showError` | `void showError(const QString& title, const QString& detail, int timeoutMs = 3000);` | shows an error toast |
| `ToastNotification::setTopOffset` | `void setTopOffset(int y) { topOffset_ = y; }` | leaves room for the card stack height |
| `ProgressCard::showCard` | `void showCard(const QString& title, bool cancelable);` | shows the large card |
| `ProgressCard::setProgress` | `void setProgress(int percent, const QString& status);` | progress plus status text |
| `ProgressCard::complete` / `fail` / `hideCard` | `void complete(const QString& status);` / `void fail(const QString& status);` / `void hideCard();` | terminal states |
| `WorkCardStack::addCard` | `WorkCard* addCard(const QString& title, bool cancelable);` | adds a compact task card |
| `WorkCardStack::removeCard` | `void removeCard(WorkCard* card);` | removes a task card |
| `WorkCardStack::reposition` | `void reposition();` | re-anchors to the host's top-right corner after a resize |
| `RepoTreePanel::setRootPath` | `void setRootPath(const QString& branchDir);` | sets the branch directory as the tree root |
| `RepoTreePanel::setPointerDir` | `void setPointerDir(const QString& branchConfigDir);` | pointer configuration directory |
| `RepoTreePanel::setInheritedFiles` | `void setInheritedFiles(const QStringList& rels, bool rebuild = true);` | files inherited from the parent chain (shown greyed out) |
| `RepoTreePanel::setBranchManifest` | `void setBranchManifest(const QMap<QString, QString>& markers, bool rebuild = true);` | `delete`/`override` markers |
| `RepoTreePanel::setPointerFiles` | `void setPointerFiles(const QMap<QString, QString>& relToSha, const QMap<QString, QString>& relToResolver, bool rebuild = true);` | shows pointer files at their real location |
| `RepoTreePanel::setExtraConfigFiles` | `void setExtraConfigFiles(const QSet<QString>& rels);` | paths additionally treated as configuration files |
| `RepoTreePanel::setSkipDirs` | `void setSkipDirs(const QStringList& dirs);` | directory names skipped while building the tree (default: `.git` only) |
| `RepoTreePanel::refresh` | `void refresh();` | rebuilds the tree, preserving expansion state |
| `RepoTreePanel::currentSelection` | `RepoObjectInfo currentSelection() const;` | currently selected node |
| `OutputTreePanel::loadEntries` | `void loadEntries(const nlohmann::json& entries, const QSet<QString>& pointerRels = {});` | loads build output entries (`path`/`dir`/`umd`) |
| `OutputTreePanel::setFormat` / `format` | `void setFormat(const QString& format);` / `QString format() const;` | export format combo box |
| `OutputTreePanel::setStatusText` | `void setStatusText(const QString& text);` | bottom status bar text |
| `DeepTreeBehavior::DeepTreeBehavior` | `explicit DeepTreeBehavior(QTreeWidget* tree, QObject* parent = nullptr);` | takes effect as soon as it is attached to the tree |
| `DeepTreeBehavior::toggleItem` | `void toggleItem(QTreeWidgetItem* item);` | expand one level / collapse the whole subtree |
| `ICodeEditor::widget` | `virtual QWidget* widget() = 0;` | the editor as a widget for the host layout |
| `ICodeEditor::setLanguage` | `virtual void setLanguage(const QString& langId) = 0;` | `json/yaml/properties/toml/snbt/txt/plain` |
| `ICodeEditor::language` | `virtual QString language() const = 0;` | current language id |
| `ICodeEditor::setPlainText` / `toPlainText` | `virtual void setPlainText(const QString& text) = 0;` / `virtual QString toPlainText() const = 0;` | text in and out |
| `ICodeEditor::setReadOnly` / `isReadOnly` | `virtual void setReadOnly(bool ro) = 0;` / `virtual bool isReadOnly() const = 0;` | read-only mode |
| `ICodeEditor::setLineNumbers` | `virtual void setLineNumbers(bool on) = 0;` | line-number gutter |
| `ICodeEditor::setFontSize` | `virtual void setFontSize(int pt) = 0;` | font size in points |
| `ICodeEditor::setTabWidth` | `virtual void setTabWidth(int spaces) = 0;` | Tab width in spaces (2/4, default 4) |
| `ICodeEditor::setDarkMode` | `virtual void setDarkMode(bool dark) = 0;` | dark/light theme (VS Code palette logic) |
| `ICodeEditor::setRegionHighlights` | `virtual void setRegionHighlights(const QVector<RegionHighlight>& regions) = 0;` | background marks for line/column ranges (merge tracking) |
| `ICodeEditor::registerHighlighter` | `virtual void registerHighlighter(ICodeHighlighter* h) = 0;` | attaches an external highlighting driver |
| `ICodeEditor::addAction` | `virtual void addAction(const EditorAction& action) = 0;` | adds a toolbar action |
| `ICodeEditor::scrollToTop` | `virtual void scrollToTop() = 0;` | scrolls back to the first line |
| `registerCodeEditorFactory` | `void registerCodeEditorFactory(CodeEditorKind kind, CodeEditorFactoryFn fn);` | registers the `Qt` / `Web` implementation |
| `createCodeEditor` | `ICodeEditor* createCodeEditor(CodeEditorKind kind, QWidget* parent);` | creates by kind and returns a raw pointer |
| `builtinLanguages` | `QStringList builtinLanguages();` | list of built-in languages |
| `editorThemeColors` | `EditorThemeColors editorThemeColors(bool dark);` | dark/light color table |
| `trackedRegionColor` | `QString trackedRegionColor(bool dark);` | default merge-tracking background color |
| `CodeEditor::editor` | `QPlainTextEdit* editor() const { return editor_; }` | internal access (line-number gutter and similar) |
| `FileContentEditor::loadContent` | `void loadContent(const QString& relPath, const QString& absPath, bool inherited, const QString& sourceAbs);` | loads file content into the editor |
| `MergePreviewDialog::setContent` | `void setContent(const QString& content, const QString& info, const QString& langId, const QVector<RegionHighlight>& highlights = {});` | sets the preview content |
| `GitPanel::setRepoPath` | `void setRepoPath(const std::string& path);` | repository directory (`std::string`) |
| `GitPanel::setGitPath` | `void setGitPath(const QString& path) { gitPath_ = path; }` | injects the git executable (default `"git"`) |
| `GitPanel::refresh` | `void refresh();` | refreshes status and history asynchronously |
| `GitPanel::softResetTo` | `void softResetTo(const QString& hash);` | `reset --soft` to the target commit |
| `GitPanel::squashDialog` | `void squashDialog();` | picks a target commit and writes a combined commit message |
| `GitPanel::hasChanges` | `bool hasChanges() const` | whether uncommitted changes exist |
| `WebCodeEditor::usingWebView` | `bool usingWebView() const;` | whether WebView2 is ready (otherwise the placeholder is shown) |

The main signals you will connect to (verbatim from the headers):

| Component | Signals |
|---|---|
| `RepoTreePanel` | `objectActivated(const RepoObjectInfo&)`, `filesDropped(const QStringList&, const QString&)`, `deleteRequested(const RepoObjectInfo&)`, `contentEditRequested(const RepoObjectInfo&)`, `dropTargetChanged(const QString&, bool)` |
| `OutputTreePanel` | `formatChanged(const QString&)`, `refreshRequested()`, `deleteRequested(const RepoObjectInfo&)`, `filesDropped(const QStringList&, const QString&)` |
| `FileContentEditor` | `contentSaveRequested(const QString& relPath, const QString& content, bool inherited)` |
| `GitPanel` | `statusChanged(const QString& branch, int totalChanges)`, `commitFinished(const QString& branch)`, `historyChanged(const QString& branch)`, `gitOutput(const QString& line)`, `gitOperationFinished(bool ok, const QString& errMsg)` |

## Typical usage

### Progress card plus compact work cards

```cpp
auto* stack = new HiBerGUI::WorkCardStack(this);
HiBerGUI::WorkCard* task = stack->addCard("Syncing target directory", true);
task->setProgress(30, "config/*.json");
connect(task, &HiBerGUI::WorkCard::cancelRequested, this, &MyWindow::cancelSync);
task->complete("128 files synced");
stack->reposition();      // re-anchor after the window is resized
```

### Repository tree (inheritance and pointers)

```cpp
repoTree_ = new HiBerGUI::RepoTreePanel(this);
repoTree_->setRootPath(QString::fromStdString(branchDir));          // branch data directory
repoTree_->setPointerDir(QString::fromStdString(branchConfigDir));  // branch_config
repoTree_->setInheritedFiles(inheritedRels, false);
repoTree_->setBranchManifest(markerMap, false);
repoTree_->setPointerFiles(relToSha, relToResolver, false);
repoTree_->setExtraConfigFiles(configRels);
repoTree_->refresh();                    // the calls above pass rebuild=false; rebuild once here
new HiBerGUI::DeepTreeBehavior(repoTree_->tree(), repoTree_->tree());
connect(repoTree_, &HiBerGUI::RepoTreePanel::objectActivated,
        this, &MyWindow::openObject);
```

### Editor factory plus a custom highlighter

```cpp
HiBerGUI::registerCodeEditorFactory(HiBerGUI::CodeEditorKind::Web,
    &HiBerGUI::createWebCodeEditor);      // requires linking HiBerGUIWebView2

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
        // locate the fragments yourself and push them into spans
    }
};
ed->registerHighlighter(new MyHighlighter());   // registered drivers take precedence over built-in rules
```

### Editor toolbar actions and merge markers

```cpp
HiBerGUI::EditorAction act;
act.id = "format";
act.text = "Format";
act.tooltip = "Format the current content with the parser";
act.handler = [ed](QWidget*) { /* handle the action */ };
ed->addAction(act);

HiBerGUI::RegionHighlight region;
region.startLine = 12;
region.endLine = 12;
region.color = HiBerGUI::trackedRegionColor(true);
region.tag = "tracked";
ed->setRegionHighlights({region});
```

### Git panel (host injects the git path)

```cpp
auto* gitPanel = new HiBerGUI::GitPanel(this);
gitPanel->setGitPath(QString::fromStdString(InstallConfig::load().gitPath));
gitPanel->setRepoPath(repoDir);
connect(gitPanel, &HiBerGUI::GitPanel::gitOperationFinished,
        this, [](bool ok, const QString& err) { if (!ok) showError(err); });
gitPanel->refresh();
```

## Notes

- **Rebuild every target that uses these components after changing a class member**: `GUIWorker`, the main application and the editor share these headers; an old `.obj` allocates with the old `sizeof` while the new implementation writes at the new offsets -> `HEAP CORRUPTION` on shutdown. After touching a header run `--clean-first` and verify the `.obj` timestamp of every TU that calls `new Xxx()`.
- **Do not combine `DeepTreeBehavior` with `setExpandsOnDoubleClick(true)`**: double-click and the collapse triangle are both handled by its viewport event filter; leaving the default double-click enabled produces "click to select, click again to expand" and swallows the animation.
- **Do not clamp `AnimatedProgress` with `setMinimumHeight`**: the default height is about 42px (text included), and forcing it to 18px cuts the text in half; use `setCompact(true)` for the compact form.
- **Give `ProgressCard` / `WorkCard` enough height**: a card has a title row, a status row and a progress bar, so a tight host layout squeezes the text.
- **`GitPanel` is fully asynchronous**: commit/push/pull refreshes all run `QProcess` asynchronously and discard stale results through generation counters; the host must not block the same thread with `waitForFinished()` waiting for a panel refresh.
- **`GitPanel` does not offer `--force` push**: the UI only performs safe operations (soft reset, squash); force push and hard reset are left to the CLI.
- **The entry JSON structure of `OutputTreePanel` / `RepoTreePanel` is fixed**: the output tree is `[{"path","dir","umd"}]` with `umd` in `{"", "U", "M", "D"}`; changing it upstream requires changing this side too.
- **`createCodeEditor` returns a raw pointer**: ownership goes to the parent widget (`parent`); do not `delete` it and keep using it afterwards.
- **WebView2 is optional**: using only `HiBerGUILibrary` means zero WebView2 dependency; linking `HiBerGUIWebView2` requires `WebView2Loader.dll` and the system runtime. When the runtime is missing `WebCodeEditor` shows a placeholder instead of crashing, and `usingWebView()` returns false.
- **Theme switching**: components restyle themselves on `QEvent::PaletteChange` / `ThemeChange`, so the host only needs to change `QApplication::palette()` instead of calling per-component style functions.

## Related

- [README.md](./README.md)
- [Architecture overview](../../architecture/overview.md)
- [Editor extensions](../../../extensions/editor/index.md)
