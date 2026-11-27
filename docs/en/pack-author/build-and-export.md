---
title: Build and Export
description: From repository build to four export formats: pipeline, commands, fields and common failures.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/pack-author/build-and-export.md
---

# Build and Export

## Pipeline

```mermaid
flowchart LR
    A[Choose repo/branch] --> B[fetch + checkout git branch]
    B --> C[Merge parent branches]
    C --> D[Scan files/manifest]
    D --> E[Resolve/download pointers]
    E --> F[Sync policy + L3]
    F --> G[Export format]
```

## Formats

| Format | Extension | Size | Use |
|---|---|---|---|
| `nsum` | `.nsumpack` | ~1 KB (default) | Server players, HMCL + bridge |
| `mcbbs` | `.zip` | Full files | Forums/cloud drives/generic launchers |
| `modrinth` | `.mrpack` | Full files | Publishing to Modrinth |
| `hmcl` | directory | Full files | Local instance sync |

## Commands

```powershell
# Preview (no writes)
NeoServerUpdateModpack.exe info preview --repo <repository> --modpack base --format nsum --json

# Build and export
NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main `
    --format nsum --export out.nsumpack

# Explicitly embed files
NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main `
    --format nsum --embed-patterns '^config/.*\.json$' --export out.nsumpack

# Export only (reuse an existing build)
NeoServerUpdateModpack.exe exec export --export out.nsumpack --format nsum --repo <repository> --modpack base
```

GUI: choose repository → branch → modpack → export type → directory → extra info → checklist → build.

## Parameters

| Parameter | Description |
|---|---|
| `--repo` | Repository URL or local path |
| `--modpack` | Modpack branch (workspace.json branches) |
| `--git-branch` | Git branch to check out (defaults to the modpack branch) |
| `--format` | `nsum`/`mcbbs`/`modrinth`/`hmcl` |
| `--export` | Output file or directory |
| `--embed-patterns` | nsum only: embed regexes (repeatable/newline/`;`) |
| `--json` | JSON output |

## Export fields

| Format | Required | Optional |
|---|---|---|
| nsum | name, version | author, description, embed_patterns |
| mcbbs | name, version | author, description |
| modrinth | name, version | summary, author, description |
| hmcl | — | — |

## Common failures

| Symptom | Cause | Fix |
|---|---|---|
| `pathspec ... did not match` | Git branch missing/confused | Pass `--git-branch main` |
| `No resolver found` | Pointer resolver missing | Check `pointers/` |
| Empty/incomplete output | Wrong branch/broken parent chain | Compare with `info preview` |
| Huge package | Accidentally embedded everything | Remove/narrow `--embed-patterns` |

## Related

- [`.nsumpack` format](./nsumpack.md)
- [Pointers](./pointers.md)
