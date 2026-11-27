---
title: NeoWorkspace (module)
description: Workspace engine: workspace.json, branches, Git operations, sync engine, file scanning and history.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoWorkspace/README.md
---

# NeoWorkspace

## Overview

Workspace engine: workspace.json, branches, Git operations, sync engine, file scanning and history.

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
| NeoCore, Qt6::Core, nlohmann_json | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `include/workspace_manager.h`, `include/git_operations.h`, `include/sync_engine.h`, `include/sync_policy.h`, `include/file_scanner.h`, `include/history_store.h` | Core sources/headers |

## Build integration

- target: `NeoWorkspace` (STATIC);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `WorkspaceManager` | class | Workspace/branches |\n| `GitOperations` | class | Git commands |\n| `SyncEngine` | class | Sync |\n| `FileScanner` | class | Scan/hash |\n| `HistoryStore` | class | History |

## Related

- [usage.md](./usage.md)