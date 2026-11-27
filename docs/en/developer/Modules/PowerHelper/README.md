---
title: PowerHelper (module)
description: Markdown documentation reader: WebView2 rendering, TOC, CLI, QTextBrowser fallback.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/PowerHelper/README.md
---

# PowerHelper

## Overview

Markdown documentation reader: WebView2 rendering, TOC, CLI, QTextBrowser fallback.

## Goals

- Clear boundaries and reusable APIs;
- UTF-8 paths and explicit failures;
- Minimal coupling to surrounding modules.

## Boundaries

**Does**: its own responsibility as described above.
**Does not**: implement other modules' responsibilities.

## Dependencies

| Dependency | Type | Purpose |
|---|---|---|
| Qt6, WebView2, cmark-gfm | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `core/`, `app/`, `bridge/` | Core sources/headers |

## Build integration

- target: `PowerHelper` (DLL/STATIC/EXE);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `PowerHelper::MarkdownRenderer` | class | Render |\n| `PowerHelper::Bridge::launchReader` | function | Launch reader |

## Related

- [usage.md](./usage.md)