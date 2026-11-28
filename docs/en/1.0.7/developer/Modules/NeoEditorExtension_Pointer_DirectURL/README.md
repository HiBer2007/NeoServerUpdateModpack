---
title: NeoEditorExtension_Pointer_DirectURL (module)
description: Editor extension: edit/validate UI for `direct_url` pointers.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/Modules/NeoEditorExtension_Pointer_DirectURL/README.md
---

# NeoEditorExtension_Pointer_DirectURL

## Overview

`NeoEditorExtension_Pointer_DirectURL` is an editor extension (SHARED) implementing `NeoCore::IPointerEditorExtension`, providing editing/validation for `direct_url` pointers.

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
    E[NeoEditorExtension_Pointer_DirectURL] --> Core[NeoCore]
    E --> QtWidgets[Qt6::Widgets]
```

## Files

| File | Purpose |
|---|---|
| `src/` | Implementation and `CreateEditorExtension` |
| `NeoEditorExtension_Pointer_DirectURL.meta.json` | Capability declaration |
| `CMakeLists.txt` | SHARED target |

## Build integration

- target `NeoEditorExtension_Pointer_DirectURL` (SHARED);
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