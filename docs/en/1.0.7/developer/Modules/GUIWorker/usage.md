---
title: GUIWorker usage
description: GUIWorker APIs and typical usage: embedding the wizard, driving a build with progress, reusing the flow mode and registering editor extensions.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/Modules/GUIWorker/usage.md
---

# GUIWorker usage

## Quick start

```cpp
#include <wizard_window.h>

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto* wizard = new GUIWorker::WizardWindow();      // parent defaults to nullptr
    wizard->setAttribute(Qt::WA_DeleteOnClose);
    wizard->show();

    return app.exec();
}
```

The wizard is a self-contained `QMainWindow`: it owns its nine pages, its clone/fetch workers and its progress card. A host only needs to show it (or embed it, since it is an ordinary widget) and read the result through signals.

## Public API

| API | Signature | Description |
|---|---|---|
| `WizardWindow::WizardWindow` | `explicit WizardWindow(QWidget* parent = nullptr)` | builds all pages and the progress card |
| `WizardWindow::setFlowMode` | `void setFlowMode(const FlowConfig& cfg)` | enters flow mode: start/end page, collect-only, prefill |
| `WizardWindow::pageNameToIndex` | `static int pageNameToIndex(const QString& name)` | page-name to index; `-1` when unknown |
| `WizardWindow::flowTriggerNext` | `void flowTriggerNext()` | advances to the next page programmatically (automation hook) |
| `WizardWindow::flowDone` | `bool flowDone() const` | true once the flow already ended; the host deletes the window instead of showing it |
| `WizardWindow::flowDataReady` | `void flowDataReady(const QString& json)` (signal) | collected flow data as JSON |
| `BuildPage::BuildPage` | `explicit BuildPage(QWidget* parent = nullptr)` | build page with main/sub progress bars and a log panel |
| `BuildPage::startBuild` | `void startBuild(QString repoDir, QString gitBranch, QString modpackBranch, QString exportFormat, QString outputPath)` | runs the exporter plugin's `build_modpack` on a worker thread |
| `BuildPage::cancelBuild` | `void cancelBuild()` | raises the internal `CancelToken` and asks the worker to stop |
| `BuildPage::outputDir` / `warnings` | `QString outputDir() const`; `const QStringList& warnings() const` | result accessors after `buildFinished` |
| `BuildPage::buildFinished` / `progressUpdated` / `subProgressUpdated` / `logLine` / `subBarUpdate` | signals | UI-bound progress events, already marshalled to the GUI thread |
| `ModpackContentIde::ModpackContentIde` | `explicit ModpackContentIde(QWidget* parent = nullptr)` | content IDE: output/repo trees, undo/redo, import, policies |
| `ModpackContentIde::setRepository` / `setBranch` | `void setRepository(const QString& repoDir)`; `void setBranch(const QString& branch, const QString& branchConfigDir)` | bind the IDE to a repository and branch |
| `ModpackContentIde::refreshPreview` / `importDroppedFiles` | `void refreshPreview()`; `void importDroppedFiles(const QStringList& filePaths, const QString& targetRel)` | rebuild the preview / import files at a drop target |
| `PointerEditorPanel::PointerEditorPanel` | `explicit PointerEditorPanel(QWidget* parent = nullptr)` | pointer editor panel |
| `PointerEditorPanel::setContext` / `setExtensionRegistry` | `void setContext(const QString& repoDir, const QString& branch, const QString& branchConfigDir)`; `void setExtensionRegistry(EditorExtensionRegistry* reg)` | repository context and resolver editor plugins |
| `PointerEditorPanel::loadPointer` / `batchConvertJars` | `void loadPointer(const QString& sha, const NeoCore::PointerFileData& data)`; `void batchConvertJars(const QString& folderPath)` | load one pointer / convert a jar folder |
| `EditorExtensionRegistry::scan` | `void scan(const QString& baseDir)` | scans `*.meta.json` plus DLLs under a directory |
| `EditorExtensionRegistry::configEditorFor` / `pointerEditorFor` | `NeoCore::IConfigEditorExtension* configEditorFor(const QString& fileExt) const`; `NeoCore::IPointerEditorExtension* pointerEditorFor(const QString& resolverType) const` | editor lookup, `nullptr` when unregistered |
| `EditorExtensionRegistry::extensions` / `count` | `const QList<EditorExtensionInfo>& extensions() const`; `int count() const` | scanned extension list |
| `BranchEditor::BranchEditor` | `explicit BranchEditor(QWidget* parent = nullptr)` | branch table editor with type/core selectors |
| `BranchEditor::loadBranches` / `saveBranches` | `void loadBranches(const std::vector<nlohmann::json>& branches, const std::string& defaultBranch = "")`; `std::vector<nlohmann::json> saveBranches() const` | branch model in/out |
| `RepoEditor::loadFromJson` / `saveToJson` | `void loadFromJson(const nlohmann::json& config)`; `nlohmann::json saveToJson() const` | workspace-level settings |
| `availableCores` | `const std::vector<CoreInfo>& availableCores()` | server core catalogue (`id`, `name`, `category`, `supportsPlugins`) |

## Typical usage

