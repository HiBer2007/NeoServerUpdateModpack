---
title: GitIgnoreMarkup (module)
description: Marked .gitignore blocks plus an editing GUI, so tools can manage rules without touching user lines.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/GitIgnoreMarkup/README.md
---

# GitIgnoreMarkup

## Overview

Marked .gitignore blocks plus an editing GUI, so tools can manage rules without touching user lines.

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
| Qt6::Core/Widgets, HiBerGUILibrary (GUI part) | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `include/*`, `src/*` | Core sources/headers |

## Build integration

- target: `GitIgnoreMarkup` (STATIC);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `GitIgnoreMarkup` | namespace/class | Marked blocks |

## Related

- [usage.md](./usage.md)