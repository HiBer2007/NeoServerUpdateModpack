---
title: CrashTrackerHandleLib (module)
description: Crash capture library: SEH/CRT hooks, dump/trace/meta writing, analyzer launch.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/CrashTrackerHandleLib/README.md
---

# CrashTrackerHandleLib

## Overview

Crash capture library: SEH/CRT hooks, dump/trace/meta writing, analyzer launch.

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
| DbgHelp, Win32 | link/interface | Build and runtime |

## Files

| File | Purpose |
|---|---|
| `include/crash_reporter.h`, `src/crash_reporter.cpp` | Core sources/headers |

## Build integration

- target: `CrashTrackerHandleLib` (STATIC);
- build via the root CMake presets; deploy artifacts follow the module type.

## Public symbols

| Symbol | Type | Description |
|---|---|---|
| `HiBerCTM::InstallCrashHandler` | function | Install hooks |\n| `HiBerCTM::SetCrashAppName` | function | App name |\n| `HiBerCTM::InstallCrtReportHook` | function | CRT hook |

## Related

- [usage.md](./usage.md)