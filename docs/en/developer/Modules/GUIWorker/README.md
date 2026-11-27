---
title: GUIWorker (module)
description: GUI domain layer: wizard pages, build pages, content IDE, pointer/policy/serverconfig editors and extension registry.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/GUIWorker/README.md
---

# GUIWorker

## Overview

GUI domain layer: wizard pages, build pages, content IDE, pointer/policy/serverconfig editors and extension registry.

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
| HiBerGUILibrary, NeoCore, NeoBuild, Qt6::Core, Qt6::Widgets (private PowerHelperBridgeLib, libzippp, tomlplusplus) | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `wizard_window.*`, `build_page.*`, `modpack_content_ide.*`, `pointer_editor_panel.*`, `editor_extension_registry.*` | Core sources/headers |

## Build integration

- target: `GUIWorker` (STATIC);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `WizardWindow` | QWidget | Build wizard |\n| `BuildPage` | QWidget | Build progress |\n| `ModpackContentIde` | QWidget | Content IDE |\n| `PointerEditorPanel` | QWidget | Pointer editor |\n| `EditorExtensionRegistry` | class | Extension registry |

## Related

- [usage.md](./usage.md)