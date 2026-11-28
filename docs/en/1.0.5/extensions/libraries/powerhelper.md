---
title: PowerHelper
description: NSUM's built-in Markdown documentation reader: WebView2 rendering, CLI and fallback.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/extensions/libraries/powerhelper.md
---

# PowerHelper

PowerHelper is the documentation reader: "Open help docs" on the done page and the editor's "Help → Documentation" launch it against `exe/docs`.

## Components

| Component | Purpose |
|---|---|
| PowerHelperCore (DLL) | Markdown rendering, TOC, doc-group scanning |
| PowerHelperBridgeLib (STATIC) | Integration: `launchReader(path, extraArgs)` |
| PowerHelper.exe | GUI reader shell |

## Rendering

- Prefers system WebView2 (`webview2` vcpkg package);
- Falls back to `QTextBrowser` when WebView2 is missing;
- Supports anchors (`--anchor`) and TOC.

## CLI

```powershell
PowerHelper.exe <doc path> [--anchor <id>]
PowerHelper.exe render <md>
PowerHelper.exe toc <md>
```

## Launch order

`launchReader(target, extraArgs)` puts `target` in argv[1]; the wrong order makes `--anchor` be treated as the file and exits with 2.

## Notes

- GUI-subsystem EXEs do not set `$LASTEXITCODE`; verify with `Start-Process -Wait`;
- Docs must live under `exe/docs`;
- Language folders (`docs/zh`, `docs/en`) select the documentation language.

## Related

- [Libraries](./index.md)
- [PowerHelper module](../../developer/Modules/PowerHelper/README.md)
