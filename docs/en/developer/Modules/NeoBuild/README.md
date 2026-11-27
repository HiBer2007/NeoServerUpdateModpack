---
title: NeoBuild (module)
description: Build engine: clone/merge/scan/pointer download/policy sync/exporter loading.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoBuild/README.md
---

# NeoBuild

## Overview

Build engine: clone/merge/scan/pointer download/policy sync/exporter loading.

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
| NeoCore, NeoWorkspace, Qt6::Core, Qt6::Network, libzippp (private tomlplusplus) | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `include/build_engine.h`, `include/branch_merger.h`, `include/pointer_downloader.h`, `include/modpack_exporter.h`, `include/serverconfig_sync.h`, `include/sync_policy_executor.h` | Core sources/headers |

## Build integration

- target: `NeoBuild` (STATIC);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `BuildEngine` | class | Build entry |\n| `BranchMerger` | class | Parent merge |\n| `PointerDownloader` | class | Pointer downloads |\n| `ModpackExporter` | class | Exporter management |\n| `ServerConfigSync` | class | L3 sync |

## Related

- [usage.md](./usage.md)