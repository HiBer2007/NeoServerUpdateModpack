---
title: HiBerCommonInstaller (module)
description: Generic installer framework (HIC): JSON flows + Lua conditions, extension host, GUI/TUI/CLI shells.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/HiBerCommonInstaller/README.md
---

# HiBerCommonInstaller

## Overview

Generic installer framework (HIC): JSON flows + Lua conditions, extension host, GUI/TUI/CLI shells.

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
| hci_core, hci_flow, hci_ext, Qt6 (GUI shell) | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `core/`, `flow/`, `ext/`, `gui/`, `tui/`, `cli/` | Core sources/headers |

## Build integration

- target: `HiBerCommonInstaller` (LIB/EXE);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `hci::EventBus` | class | Event bus |\n| `hci::ServiceRegistry` | class | Services |\n| `hci::ExtensionLoader` | class | Extensions |\n| `hci::FlowRunner` | class | Flow execution |

## Related

- [usage.md](./usage.md)