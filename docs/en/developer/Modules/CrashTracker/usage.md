---
title: CrashTracker usage
description: CLI and GUI usage of CrashTracker.exe, how to read its reports, the watch modes and troubleshooting notes.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/CrashTracker/usage.md
---

# CrashTracker usage

## Quick start

```powershell
# Show the newest crash report (prints readable text to the terminal)
CrashTracker.exe --cli --latest

# List all reports
CrashTracker.exe --cli --list

# Analyze a specific dump
CrashTracker.exe --cli .\crash-report\20261127_181530\crash_20261127_181530.dmp
```

GUI usage: double-click `CrashTracker.exe` to open the window, then pick a `*.dmp`; alternatively pass the `.dmp` path as an argument.

## Public API

This tool is an EXE, so its external contract is **command-line arguments plus the report text format**.

| Argument | Form | Description |
|---|---|---|
| `--cli` | `--cli <file.dmp\|file.trace\|file.txt>` | analyze a single file, print the report to stdout and archive `_report.txt` |
| `--cli --list` | `--cli --list` | list `<exe>/crash-report/*/crash_*.dmp` in reverse chronological order |
| `--cli --latest` | `--cli --latest` | take the newest report and print it (including the CRT message from `.meta`) |
| bare path | `CrashTracker.exe <file.dmp>` | open that dump directly in the GUI |
| `--watch` | `--watch=<signalFile>` | watch a signal file: as soon as it appears, read the path inside and open it (polled every 500 ms) |
| `--watch-pid` | `--watch-pid=<pid>` | watch a process: after it exits, scan for dumps from the last 120 s and open one |
| `--watch-dir` | `--watch-dir=<dir>` | together with `--watch-pid`, the directory to scan |
| no arguments | `CrashTracker.exe` | open an empty window and choose a dump manually |

Exit codes: `0` success (`--list` with no reports also returns 0); `1` load failure or a CLI usage error.

## Typical usage

### Crash scene forensics

```powershell
# 1) After a crash the report directory looks like this (written by CrashTrackerHandleLib)
#    <exe>/crash-report/20261127_181530/crash_20261127_181530.dmp
#                                  /crash_20261127_181530.trace
#                                  /crash_20261127_181530.meta
#                                  /crash_signal.txt

# 2) Generate a readable report and archive it in the same directory
CrashTracker.exe --cli .\crash-report\20261127_181530\crash_20261127_181530.dmp

# 3) The archived file (scripts can read this text directly for attribution)
Get-Content .\crash-report\20261127_181530\crash_20261127_181530_report.txt
```

### What the report contains

| Section | Content |
|---|---|
| Exception | exception name (such as `ACCESS_VIOLATION`), exception code, exception address; a trace-only report shows `TRACE_ONLY` |
| System information | CPU model and similar (read from the registry key `HARDWARE\DESCRIPTION\System\CentralProcessor\0`) |
| Crashing module | the application name from the first line of `.meta` plus this crash's module information |
| Module list | loaded modules recorded in the minidump |
| Call stack | a list of `address + module` entries (no PDB symbols, no source line numbers) |
| metadata | the CRT message section of `.meta` (between `===CRT_MESSAGE===` and `===CRT_HELP===`) |
| Crash explanations | the `===CRASH_DESCRIPTIONS===` section of `.meta` (written by the host via `AddCrashTypeInfo`) |

### Letting the host launch the analysis automatically

Once the host side (`CrashTrackerHandleLib`) has installed its handlers, a crash automatically spawns the co-located `CrashTracker.exe <dmp>` via `CreateProcessA`. If you prefer the report in the current terminal instead of a GUI window, the host should call `HiBerCTM::SetCrashCliMode(true)` first (the library then runs `--cli` through an anonymous pipe and forwards the output).

## Notes

- **The working directory determines the scan root**: the report root is fixed to `QCoreApplication::applicationDirPath() + "/crash-report"`, so moving `CrashTracker.exe` elsewhere makes it miss the host's crash records - that is also why deployment keeps it next to the main application.
- **No symbols means no line numbers**: the report only gives addresses and module names. To resolve functions or lines, analyze the dump separately with `dumpbin`/WinDbg and the matching PDBs.
- **`.meta` is a text protocol**: parsing depends on the separator lines `===CRT_MESSAGE===` / `===CRT_HELP===` / `===CRASH_DESCRIPTIONS===`; changing how `CrashTrackerHandleLib` writes `.meta` requires updating this tool too (`CrashWindow` and `cliShowEntry` each parse it once).
- **CLI mode attaches a console automatically**: a GUI-subsystem EXE started without a console calls `AttachConsole(ATTACH_PARENT_PROCESS)` and falls back to `AllocConsole()`; when a script redirects stdout it receives the plain-text report.
- **`--watch-pid` only considers dumps from the last 120 seconds**: older records are ignored so that a stale report is not opened by mistake.
- **Archived reports overwrite same-named files**: `_report.txt` is derived from the dump's base name, so analyzing the same dump twice overwrites the previous archive.
- **Do not treat it as a live debugger**: it never attaches to a process or changes execution flow; terminating the process is the crash handler's job.
- **WER/system dialogs may still appear**: if system policy enables Windows Error Reporting, WER prompts beyond the `.dmp` are unrelated to this tool.

## Related

- [README.md](./README.md)
- [CrashTrackerHandleLib module](../CrashTrackerHandleLib/README.md)
- [CrashTrackerHandleLib usage](../CrashTrackerHandleLib/usage.md)
- [Testing and crash smoke tests](../../testing.md)
