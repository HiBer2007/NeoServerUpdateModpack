---
title: Libraries
description: Shared libraries used by NSUM: CrashTracker, PowerHelper, HiBerGUILibrary and GitIgnoreMarkup.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/extensions/libraries/index.md
---

# Libraries

These libraries are not NSUM business logic but are reused by the main program, editor and installer; each lives in its own repository.

| Library | Purpose | Form |
|---|---|---|
| CrashTracker | Crash capture and offline analysis | EXE + static lib |
| PowerHelper | Markdown reader (WebView2) | DLL + EXE |
| HiBerGUILibrary | Generic Qt widgets (trees, progress, Git panel) | STATIC |
| GitIgnoreMarkup | `.gitignore` marked blocks + editor GUI | STATIC + GUI |

## Dependency direction

```mermaid
flowchart LR
    Core[NeoCore] --> Logger[CommonLoggerCPP]
    GUI[GUIWorker] --> HiBerGUI[HiBerGUILibrary]
    Editor[NeoWorkspaceEditor] --> HiBerGUI
    Plugin[Plugins] --> Core
    App[Main/Editor] --> CT[CrashTrackerHandleLib]
    App --> PH[PowerHelperBridgeLib]
```

## Pages

- [CrashTracker](./crashtracker.md)
- [PowerHelper](./powerhelper.md)
- [HiBerGUILibrary](./guibergui.md)
- [GitIgnoreMarkup](./gitignore-markup.md)
