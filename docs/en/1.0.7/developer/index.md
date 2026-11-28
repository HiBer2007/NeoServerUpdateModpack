---
title: Developer Docs
description: NSUM architecture, design goals, base framework, buses and extension interfaces, ABI, module reference and processes.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/index.md
---

# Developer Docs

This section targets **NSUM core contributors, plugin authors and integrators**. Suggested reading order:

1. [Architecture](./architecture/index.md): system layout, design goals, data model, build/sync/pointers/downloads/security;
2. [Framework / Buses / ABI](./framework/index.md): NeoCore base framework, module layout, HIC event bus, extension registry, plugin contracts, cross-DLL ABI;
3. [Module Reference](./Modules/_meta.json): 34 modules with README (role/boundaries/dependencies/symbols) and usage (API/examples/pitfalls);
4. [Testing](./testing.md), [Contributing](./contributing.md), [Versioning](./versioning.md), [Release](./release.md);
5. [ADRs](./adr/index.md): key decisions and their rationale.

## Code map

| Layer | Modules | Artifact |
|---|---|---|
| Foundation | `NeoCore`, `CommonLoggerCPP` | STATIC |
| Engines | `NeoWorkspace`, `NeoBuild` | STATIC |
| Interfaces | `NeoCLI`, `GUIWorker`, `NeoWorkspaceEditor` | STATIC / EXE |
| Plugins | `NeoParser_*`, `NeoPointer_*`, `NeoExporter_*`, `NeoEditorExtension_*` | DLL |
| Toolchain | `HiBerCommonInstaller`, `CrashTracker`, `PowerHelper`, `HiBerGUILibrary`, `GitIgnoreMarkup` | Library / EXE |

## Build and validate

```powershell
cmake --preset msvc
cmake --build build --config Debug --target neo_deploy

pwsh -NoProfile -File tools/docs_check.ps1
```

See [testing.md](./testing.md) and AGENTS.md for the full commands and environment.