### Running a build with progress

`BuildPage` is the supported entry point. Do not try to construct `BuildPage::GuiBuildProgress` - it is a private nested adapter created by the page itself.

```cpp
auto* page = new GUIWorker::BuildPage(parent);

QObject::connect(page, &GUIWorker::BuildPage::buildFinished,
    page, [](bool ok, QString message, QStringList warnings) {
        // ok == false -> message carries the failure, warnings the soft issues
    });
QObject::connect(page, &GUIWorker::BuildPage::progressUpdated,
    page, [](QString stage, int percent, QString message) {
        // drive a status bar
    });
QObject::connect(page, &GUIWorker::BuildPage::logLine,
    page, [](QString line) {
        // append to a log widget
    });

page->startBuild(repoDir, QStringLiteral("main"), QStringLiteral("base"),
                 QStringLiteral("hmcl"), targetDir);
// later: page->cancelBuild();  -> the worker thread sees the token and stops
```

`gitBranch` is the Git branch used for checkout; `modpackBranch` is the workspace branch (`base`, `server-prod`, ...). Swapping the two is the classic integration mistake: checkout then fails because the modpack branch usually exists only as a directory under `branches/`.

### Registering editor extensions

```cpp
#include <editor_extension_registry.h>

GUIWorker::EditorExtensionRegistry registry;
registry.scan(QStringLiteral("editor/extension"));       // *.meta.json + NeoEditorExtension_*.dll

if (auto* jsonEditor = registry.configEditorFor(QStringLiteral("json"))) {
    // hand it to ConfigFileEditor::setEditorExtension
}
for (const auto& info : registry.extensions()) {
    // info.name / info.version / info.fileTypes / info.kind / info.priority
}
```

`ModpackContentIde` does the same scan automatically for the usual locations (`<exe>/editor/extension`, `<exe>/../editor/extension`, `build/deploy/editor/extension`) and de-duplicates them, so a host that only uses the IDE does not need its own registry.

### Reusing flow mode

`flow gui` is implemented entirely in the host executable on top of this module:

```cpp
GUIWorker::FlowConfig cfg;
cfg.startPage  = QStringLiteral("repo");                 // repo|branch|modpack|export-type|export-dir|extra-info|checklist|build|done
cfg.endPage    = QStringLiteral("checklist");
cfg.collectOnly = true;
cfg.prefill.insert(QStringLiteral("repo"), repoUrl);

auto* wizard = new GUIWorker::WizardWindow();
wizard->setAttribute(Qt::WA_DeleteOnClose);
QObject::connect(wizard, &GUIWorker::WizardWindow::flowDataReady,
    [](const QString& json) { /* print the JSON block and exit 0 */ });
wizard->setFlowMode(cfg);                                // call before show()

if (wizard->flowDone()) {
    wizard->deleteLater();                               // already finished, nothing to show
} else {
    wizard->show();
}
```

A flow that is closed before reaching its end page is a cancellation (the host reports exit code `1`), and `NSUM_FLOW_AUTOFINISH=1` makes the wizard advance by itself after 1.5 seconds for automated tests.

### Building the branch core selector

```cpp
for (const auto& core : GUIWorker::availableCores()) {
    // core.id / core.name / core.category / core.supportsPlugins
    // separator per category, label "category / name"
}
```

The branch editor enables this selector only for `type == "server"` and removes `core` from the other branch types when saving.

## Notes

- **Pages must not run Git or network work themselves**: they call `NeoWorkspace` / `NeoBuild` and report through signals. A page that shells out to git breaks the CLI/GUI parity and blocks the UI thread.
- **After changing a page class layout, rebuild every translation unit that instantiates it**: `WizardWindow`, `ModpackContentIde` and `NeoWorkspaceEditor`'s `main.cpp` all `new` these widgets; a stale object file allocates the old `sizeof` and the constructor writes past the end (heap corruption at shutdown). A clean build of the affected targets is the fix, not a code change.
- **Extension editors need a live registry**: `ModpackContentIde` creates its own `EditorExtensionRegistry`, scans the three standard directories in its constructor and injects the registry into `PointerEditorPanel` (`rescanExtensions()` rebuilds it after a plugin is installed). A host that embeds `PointerEditorPanel` on its own must call `setExtensionRegistry` before `loadPointer`, otherwise the resolver-specific editor is unavailable.
- **Log with `CLogger` in English**: repository rule - UI text is Chinese, every log/`qInfo`/`CLogger` message is English and ASCII.
- **Flow navigation needs the guard**: while a flow is active, the load-complete callbacks of the repo/branch pages must not bounce the wizard back to their next page; keep the guard so `--from`/`--to`/`--prefill` navigation is not overwritten.
- **Respect Qt ownership and the build worker's lifetime**: pass a parent where possible and `WizardWindow` deletes itself only with `WA_DeleteOnClose`. `BuildPage::startBuild` joins a previous worker before starting a new one but `BuildPage` declares no destructor, so destroy the page only after `buildFinished` - deleting a page whose `std::thread` is still joinable calls `std::terminate`.

## Related

- [README.md](./README.md)
- [Editor extensions](../../../extensions/editor/index.md)
