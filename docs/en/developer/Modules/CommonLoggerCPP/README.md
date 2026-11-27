---
title: CommonLoggerCPP (module)
description: Shared logging: CLogger facade over spdlog, plugin log sink injection.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/CommonLoggerCPP/README.md
---

# CommonLoggerCPP

## Overview

Shared logging: CLogger facade over spdlog, plugin log sink injection.

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
| spdlog, Qt6::Core (transitively) | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `include/logger.h`, `include/plugin_log_sink.h`, `src/logger.cpp` | Core sources/headers |

## Build integration

- target: `CommonLoggerCPP` (STATIC);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `CLogger` | class | Log facade |\n| `ILogSink` | interface | Sink |\n| `LoggerLogSink` | class | Host sink |

## Related

- [usage.md](./usage.md)