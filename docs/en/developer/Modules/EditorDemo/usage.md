---
title: EditorDemo usage
description: Run editor_demo to compare the Qt and WebView2 editors, try the merge preview and self-check the WebView2 runtime.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/EditorDemo/usage.md
---

# EditorDemo usage

## Quick start

```powershell
# Build (editor_demo is part of the default build)
cmake --build build --target editor_demo

# Run: three tabs, starting on the Qt version
.\build\...\editor_demo.exe

# Jump straight to the WebView2 tab (diagnoses whether the runtime is available)
.\build\...\editor_demo.exe --web
```

The window title is "Editor dual-version demo (Qt vs WebView2)" and the tabs are `Qt editor`, `WebView2 editor` and `merge preview comparison` in that order. UI strings are localized in the source; the English equivalents are shown here.

## Public API

The demo exports no API; what is reusable are the call sites it demonstrates (all from `HiBerGUILibrary` / `HiBerGUIWebView2`):

| API / call site | Signature | What it demonstrates |
|---|---|---|
| `editor_demo` command line | `editor_demo.exe [--web]` | `--web` = switch to the WebView2 tab on startup |
| `registerCodeEditorFactory` | `void registerCodeEditorFactory(CodeEditorKind kind, CodeEditorFactoryFn fn);` | inside `main`, registers `CodeEditorKind::Web` with `createWebCodeEditor` |
| `createCodeEditor` | `ICodeEditor* createCodeEditor(CodeEditorKind kind, QWidget* parent);` | obtains the `CodeEditorKind::Qt` / `Web` implementation |
| `CodeEditor::setLanguage` | `void setLanguage(const QString& langId) override;` | language combo (`builtinLanguages()`) |
| `CodeEditor::setTabWidth` | `void setTabWidth(int spaces) override;` | Tab width switching (2 / 4) |
| `CodeEditor::setDarkMode` | `void setDarkMode(bool dark) override;` | dark/light switching |
| `CodeEditor::setReadOnly` | `void setReadOnly(bool ro) override;` | read-only/editable switching |
| `CodeEditor::setRegionHighlights` | `void setRegionHighlights(const QVector<RegionHighlight>& regions) override;` | preset `{3, 4, "#2f6b31", "tracked"}` region highlight |
| `CodeEditor::addAction` | `void addAction(const EditorAction& action) override;` | toolbar action `fmt` ("indent check") |
| `MergePreviewDialog::MergePreviewDialog` | `explicit MergePreviewDialog(CodeEditorKind kind, QWidget* parent = nullptr);` | merge preview window for either version |
| `MergePreviewDialog::setContent` | `void setContent(const QString& content, const QString& info, const QString& langId, const QVector<RegionHighlight>& highlights = {});` | loads the merge sample and its highlights |
| `WebCodeEditor::usingWebView` | `bool usingWebView() const;` | whether WebView2 is ready (status-bar hint) |

## Typical usage

### Comparing the two editors

1. On the `Qt editor` tab click "Load JSON" or "Load properties", then switch language/Tab/theme/read-only and observe line numbers, highlighting and region highlights;
2. Switch to the `WebView2 editor` tab and repeat the same operations;
3. When the status bar shows "WebView2 version ready." the runtime is available (the source checks `usingWebView()` 500 ms after startup);
4. If the Web tab shows an error placeholder instead of an editor, the system lacks the WebView2 runtime or `WebView2Loader.dll` was not copied next to the EXE.

### Comparing merge previews

On the `merge preview comparison` tab click "Open Qt merge preview" / "Open Web merge preview"; both are fed the same merge sample:

- Yellow (`#8a6d1a`, light theme `#f7e8a8`) = lines fully overwritten from the source file;
- Green (`#2f6b31` / `#b7e4c7`) = tracked key-value lines (column range 2-33).

### Self-checking the WebView2 runtime

```powershell
# Typical symptom of a missing runtime: the Web tab shows the placeholder and the status bar never says ready
.\editor_demo.exe --web

# Confirm the loader was copied to the output directory by the build
Get-ChildItem <build output directory>\WebView2Loader.dll
```

`WebView2Loader.dll` is copied by the POST_BUILD step in `CMakeLists.txt` from `vcpkg_installed/<triplet>/bin/`; the system additionally needs the Edge WebView2 Runtime.

## Notes

- **Demo only, not a product entry point**: `editor_demo` is not in the `neo_deploy` list and never ships in a release package; use `NeoWorkspaceEditor` or `hci_gui` as the real shell in products.
- **Sample data is hard-coded in `src/main.cpp`**: to add comparison scenarios (TOML/SNBT, very large files for performance) edit `sampleJson()` / `sampleProps()` / `sampleMerge()` directly.
- **The Web version depends on the vcpkg webview2 package**: building requires `unofficial-webview2` to be installed; without it `HiBerGUIWebView2` cannot configure.
- **The Qt version has zero WebView2 dependency**: if you only care about the Qt version you can ignore the Web tab, but linking `HiBerGUIWebView2` still copies `WebView2Loader.dll` in POST_BUILD.
- **Column-range semantics of region highlights**: `RegionHighlight`'s `startColumn`/`endColumn` are 0-based and `endColumn` is exclusive; a negative value means the whole line. The green entry in the demo deliberately uses a column range.
- **Console subsystem**: the EXE is not marked `WIN32`, so starting it from a terminal occupies terminal output for `qDebug()`; the log from the `fmt` action goes to the terminal.

## Related

- [README.md](./README.md)
- [HiBerGUILibrary module](../HiBerGUILibrary/README.md)
- [HiBerGUILibrary usage](../HiBerGUILibrary/usage.md)
