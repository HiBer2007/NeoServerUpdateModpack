---
title: PowerHelper usage
description: Rendering, view and bridge APIs, the render/toc/group CLI, and how a host actually launches the reader.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/developer/Modules/PowerHelper/usage.md
---

# PowerHelper usage

## Quick start

**Host side: opening the documentation (the most common case, only needs `PowerHelperBridgeLib`)**

```cpp
#include <powerhelper_bridge.h>

// Open the default document group (<exe directory>/docs)
PowerHelper::Bridge::launchReader(PowerHelper::Bridge::defaultDocsDir());

// Open a single document and jump to a heading
PowerHelper::Bridge::launchReader("docs/en/user/quick-start.md",
    QStringList{"--anchor", "Quick start"});
```

**Embedding a custom view (links `PowerHelperCore`)**

```cpp
#include <markdown_viewer.h>

auto* viewer = new PowerHelper::MarkdownViewer(this);
viewer->loadFile("/path/to/guide.md");
connect(viewer, &PowerHelper::MarkdownViewer::openFileRequested,
        this, [viewer](const QString& absPath, const QString& anchor) {
            viewer->loadFile(absPath);
            if (!anchor.isEmpty()) viewer->scrollToHeadingText(anchor);
        });
```

## Public API

### Rendering and document groups (`PowerHelperCore`)

| API | Signature | Description |
|---|---|---|
| `renderToHtml` | `QString PH_API renderToHtml(const QString& markdown);` | Markdown -> HTML (CommonMark + GFM: tables/strikethrough/autolink/tasklist/tagfilter) |
| `renderToTerminal` | `QString PH_API renderToTerminal(const QString& markdown);` | Markdown -> ANSI terminal text (color/bold-italic/tabs/table column alignment/CJK full-width) |
| `renderToDocument` | `void PH_API renderToDocument(QTextDocument* doc, const QString& markdown, bool darkMode = false);` | Markdown -> `QTextDocument`; headings carry `ph-n` anchors |
| `extractToc` | `QVector<TocEntry> PH_API extractToc(const QString& markdown);` | extracts the heading outline with `level >= 1` |
| `displayWidth` | `int PH_API displayWidth(const QString& text);` | display width (CJK full-width counts as 2) |
| `expandTabs` | `QString PH_API expandTabs(const QString& text, int tabWidth = 4);` | expands tabs according to display width |
| `anchorName` | `QString PH_API anchorName(int index);` | heading anchor name `ph-<index>` |
| `scanDocGroup` | `QVector<DocFileInfo> PH_API scanDocGroup(const QString& dir);` | recursively scans `.md` files and fills in `title` and `toc` |
| `MarkdownViewer::setMarkdown` | `void setMarkdown(const QString& markdown);` | sets the content (WebView2 or fallback rendering) |
| `MarkdownViewer::loadFile` | `void loadFile(const QString& path);` | reads and renders a file, remembering the base directory for relative links |
| `MarkdownViewer::toc` | `QVector<TocEntry> toc() const { return toc_; }` | outline of the current document |
| `MarkdownViewer::scrollToHeading` | `void scrollToHeading(int index);` | jumps to a heading by index |
| `MarkdownViewer::scrollToHeadingText` | `void scrollToHeadingText(const QString& text);` | jumps to a heading by its text |
| `MarkdownViewer::usingWebView` | `bool usingWebView() const;` | whether WebView2 is really in use (otherwise the Qt fallback is active) |
| `MarkdownViewer::openFileRequested` | `void openFileRequested(const QString& absPath, const QString& anchor);` | a relative link inside the document was clicked; `absPath` is already resolved |

### Bridge (`PowerHelperBridgeLib`, `PowerHelper::Bridge`)

| API | Signature | Description |
|---|---|---|
| `findPowerHelperExe` | `QString findPowerHelperExe();` | `PowerHelper.exe` next to the current process exe |
| `launchReader` | `bool launchReader(const QString& target, const QStringList& extraArgs = QStringList());` | launches the standalone reader (file or directory); `target` goes into argv[1] |
| `defaultDocsDir` | `QString defaultDocsDir();` | default document group directory = `docs` next to the current exe |
| `classifyTarget` | `TargetKind classifyTarget(const QString& path);` | `File` (`.md`) / `Dir` (document group) / `Unknown` |

### Shell CLI

