---
title: CrashTracker
description: NSUM crash capture and offline analysis: dump/trace generation, report layout and usage.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/extensions/libraries/crashtracker.md
---

# CrashTracker

CrashTracker captures the state on process crash (dump + trace + meta) and provides an offline analyzer.

## Components

| Component | Purpose |
|---|---|
| `CrashTrackerHandleLib` | Static lib: SEH/CRT hooks, report writing, analyzer launch |
| `CrashTracker.exe` | Analyzer: reads dump/trace/meta and produces a readable report |

## Report layout

```text
<exe>/crash-report/<date_time>/
├─ crash_<ts>.dmp
├─ crash_<ts>.trace
├─ crash_<ts>.meta
└─ crash_signal.txt
```

## Usage

- Hooks are installed automatically by the main program/editor;
- On crash the analyzer is launched automatically;
- Manual analysis: `CrashTracker.exe <dump/trace/meta>`.

## Hook behavior

- SEH unhandled exceptions write the real exception code;
- CRT reports (HEAP CORRUPTION) write `0xC0000374`;
- The process is terminated after writing to avoid double crashes/hangs;
- The CRT hook must not allocate on the heap (stack buffers + Win32 only).

## Notes

- Reports live next to the exe; exclude `crash-report/` from packages;
- Analysis needs symbols (PDB) for source lines;
- Heap-corruption processes may deadlock in ShellExecuteEx; `CreateProcessA` is used instead.

## Related

- [Libraries](./index.md)
- [CrashTracker module](../../developer/Modules/CrashTracker/README.md)
