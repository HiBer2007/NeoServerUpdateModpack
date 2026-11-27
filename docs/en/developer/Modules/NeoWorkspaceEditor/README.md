---
title: NeoWorkspaceEditor (module)
description: Repository content editor: GUI three tabs, Git panel, editor extensions, and a headless CLI for AI.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoWorkspaceEditor/README.md
---

# NeoWorkspaceEditor

## Overview

Repository content editor: GUI three tabs, Git panel, editor extensions, and a headless CLI for AI.

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
| NeoCore, NeoWorkspace, NeoBuild, GUIWorker, HiBerGUILibrary | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `src/main.cpp`, `src/editor_window.*`, `src/editor_cli.*` | Core sources/headers |

## Build integration

- target: `NeoWorkspaceEditor` (EXE);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `EditorWindow` | QMainWindow | GUI |\n| `editor_cli::run` | function | CLI |\n| `promoteEditorDocs` | function | Release pack-author docs |

## Related

- [usage.md](./usage.md)