| Command | Usage | Description |
|---|---|---|
| `render` | `PowerHelper render <file.md> [--json]` | terminal rendering (ANSI) |
| `toc` | `PowerHelper toc <file.md> [--json]` | prints the heading outline |
| `group` | `PowerHelper group <dir> [--toc] [--json]` | lists document group files (optionally with TOCs) |
| help/version | `-h`, `--help`, `-help`, `help`, `/h`, `/?`, `-?`; `-v`, `--version`, `/v` | same help/version families as the main application |
| GUI | `PowerHelper <file.md\|dir> [--anchor <heading>]` | opens a document; `--anchor` is accepted anywhere and the target is the first non-`-` argument |

## Typical usage

### Reading document structure from the CLI (for scripts and AI agents)

```powershell
# Render a document in the terminal
PowerHelper.exe render docs\en\user\quick-start.md

# Get the outline (JSON marker block)
PowerHelper.exe toc docs\en\user\quick-start.md --json

# List a whole document group with TOCs
PowerHelper.exe group docs\en --toc --json
```

The `--json` output looks like:

```text
=====JSON-BEGIN=====
{
  "category": "powerhelper",
  "command": "toc",
  "data": [{"level": 1, "text": "Quick start"}]
}
=====JSON-END=====
```

> Note: with `--json` the human-readable rendering goes to **stderr**; stdout carries only the JSON marker block.

### Providing an "open help" button in the editor or main application

```cpp
connect(helpButton, &QPushButton::clicked, this, [] {
    const QString dir = PowerHelper::Bridge::defaultDocsDir();
    if (!PowerHelper::Bridge::launchReader(dir))
        QMessageBox::warning(nullptr, "Help", "PowerHelper.exe not found");
});
```

`defaultDocsDir()` is `<current exe directory>/docs`, so `docs/` in a release package must sit next to the main executable; `launchReader` returns `false` when the reader cannot locate its own executable.

### Custom rendering output (without the view)

```cpp
const QString md = QString::fromUtf8(file.readAll());

QTextDocument doc;
PowerHelper::renderToDocument(&doc, md, /*darkMode=*/true);
textBrowser->setDocument(&doc);

const QString ansi = PowerHelper::renderToTerminal(md);   // print directly in a terminal or log
const auto toc = PowerHelper::extractToc(md);
```

## Notes

- **Hosts should not link `PowerHelperCore` directly**: when all you need is "open a document", link `PowerHelperBridgeLib`; the core and the WebView2 dependency stay inside the shell EXE, which keeps hosts from being tied to the WebView2 runtime.
- **Argument order of `launchReader`**: `target` always goes into argv[1] and `extraArgs` follows it. Historically the target was placed after `--anchor`, which made the shell treat `--anchor` as a file path and exit 2 immediately.
- **Automatic fallback when WebView2 is missing**: if `MarkdownViewer` cannot find the runtime it renders with `QTextBrowser` (`usingWebView() == false`); tables and images degrade but nothing crashes. Check that return value first when debugging.
- **Relative links are handled by the host**: the view only emits `openFileRequested(absPath, anchor)`; loading the next document is the host's responsibility (see the quick-start example).
- **`PH_API` depends on the linkage mode**: by default `PowerHelperCore` is SHARED (`__declspec(dllexport/dllimport)`); with `POWERHELPER_STANDALONE_ONLY=ON` it is compiled STATIC and defines `POWERHELPER_STATIC`, which makes the macro empty. Switching modes requires a reconfigure and a full rebuild, otherwise you get link errors about mismatched export symbols.
- **Carry the DLLs when moving the EXE by hand**: `PowerHelper.exe` needs `PowerHelperCore.dll` and `libcmark-gfm*.dll` in the same directory (copied by the POST_BUILD step); copying the EXE alone makes it fail to start.
- **CLI exit codes**: `0` success, `1` read failure, `2` usage error; an unknown argument that is not a file does not accidentally open the GUI (it exits with `2`).
- **Terminal output always contains ANSI sequences**: `renderToTerminal` inserts `\x1b[...]` color/style codes unconditionally, and they are kept when redirecting to a file; for plain text do not use the `render` subcommand (`data.rendered` in `--json` output is such a string with codes as well).

## Related

- [README.md](./README.md)
- [Architecture overview](../../architecture/overview.md)
- [User documentation: troubleshooting](../../../user/troubleshooting.md)
