---
title: NeoCLI (module)
description: CLI subsystem: argument parsing, info/exec/flow dispatch, JSON output.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoCLI/README.md
---

# NeoCLI

## Overview

CLI subsystem: argument parsing, info/exec/flow dispatch, JSON output.

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
| NeoCore, NeoWorkspace, NeoBuild, Qt6::Core, nlohmann_json | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `include/arg_parser.h`, `include/cli_dispatcher.h`, `include/cli_output.h`, `src/*` | Core sources/headers |

## Build integration

- target: `NeoCLI` (STATIC);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `ArgParser` | class | Parse args |\n| `CliDispatcher` | class | Dispatch commands |\n| `CliOutput` | class | JSON output |

## Related

- [usage.md](./usage.md)