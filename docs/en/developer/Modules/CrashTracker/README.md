---
title: CrashTracker (module)
description: Crash analyzer: reads dump/trace/meta and produces a readable report.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/CrashTracker/README.md
---

# CrashTracker

## Overview

Crash analyzer: reads dump/trace/meta and produces a readable report.

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
| CrashTrackerHandleLib, Qt6 | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `src/main.cpp`, analyzer UI | Core sources/headers |

## Build integration

- target: `CrashTracker` (EXE);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `CrashTracker.exe` | EXE | Analyzer |

## Related

- [usage.md](./usage.md)