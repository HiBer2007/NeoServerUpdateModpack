---
title: EditorDemo (module)
description: Minimal editor demo target for component testing; not shipped to users.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/EditorDemo/README.md
---

# EditorDemo

## Overview

Minimal editor demo target for component testing; not shipped to users.

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
| GUIWorker, HiBerGUILibrary | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `src/main.cpp` | Core sources/headers |

## Build integration

- target: `EditorDemo` (EXE);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| demo app | EXE | Example |

## Related

- [usage.md](./usage.md)