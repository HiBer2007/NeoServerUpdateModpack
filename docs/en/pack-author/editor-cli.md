---
title: Editor CLI
description: The headless NeoWorkspaceEditor CLI for branches, files, JSON, pointers and Git, used by AI and scripts.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/pack-author/editor-cli.md
---

# Editor CLI

When the first argument of `NeoWorkspaceEditor.exe` is `info`, `exec` or `help`, it enters a headless CLI: no window is created, stdout contains a single JSON object, and logs go to stderr.

## General rules

```powershell
NeoWorkspaceEditor.exe <info|exec|help> <subcommand> [--repo <dir>] [options]
```

| Rule | Description |
|---|---|
| `--repo` | Repository root; defaults to the current directory; must contain `workspace.json` |
| Output | Success `{"ok":true,"command":"...","repo":"...","data":{...}}`; failure `{"ok":false,"command":"...","error":"..."}` |
| Exit code | 0 success / 1 failure / 2 invalid arguments |
| `--text` | Human-readable output |
| Paths | Repository-relative only; absolute paths and `..` are rejected |
| Git | Built-in commands; failures still return `data.exit_code/stdout/stderr` |

## info (read-only)

```powershell
NeoWorkspaceEditor.exe info workspace --repo <repository>
NeoWorkspaceEditor.exe info branches --repo <repository>
NeoWorkspaceEditor.exe info branch <name> --repo <repository>
NeoWorkspaceEditor.exe info tree --path branches/base --depth 2 --repo <repository>
NeoWorkspaceEditor.exe info pointers --repo <repository>
NeoWorkspaceEditor.exe info sync --repo <repository>
NeoWorkspaceEditor.exe info file branches/base/config/x.json --repo <repository>
```

## exec (mutations)

### Branches

```powershell
# Create; parent optional; server branches require --core
NeoWorkspaceEditor.exe exec branch-add --name client --parent base --type client --repo <repository>
NeoWorkspaceEditor.exe exec branch-add --name server --type server --core paper --repo <repository>

# Update; --parent "" makes it a root branch; --hidden true|false
NeoWorkspaceEditor.exe exec branch-set --name client --parent base --hidden false --repo <repository>

# Rename (moves branches/ and branch_config/, updates child parents)
NeoWorkspaceEditor.exe exec branch-rename --name client --to client-lite --repo <repository>

# Delete (--force reparents children; --purge deletes content)
NeoWorkspaceEditor.exe exec branch-remove --name client-lite --purge --repo <repository>
```

### Files and JSON

```powershell
NeoWorkspaceEditor.exe exec file-write --path branches/base/config/x.json --content "{""a"":1}" --repo <repository>
NeoWorkspaceEditor.exe exec file-write --path mods/a.jar --from-file D:\a.jar --repo <repository>
NeoWorkspaceEditor.exe exec file-move --from branches/base/a.json --to branches/base/b.json --repo <repository>
NeoWorkspaceEditor.exe exec file-delete --path branches/base/old.json --repo <repository>

NeoWorkspaceEditor.exe exec json-get --file workspace.json --pointer /workspace/minecraft --repo <repository>
NeoWorkspaceEditor.exe exec json-set --file workspace.json --pointer /sync_policies/folders/mods --value "\"mirror\"" --repo <repository>
NeoWorkspaceEditor.exe exec json-set --file workspace.json --pointer /obsolete --delete --repo <repository>
```

`--value` is parsed as JSON first, falling back to string; `--value-string` forces a string; pointers support `/arrayIndex` and `~0`/`~1` escapes.

### Pointers

```powershell
NeoWorkspaceEditor.exe exec pointer-add --path mods/example.jar --sha256 <64-hex> `
    --resolver direct_url --url https://example.com/example.jar --branch base --repo <repository>

NeoWorkspaceEditor.exe exec pointer-remove --path mods/example.jar --branch base --repo <repository>
```

Writes `branch_config/<sha>.pointer` and updates `file_manifest`/`pointer_files` in `branch_config/<branch>.json`.

### Git

```powershell
NeoWorkspaceEditor.exe exec git-status --repo <repository>
NeoWorkspaceEditor.exe exec git-add --all --repo <repository>
NeoWorkspaceEditor.exe exec git-commit --message "feat: ..." --repo <repository>
NeoWorkspaceEditor.exe exec git-push --set-upstream --repo <repository>
NeoWorkspaceEditor.exe exec git-log --limit 10 --repo <repository>
NeoWorkspaceEditor.exe exec git-diff --stat --repo <repository>
NeoWorkspaceEditor.exe exec git-checkout --branch client --create --repo <repository>
NeoWorkspaceEditor.exe exec git-reset --mode mixed --ref HEAD --repo <repository>
NeoWorkspaceEditor.exe exec git-config --key user.name --repo <repository>
```

Safety boundaries:

- push offers `--force-with-lease` only, never plain force;
- `git-reset --mode hard` requires `--confirm-hard`;
- `git-commit` requires `--message`;
- credential prompts never block (`GIT_TERMINAL_PROMPT=0`).

## AI workflow

```powershell
# 1. Read
NeoWorkspaceEditor.exe info branches --repo K:\repo

# 2. Modify
NeoWorkspaceEditor.exe exec branch-add --name client --parent base --type client --repo K:\repo
NeoWorkspaceEditor.exe exec file-write --path branches/client/config/client.json --content "{}" --repo K:\repo

# 3. Validate
NeoWorkspaceEditor.exe exec validate --repo K:\repo

# 4. Commit and push
NeoWorkspaceEditor.exe exec git-add --all --repo K:\repo
NeoWorkspaceEditor.exe exec git-commit --message "feat: add client branch" --repo K:\repo
NeoWorkspaceEditor.exe exec git-push --repo K:\repo
```

## validate

```powershell
NeoWorkspaceEditor.exe exec validate --repo <repository>
```

- Errors: duplicate branch names, missing parent, inheritance cycle;
- Warnings: missing `branches/<name>/` or `branch_config/<name>.json`.

## Help

```powershell
NeoWorkspaceEditor.exe help
```
