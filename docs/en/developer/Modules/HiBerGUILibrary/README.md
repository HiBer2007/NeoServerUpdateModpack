---
title: HiBerGUILibrary (module)
description: Generic Qt widgets: trees, Git panel, progress, toast, file editor, deep tree behavior.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/HiBerGUILibrary/README.md
---

# HiBerGUILibrary

## Overview

Generic Qt widgets: trees, Git panel, progress, toast, file editor, deep tree behavior.

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
| Qt6::Widgets, nlohmann_json | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `include/*.h` (repo_tree_panel, output_tree_panel, git_panel, animated_progress, progress_card, toast_notification, file_content_editor, deep_tree_behavior) | Core sources/headers |

## Build integration

- target: `HiBerGUILibrary` (STATIC);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `RepoTreePanel` | QWidget | Repository tree |\n| `GitPanel` | QWidget | Git operations |\n| `AnimatedProgress` | QWidget | Progress |\n| `DeepTreeBehavior` | QObject | Tree behavior |

## Related

- [usage.md](./usage.md)