---
title: NeoWorkspaceEditor usage
description: GUI startup, every info/exec CLI subcommand, the JSON output contract and troubleshooting notes for NeoWorkspaceEditor.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoWorkspaceEditor/usage.md
---

# NeoWorkspaceEditor usage

## Quick start

```powershell
# GUI: open, create or clone a repository
NeoWorkspaceEditor.exe                          # empty window, choose a repository from the menu
NeoWorkspaceEditor.exe <repo>\workspace.json    # open a specific workspace directly

# CLI (no GUI): query repository information, stdout carries one JSON object
NeoWorkspaceEditor.exe info workspace --repo <repo>
NeoWorkspaceEditor.exe info branches  --repo <repo>
NeoWorkspaceEditor.exe exec validate  --repo <repo>
```

CLI detection rule: when the first non-program argument is `info` / `exec` / `help` the process enters CLI mode (`editor_cli::is_cli_invocation`) and never creates a `QApplication`. `--repo` defaults to the current directory, which must contain `workspace.json`.

## Public API

This tool is an EXE, so its external contract is **CLI subcommands plus JSON output**. Every command prints a single JSON object on stdout:

```json
{ "ok": true, "command": "info.workspace", "repo": "<repo>", "data": { } }
```

On failure it prints `{ "ok": false, "command": ..., "error": "..." }` (`data` is included when available, for example git's `exit_code`/`stdout`/`stderr`). `--text` switches to a short human-readable summary (`OK: <command>` / `ERROR: ...` on stderr). Exit codes: `0` success, `1` operation failed, `2` usage/argument error.

### info subcommands

| Command | Usage | Description |
|---|---|---|
| `info workspace` | `info workspace [--repo <dir>] [--text]` | workspace summary: name, MC version, loader, branch list, export metadata, sync policies |
| `info branches` | `info branches [--repo <dir>] [--text]` | branch array and count |
| `info branch` | `info branch <name> [--repo <dir>]` | single branch details + child branches + `branch_config` summary + file count |
| `info tree` | `info tree [--path <rel>] [--depth <n>] [--all] [--repo <dir>]` | directory tree (default depth 2) |
| `info pointers` | `info pointers [--repo <dir>]` | pointer files and resolver overview |
| `info sync` | `info sync [--repo <dir>]` | effective sync policies |
| `info file` | `info file <rel> [--base64] [--force]` | read a file inside the repository |
| `info git` | `info git` | branch/HEAD/upstream/ahead-behind/staged and untracked files |

### exec subcommands

| Command | Usage | Description |
|---|---|---|
| `branch-add` | `exec branch-add --name <n> [--parent <p>] [--type universal\|client\|server] [--core <id>] [--description <s>] [--modloader-version <v>] [--game-version <v>]` | create a branch and copy the default four-section config |
| `branch-set` | `exec branch-set --name <n> [--parent <p>\|--parent ""] [--type ...] [--core <id>] [--description <s>] [--modloader-version <v>] [--hidden true\|false] [--game-version <v>]` | change branch properties |
| `branch-rename` | `exec branch-rename --name <n> --to <new>` | rename (directory and config together) |
| `branch-remove` | `exec branch-remove --name <n> [--force] [--purge]` | delete a branch (`--purge` also removes the data directory) |
| `file-write` | `exec file-write --path <rel> (--content <text> \| --from-file <src>) [--append]` | write a file inside the repository |
| `file-delete` | `exec file-delete --path <rel> [--recursive]` | delete a file or directory |
| `file-move` | `exec file-move --from <rel> --to <rel>` | move or rename |
| `json-get` | `exec json-get --file <rel> [--pointer /a/b]` | read a value by JSON Pointer |
| `json-set` | `exec json-set --file <rel> --pointer /a/b (--value <json>\|--value-string <s>\|--delete)` | change a value by JSON Pointer |
| `pointer-add` | `exec pointer-add --path <rel> --sha256 <hex> --resolver <name> [--url <url>] [--branch <name>] [--metadata-json <json>]` | add a pointer and update the manifest |
| `pointer-remove` | `exec pointer-remove --path <rel> [--branch <name>]` | remove a pointer |
| `validate` | `exec validate [--repo <dir>]` | structural validation (duplicate names / missing parents / parent cycles) |
| `git-status` | `exec git-status` | git status (same data source as `info git`) |
| `git-add` | `exec git-add [<path>...] [--all]` | stage changes |
| `git-commit` | `exec git-commit --message <msg> [--add] [<path>...] [--amend] [--allow-empty] [--no-verify]` | commit (`--message` is mandatory) |
| `git-push` | `exec git-push [--remote <r>] [--branch <b>] [--set-upstream] [--force]` | push; `--force` actually uses `--force-with-lease` |
| `git-pull` | `exec git-pull [--remote <r>] [--branch <b>] [--rebase] [--ff-only]` | pull |
| `git-fetch` | `exec git-fetch [--remote <r>] [--prune]` | fetch |
| `git-log` | `exec git-log [--limit <n>]` | history |
| `git-diff` | `exec git-diff [--cached] [--stat] [<path>]` | diff |
| `git-checkout` | `exec git-checkout --branch <b> [--create] [--start <ref>] [--force]` | switch branches |
| `git-reset` | `exec git-reset --mode soft\|mixed\|hard --ref <ref> [--confirm-hard]` | reset; `hard` requires `--confirm-hard` |
| `git-config` | `exec git-config --key <k> [--value <v>] [--global]` | read or write git config |
| `help` | `NeoWorkspaceEditor.exe help` | print usage (also one of the CLI entry points) |

## Typical usage

### Querying the workspace summary

```powershell
NeoWorkspaceEditor.exe info workspace --repo <repo>
```

```json
{"ok":true,"command":"info.workspace","data":{
  "name":"E2E","minecraft":"1.21.1","modloader":"neoforge","modloader_version":"21.1.209",
  "branch_count":1,
  "branches":[{"name":"base","parent":"","type":"server","core":"paper",
               "description":"e2e","hidden":false,"has_config":true,"has_data_dir":true}],
  "git":{"remote":"","default_branch":"main"},
  "sync_policies":{"default_folder_policy":"mirror",
                   "folders":{"config":"mirror","logs":"skip"},
                   "files":{"config/partial.json":{"mode":"partial","tracked_keys":["keep","server"]}}},
  "workspace_file":"<repo>/workspace.json"},"repo":"<repo>"}
```

### Adding a branch and validating

```powershell
NeoWorkspaceEditor.exe exec branch-add --name beta --parent base --type server --core paper
NeoWorkspaceEditor.exe exec validate
```

The `data` of `validate` looks like:

```json
{"ok":true,"command":"exec.validate","data":{
  "loaded":true,"valid":true,"workspace_manager_valid":false,
  "branches":[{"name":"base","parent":"","type":"server","core":"paper",
               "has_config":true,"has_data_dir":true}],
  "errors":[],"warnings":[]}}
```

### Changing a single configuration key (without rewriting the whole file)

```powershell
NeoWorkspaceEditor.exe exec json-get --file workspace.json --pointer /workspace/name
NeoWorkspaceEditor.exe exec json-set --file workspace.json --pointer /workspace/name --value-string "My modpack"
```

> A JSON Pointer must start with `/`; the empty pointer `/` is rejected.

### Turning a jar into a pointer

```powershell
NeoWorkspaceEditor.exe exec pointer-add --path mods/example.jar --sha256 <sha256> `
    --resolver modrinth --metadata-json '{"project_id":"...","version_id":"..."}'
```

`pointer-add` writes `branch_config/<branch>.pointer` and updates `file_manifest`/`pointer_files`, matching the GUI's `PointerEditorPanel` behavior (it does not produce a hand-written `branches/<name>/x.pointer`).

### Git commit flow

```powershell
NeoWorkspaceEditor.exe exec git-status
NeoWorkspaceEditor.exe exec git-add --all
NeoWorkspaceEditor.exe exec git-commit --message "docs: update module docs"
NeoWorkspaceEditor.exe exec git-push --set-upstream
```

When git fails the JSON still carries `data.exit_code` / `data.stdout` / `data.stderr` with `ok:false`, so scripts can read the cause directly.

## Notes

- **Paths are confined to the repository**: `--path`/`--from`/`--to`/`--file` reject absolute paths and `..`; `info file` refuses binary content unless `--base64` or `--force` is passed.
- **CLI logs go to stderr**: `run()` replaces the default logger with a stderr sink, so stdout contains only JSON; that convention also holds when parsing fails.
- **The CLI still creates a `QCoreApplication`**: locating `install.conf` and the bundled git depends on `applicationDirPath()`, so the CLI builds a `QCoreApplication` (but no GUI).
- **`--force` is not a force push**: `exec git-push --force` actually uses `--force-with-lease`; `git-reset --mode hard` requires an explicit `--confirm-hard`.
- **`branch-add` copies the default four sections**: `file_manifest` / `pointer_files` / `serverconfig_sync` / `custom_mods`; a missing directory or missing `branch_config` only counts as a warning in `validate`.
- **GUI startup promotes documentation**: if `docs/_editor/<lang>/pack-author` exists (the installer included the editor), it is moved into the regular documentation tree at startup; that is how "section 2 is released by the editor only" works, so do not hand-edit that directory elsewhere.
- **Rebuild everything after changing shared GUI class members**: the editor consumes `GUIWorker`/`HiBerGUILibrary`, and an old `.obj` allocates objects with the old layout, causing `HEAP CORRUPTION` on shutdown (after `--clean-first`, smoke test: start -> wait 3-4 seconds -> close -> exit 0 with no new `crash-report`).
- **Both CLI and GUI are covered by the crash backstop**: in CLI mode `SetCrashCliMode` makes `CrashTracker --cli` print to the current terminal; the report directory is `crash-report/` next to the exe.
- **`--text` only changes presentation, not the contract**: scripts should use the default JSON mode; `--text` is for humans troubleshooting.

## Related

- [README.md](./README.md)
- [Repository editor (GUI)](../../../pack-author/editor-gui.md)
- [Editor CLI](../../../pack-author/editor-cli.md)
- [ADR 0008 - Editor CLI](../../adr/0008-editor-cli.md)
- [Repository structure](../../../pack-author/repo-structure.md)
