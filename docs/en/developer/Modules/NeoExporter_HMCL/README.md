---
title: NeoExporter_HMCL (module)
description: Exporter plugin for hmcl ((directory)).
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoExporter_HMCL/README.md
---

# NeoExporter_HMCL

## Overview

`NeoExporter_HMCL` is an exporter plugin (SHARED) implementing `NeoCore::IModpackExporter`, exporting builds as hmcl ((directory)).

## Goals

- Pluggable formats; GUI/CLI forms come from meta;
- Exporter provides its build entry (`build_modpack`);
- No UI dependencies.

## Boundaries

**Does**: declare format/extension/fields, build and export.
**Does not**: sync policies or instance markers (engine responsibility).

## Dependencies

```mermaid
flowchart LR
    E[NeoExporter_HMCL] --> Core[NeoCore]
    E --> Qt[Qt6::Core]
    E --> Zip[libzippp]
```

## Files

| File | Purpose |
|---|---|
| `src/exporter_*.cpp` | Implementation and `CreateExporter` |
| `NeoExporter_HMCL.meta.json` | Format/extension/fields |
| `CMakeLists.txt` | SHARED target |

## Build integration

- target `NeoExporter_HMCL` (SHARED);
- copied to `exporters/`;
- exports `CreateExporter`.

## Public symbols

| Symbol | Description |
|---|---|
| `CreateExporter` | Factory (`__declspec(dllexport)`) |
| `IModpackExporter` impl | format/extension/build/export |

## Related

- [usage.md](./usage.md)
- [Exporter docs](../../../extensions/exporters/index.md)