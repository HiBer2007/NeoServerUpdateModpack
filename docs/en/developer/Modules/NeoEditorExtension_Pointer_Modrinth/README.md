---
title: NeoEditorExtension_Pointer_Modrinth (module)
description: Editor extension: edit/validate UI for `modrinth` pointers.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoEditorExtension_Pointer_Modrinth/README.md
---

# NeoEditorExtension_Pointer_Modrinth

## Overview

`NeoEditorExtension_Pointer_Modrinth` is an editor extension (SHARED) implementing `NeoCore::IPointerEditorExtension`, providing editing/validation for `modrinth` pointers.

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
    E[NeoEditorExtension_Pointer_Modrinth] --> Core[NeoCore]
    E --> QtWidgets[Qt6::Widgets]
```

## Files

| File | Purpose |
|---|---|
| `src/` | Implementation and `CreateEditorExtension` |
| `NeoEditorExtension_Pointer_Modrinth.meta.json` | Capability declaration |
| `CMakeLists.txt` | SHARED target |

## Build integration

- target `NeoEditorExtension_Pointer_Modrinth` (SHARED);
- copied to `editor/extension/pointers/`;
- scanned/registered at editor startup.

## Public symbols

| Symbol | Description |
|---|---|
| `CreateEditorExtension` | Factory (`__declspec(dllexport)`) |
| `IPointerEditorExtension` impl | createEditor/... |

## Related

- [usage.md](./usage.md)
- [Editor extensions](../../../extensions/editor/index.md)