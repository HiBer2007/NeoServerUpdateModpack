---
title: NeoPointer_DirectURL (module)
description: Pointer resolver plugin for direct_url.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/Modules/NeoPointer_DirectURL/README.md
---

# NeoPointer_DirectURL

## Overview

`NeoPointer_DirectURL` is a runtime pointer resolver plugin (SHARED) implementing `NeoCore::IPluginPointer` to download directly from the pointer metadata URL.

## Goals

- Resolve pointer metadata into a download URL;
- Pair with the editor extension (no hand-written JSON);
- Depend only on NeoCore interfaces.

## Boundaries

**Does**: can_handle, resolve URL, validate downloads.
**Does not**: cache management (PointerDownloader) or UI.

## Dependencies

```mermaid
flowchart LR
    P[NeoPointer_DirectURL] --> Core[NeoCore]
    P --> JSON[nlohmann_json]
```

## Files

| File | Purpose |
|---|---|
| `src/` | Implementation and `CreatePointer` |
| `NeoPointer_DirectURL.meta.json` | Capability declaration |
| `CMakeLists.txt` | SHARED target |

## Build integration

- target `NeoPointer_DirectURL` (SHARED);
- copied to `pointers/`;
- exports `CreatePointer`.

## Public symbols

| Symbol | Description |
|---|---|
| `CreatePointer` | Factory (`__declspec(dllexport)`) |
| `IPluginPointer` impl | name/can_handle/resolve_url/validate |

## Related

- [usage.md](./usage.md)
- [Pointer docs](../../../extensions/pointers/index.md)