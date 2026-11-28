---
title: Command Line (CLI)
description: Runtime CLI command categories, JSON output and exit codes for advanced users and automation.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/user/cli.md
---

# Command Line (CLI)

`NeoServerUpdateModpack.exe` enters CLI mode without GUI arguments. Commands are split into three mutually exclusive categories:

```text
info  read-only queries (structured output)
flow  guided flows (GUI/console, reusing the GUI wizard)
exec  operations (build, export, sync, tools)
```

## Basic usage

```powershell
NeoServerUpdateModpack.exe <category> <verb> [options]
```

Global options:

| Option | Description |
|---|---|
| `--json` | Emit a JSON block (`=====JSON-BEGIN=====` / `=====JSON-END=====`); human logs go to stderr |
| `--verbose` | Verbose logging |
| `--silent` | Silent (errors only) |

## info

```powershell
.\NeoServerUpdateModpack.exe info version
.\NeoServerUpdateModpack.exe info system
.\NeoServerUpdateModpack.exe info git
.\NeoServerUpdateModpack.exe info modpacks --repo <repository> --json
.\NeoServerUpdateModpack.exe info status --repo <repository> --json
.\NeoServerUpdateModpack.exe info preview --repo <repository> --modpack base --format nsum --json
.\NeoServerUpdateModpack.exe info plugins --json
.\NeoServerUpdateModpack.exe info exporters --json
```

## flow

```powershell
# Reuse the GUI wizard, collect parameters, print JSON
.\NeoServerUpdateModpack.exe flow gui --from repo --to checklist --prefill repo=<repository>
# Text wizard
.\NeoServerUpdateModpack.exe flow console
```

## exec

```powershell
# Build and export .nsumpack (no embedded files by default)
.\NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main --format nsum --export out.nsumpack

# Explicitly embed selected files (regex)
.\NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main --format nsum `
    --embed-patterns '^config/.*\.json$' --export out.nsumpack

# Export only (reuse an existing build)
.\NeoServerUpdateModpack.exe exec export --export out.nsumpack --format nsum --repo <repository> --modpack base

# Other
.\NeoServerUpdateModpack.exe exec sync-serverconfig --save <world> --repo <repository>
.\NeoServerUpdateModpack.exe exec verify-repo --repo <repository>
.\NeoServerUpdateModpack.exe exec resolve-pointer <file.pointer>
.\NeoServerUpdateModpack.exe exec git-update
```

## JSON output

```text
=====JSON-BEGIN=====
{"category":"info","command":"version","data":{...}}
=====JSON-END=====
```

- `category`: command category;
- `command`: verb;
- `data`: structured result.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Operation failed (build/verify/git, …) |
| 2 | Invalid arguments or usage |

> The installer's `hci_cli` has an additional exit 10 (external download pause), unrelated to the Runtime CLI.

## Tips

- Automation should prefer `--json`;
- Run `info preview` before building;
- Keep `.nsumpack` exports metadata-only unless files are really needed (see [Unit format](../pack-author/nsumpack.md)).
