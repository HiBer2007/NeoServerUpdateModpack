---
title: Overview
description: NSUM components, process boundaries and data flow: Runtime, CLI/GUI, bridge, installer, editor, MCDR and plugins.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/architecture/overview.md
---

# Overview

NSUM is composed of several processes/components working around one Git repository.

## Components

| Component | Form | Responsibility |
|---|---|---|
| NSUM Runtime | `NeoServerUpdateModpack.exe` + `deploy/` | Build, sync, export, downloads, CLI |
| GUI | same EXE (no args) | Wizard/build pages |
| CLI | same EXE (`info/flow/exec`) | Headless operations and JSON |
| nsum-bridge | HMCL extension jar | NSUM page, launch injection, Runtime install |
| HIC installer | standalone EXE | Install Runtime/Editor, bundled Git |
| NeoWorkspaceEditor | standalone EXE | Repository content editor + CLI |
| MCDR plugin | Python plugin | Server core install/sync |
| Plugin DLLs | `parsers/pointers/exporters/editor/extension` | Parsing, downloads, export, editing |

## Processes and data flow

```mermaid
flowchart TB
    subgraph Client
        HMCL[HMCL + FLFHMCL] --> Bridge[nsum-bridge]
        Bridge --> Runtime[NSUM Runtime]
        Runtime --> Plugins[(Plugin DLLs)]
        Runtime --> Instance[Instance directory]
        Editor[NeoWorkspaceEditor] --> Repo[(Git repository)]
        Editor --> Plugins2[(Editor extensions)]
    end
    subgraph Server
        MCDR[MCDR plugin] --> RuntimeS[Runtime]
        RuntimeS --> ServerDir[Server directory]
    end
    Repo --> Runtime
    Repo --> RuntimeS
    Metadata[metadata site] --> Bridge
    Metadata --> HIC[HIC installer]
    HIC --> Runtime
```

## Module layers

```mermaid
flowchart LR
    NeoCore --> CommonLogger[CommonLoggerCPP]
    NeoWorkspace --> NeoCore
    NeoBuild --> NeoWorkspace
    NeoBuild --> NeoCore
    NeoCLI --> NeoBuild
    GUIWorker --> NeoBuild
    Editor[NeoWorkspaceEditor] --> NeoWorkspace
    Editor --> GUIWorker
    Plugins --> NeoCore
    HIC --> NeoCore
```

- **NeoCore**: interfaces, PluginLoader, paths, parallel downloader;
- **NeoWorkspace**: workspace.json, branches, Git operations, sync policies;
- **NeoBuild**: build engine, branch merge, pointer download, serverconfig, exporter loading;
- **NeoCLI/GUIWorker**: CLI and GUI domain layers;
- **Plugins**: loaded as DLLs at runtime.

## Data directories

| Directory | Content |
|---|---|
| `.hmcl/nsum/runtime/` | Runtime |
| `.hmcl/nsum/repos/` | Cloned repositories |
| `.hmcl/nsum/cache/files/` | Embedded file cache (SHA-256) |
| `.minecraft/versions/.cache/` | Build-side pointer cache (SHA-256) |
| `config/history/` | GUI history and remote cache |

## Key constraints

- Runtime does not depend on system Git (bundled MinGit);
- Plugins interact via exported symbols; cross-DLL STL/Qt passing is constrained (see [ABI](../framework/abi.md));
- All paths are UTF-8 (Windows argv rebuilt);
- Sync failure aborts launch by default.

## Related

- [Design goals](./design-goals.md)
- [Data model](./data-model.md)
