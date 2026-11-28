---
title: NeoParser_Properties (module)
description: Properties config parser plugin: capability, interface implementation and deployment.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/Modules/NeoParser_Properties/README.md
---

# NeoParser_Properties

## Overview

`NeoParser_Properties` is a runtime config parser plugin (SHARED) implementing `NeoCore::IConfigParser` for Properties configs.

## Goals

- Merge by `tracked_keys`, preserving player-local settings;
- Pair with the editor extension (highlighting/key list);
- Depend only on NeoCore interfaces.

## Boundaries

**Does**: parse configs, merge by key, list keys.
**Does not**: sync files, network, UI.

## Dependencies

```mermaid
flowchart LR
    P[NeoParser_Properties] --> Core[NeoCore]
    P --> Qt[Qt6::Core]
    P --> JSON[nlohmann_json]
```

## Files

| File | Purpose |
|---|---|
| `src/` | Implementation and `CreateParser` |
| `NeoParser_Properties.meta.json` | Capability declaration |
| `CMakeLists.txt` | SHARED target |

## Build integration

- target `NeoParser_Properties` (SHARED);
- copied to `parsers/` (DLL + meta);
- exports `CreateParser`.

## Public symbols

| Symbol | Description |
|---|---|
| `CreateParser` | Factory (`__declspec(dllexport)`) |
| `IConfigParser` impl | capability/can_handle/parse/merge/list_keys |

## Related

- [usage.md](./usage.md)
- [Parser docs](../../../extensions/parsers/index.md)