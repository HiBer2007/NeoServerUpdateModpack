---
title: Updates
description: Version checks and updates for NSUM Runtime, bridge and related components, plus rollback.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/user/updates.md
---

# Updates

NSUM versions are published on the metadata site (`https://metadata.hibernet.top`). bridge and Runtime check independently.

## Components

| Component | Version source | Update method |
|---|---|---|
| NSUM Runtime | `Latest` in metadata `NSUM.json` | "Install / Update Runtime" on the HMCL NSUM page |
| nsum-bridge | `Latest` in metadata `nsum-bridge.json` | Replace the jar under `mods/` |
| Bundled Git | Latest GitHub MinGit | `exec git-update` or Runtime background update |

## Runtime updates

1. Open HMCL → NSUM page;
2. If a newer version exists, the page offers an update;
3. bridge downloads the zip (Range multi-threaded, falling back to single stream), verifies SHA-256, and extracts into `.hmcl/nsum/runtime/`;
4. The page shows the new version.

### Manual update

```powershell
# Current version
.\NeoServerUpdateModpack.exe info version

# Update bundled Git
.\NeoServerUpdateModpack.exe exec git-update
```

If automatic update fails, download `NSUM_Runtime_CLI_<version>.zip` manually, place it into `.hmcl/nsum/downloads/`, then click "Install from downloads" on the NSUM page.

## bridge updates

1. Download `nsum-bridge-<version>.jar` from the metadata site or GitHub Releases;
2. Close HMCL;
3. Remove the old `nsum-bridge-*.jar` from `mods/` and add the new one;
4. Start HMCL; the NSUM page shows the bridge version.

> bridge versions have compatibility requirements with FLF API/HMCL; see the release notes.

## Rollback

| Scenario | Action |
|---|---|
| Runtime regression | Keep the old zip, delete `.hmcl/nsum/runtime/`, reinstall the old version |
| bridge incompatible | Restore the previous jar |
| Repository content corrupted | Restore from Git/backup, or resync the branch |

## FLFHMCL updates

> **FLFHMCL docs**: Agent/HMCL/FLF API version alignment and update steps: [FLFHMCL settings and updates](https://github.com/HiBer2007/FLFHMCL/blob/docs/bilingual/docs/en/users/flfhmcl/settings-updates.mdx).

## Version numbers

NSUM uses two version numbers:

- External `x.y.z`: release version (e.g. `1.0.5`);
- Internal `a.b.c`: iteration version (e.g. `0.1.11`);
- Full version: `1.0.5.0.1.11`.

```powershell
.\NeoServerUpdateModpack.exe info version
```
