---
title: NeoEditorExtension_Parser_SNBT (module)
description: Editor extension: edit/validate UI for `.snbt` configs.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoEditorExtension_Parser_SNBT/README.md
---

# NeoEditorExtension_Parser_SNBT

## Overview

`NeoEditorExtension_Parser_SNBT` is an editor extension (SHARED) implementing `NeoCore::IConfigEditorExtension`, providing editing/validation for `.snbt` configs.

## Goals

- Let authors edit safely in the GUI instead of hand-writing JSON;
- Pair with the runtime plugin (merge/resolve);
- Depend only on editor interfaces.

## Boundaries

**Does**: create editor widgets, parse/serialize, validate input.
**Does not**: sync/download (runtime plugins).

## Dependencies

```mermaid
flowchart LR
    E[NeoEditorExtension_Parser_SNBT] --> Core[NeoCore]
    E --> QtWidgets[Qt6::Widgets]
```

## Files

| File | Purpose |
|---|---|
| `src/` | Implementation and `CreateConfigEditor` |
| `NeoEditorExtension_Parser_SNBT.meta.json` | Capability declaration |
| `CMakeLists.txt` | SHARED target |

## Build integration

- target `NeoEditorExtension_Parser_SNBT` (SHARED);
- copied to `editor/extension/parsers/`;
- scanned/registered at editor startup.

## Public symbols

| Symbol | Description |
|---|---|
| `CreateConfigEditor` | Factory (`__declspec(dllexport)`) |
| `IConfigEditorExtension` impl | createEditor/... |

## Related

- [usage.md](./usage.md)
- [Editor extensions](../../../extensions/editor/index.md)