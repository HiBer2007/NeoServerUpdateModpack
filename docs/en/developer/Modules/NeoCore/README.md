---
title: NeoCore (module)
description: Public foundation library: plugin interfaces, loader, UTF-8 paths, cancellation, error codes, progress and parallel downloads.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoCore/README.md
---

# NeoCore

## Overview

Public foundation library: plugin interfaces, loader, UTF-8 paths, cancellation, error codes, progress and parallel downloads.

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
| CommonLoggerCPP, PowerHelperBridgeLib, Qt6::Widgets, Qt6::Network, nlohmann_json | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `include/IConfigParser.h`, `include/IPluginPointer.h`, `include/IModpackExporter.h`, `include/PluginLoader.h`, `include/parallel_downloader.h`, `include/path_utf8.h`, `include/cancel_token.h`, `include/error_codes.h` | Core sources/headers |

## Build integration

- target: `NeoCore` (STATIC);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `IConfigParser` | interface | Config parse/merge |\n| `IPluginPointer` | interface | Pointer resolution |\n| `IModpackExporter` | interface | Export formats |\n| `PluginLoader` | class | Parser loading |\n| `CancelToken` | class | Cancellation |\n| `ParallelDownloader` | class | Range downloads |

## Related

- [usage.md](./usage.md)