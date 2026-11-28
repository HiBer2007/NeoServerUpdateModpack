---
title: Quick Start
description: From receiving a .nsumpack to your first successful join.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/user/quick-start.md
---

# Quick Start

Goal: import the server's `.nsumpack`, finish the first sync, and join the game. No manual mod downloads are required.

## Prerequisites

- HMCL, FLFHMCL and nsum-bridge are installed (see [Installation](./installation.md)).

> **FLFHMCL docs**: installation steps, system requirements and the HMCL compatibility matrix: [FLFHMCL installation guide](https://github.com/HiBer2007/FLFHMCL/blob/docs/bilingual/docs/en/users/flfhmcl/install.mdx).

- Runtime is installed (HMCL → NSUM page shows a version).

## Step 1: import the pack

1. Open HMCL → "Install modpack" / drag in the `.nsumpack`;
2. The type is recognized as **NSUM modpack**;
3. Choose the install location (the instance name is editable) and confirm.

bridge will:

- create the instance directory;
- write `<instance>/.NSUM/instance.json` (repository URL, modpack branch, Git branch, game/loader versions);
- if the pack contains explicitly embedded files, install them from `embedded[]` and store them in the SHA-256 cache.

## Step 2: choose branches (first launch/first sync)

When the instance starts, bridge asks inside the **task dialog**:

1. **Git branch** (code branch, e.g. `main`);
2. **Modpack branch** (content branch, e.g. `base` or `client`);
3. Confirm.

A step is skipped automatically when only one candidate exists. The choice is stored in the instance marker and reused on later launches; you can change it in the instance settings.

## Step 3: first sync

Runtime then performs:

```text
clone/update repo → checkout branch → merge modpack branch → resolve pointers/download → sync configs/mods → done
```

The first sync downloads all mods, so it takes longer; progress is shown in the HMCL task dialog.

## Step 4: launch

After a successful sync:

- the matching game version and loader are installed/switched automatically (NeoForge/Fabric, etc.);
- HMCL continues its normal launch flow and the game starts.

Later launches only sync differences.

## Verify

| Check | Expected |
|---|---|
| `<instance>/.NSUM/instance.json` exists | Yes |
| `.hmcl/nsum/logs/` has a sync log | Yes |
| Game/loader match the server | Yes |
| Mod count matches the server | Yes |

## If something goes wrong

- Sync failed: read the latest log in `.hmcl/nsum/logs/` and see [Troubleshooting](./troubleshooting.md);
- Wrong branch: restart the instance and change the selection;
- Game crashes: make sure the sync succeeded, then check mod dependencies/Java version.
