---
title: Sync Rules
description: The three sync tiers (L1/L2/L3), sync_policies, file markers and serverconfig rules.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/pack-author/sync-rules.md
---

# Sync Rules

Sync decides which repository files enter the instance and how. NSUM has three tiers:

| Tier | Source | Description |
|---|---|---|
| L1 | Base branch (usually `base`) | Shared foundation |
| L2 | Modpack branch (with parent merge) | Branch overrides and final build output |
| L3 | Save-level `serverconfig` | Per-save server config sync |

## Sync policies

```json
{
  "default_folder_policy": "mirror",
  "folders": {
    "mods": "mirror",
    "config": "mirror",
    "resourcepacks": "incremental_add",
    "shaderpacks": "incremental_overwrite",
    "logs": "skip"
  },
  "files": {
    "config/partial.json": { "mode": "partial", "tracked_keys": ["keep", "server"] }
  }
}
```

| Policy | Behavior |
|---|---|
| `mirror` | Repository wins: add/overwrite/delete extras |
| `incremental_add` | Only add; never overwrite/delete |
| `incremental_overwrite` | Add and overwrite; never delete |
| `skip` | Do not sync |
| `partial` (file-level) | Sync only the listed config keys |

> `default_folder_policy` applies to undeclared folders; root-level protections exist (`options.txt`, `log4j2.xml`, `natives*`, root jars).

## File markers

Per-branch manifests can mark individual files:

| Marker | Effect |
|---|---|
| `delete` | Delete the parent file during merge |
| `override` | Force the branch file to overwrite |

## serverconfig (L3)

Repository path:

```text
branches/<branch>/save/[save]/serverconfig/
```

- `save` is a literal folder name and `[save]` a placeholder for any save directory; `serverconfig` is the sync target;
- Save-level rules live in `.rule/globle.json` (default mode) and `.rule/list.json` (file list);
- Runtime writes the matching files into the instance save directory during sync.

```json
// .rule/globle.json
{ "default_mode": "full", "folder_mode": "mirror" }
```

## Custom mod directory

`branch_config/<branch>.json`:

```json
{
  "custom_mods": {
    "enabled": true,
    "path": "D:/shared-mods",
    "merge_order": "post"
  }
}
```

- `merge_order=post`: merge after repository mods (external wins on conflicts);
- Useful for client mods shared by several packs.

## Merge and deletion protection

- `mirror` protects before deleting extras:
  - root-level critical files (`options.txt`, `log4j2.xml`, `natives*`, root `*.jar`);
  - `saves/` save directories;
  - explicit policy paths and instance markers (`.NSUM/`, `<id>.json`, `icon.*`).
- Deletions/overwrites are logged; use the log to recover.

## Validate

```powershell
# Preview build structure (no writes)
NeoServerUpdateModpack.exe info preview --repo <repository> --modpack base --format nsum --json

# Build into an instance
NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main --format hmcl --export <instance>
```
