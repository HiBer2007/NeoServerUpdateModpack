---
title: Command Line (CLI)
description: Runtime CLI command categories, JSON output and exit codes for advanced users and automation.
lang: en
status: complete
updated: 2026-11-28
translation_of: /zh/1.0.7/user/cli.md
---

# Command Line (CLI)

> `--repo-preference auto|ssh|https|http|manual` and `--repo-timeout <seconds>` are available since **1.0.7**: auto probes each address and picks the fastest, manual uses only the given URL.

> `repo-remotes`, `git-ssh-key` and multi-URL export are available since **1.0.6**.

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

# Maintain multi-protocol repository URLs (writes git.remotes in workspace.json)
.\NeoServerUpdateModpack.exe exec repo-remotes --repo <repository> `
    --ssh git@github.com:owner/repo.git `
    --https https://github.com/owner/repo.git `
    --http http://github.com/owner/repo.git --primary ssh

# SSH key management (bundled Git is given GIT_SSH_COMMAND automatically)
.\NeoServerUpdateModpack.exe exec git-ssh-key --list
.\NeoServerUpdateModpack.exe exec git-ssh-key --generate --comment nsum
.\NeoServerUpdateModpack.exe exec git-ssh-key --public-key
.\NeoServerUpdateModpack.exe exec git-ssh-key --test --host github.com
.\NeoServerUpdateModpack.exe exec git-ssh-key --configure --host github.com
```

GUI: main program **SSH Key...** next to the repository input; editor **Git → SSH Key...**.

## Address selection

`--repo` for `exec build` / `exec export` may be a single URL. When SSH / HTTPS / HTTP alternatives can be derived from `.nsumpack` `repository.urls` or workspace `git.remotes`, the default `auto` probes latency before cloning:

```powershell
# Default: fastest address, fall back on failure
.\NeoServerUpdateModpack.exe exec export --repo https://github.com/owner/repo.git `
    --modpack base --format nsum --export out.nsumpack --repo-preference auto

# Prefer SSH, fall back to HTTPS/HTTP
.\NeoServerUpdateModpack.exe exec export --repo https://github.com/owner/repo.git `
    --modpack base --format nsum --export out.nsumpack --repo-preference ssh

# Use only the given URL; at most 30 seconds per address
.\NeoServerUpdateModpack.exe exec export --repo https://github.com/owner/repo.git `
    --modpack base --format nsum --export out.nsumpack --repo-preference manual --repo-timeout 30
```

- `--repo-preference`: `auto` (default) / `ssh` / `https` / `http` / `manual`;
- `--repo-timeout <seconds>`: per-address timeout for both probe and clone (defaults: 8s probe, 120s clone);
- Environment variable `NSUM_REPO_PREFERENCE` supplies the default; the command line wins;
- `Repository probe: <url> -> <ms>` in the log shows measured latency; after `Address failed` the next address is tried, and only when all fail does the command exit 1.

>

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
