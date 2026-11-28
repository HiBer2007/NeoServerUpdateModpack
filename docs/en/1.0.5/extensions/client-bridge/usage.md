---
title: Bridge Usage
description: Install nsum-bridge, use the NSUM page, import .nsumpack and choose branches.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/extensions/client-bridge/usage.md
---

# Bridge Usage

## Install

1. Ensure HMCL and FLF runtime are installed;
2. Put `nsum-bridge-<version>.jar` into HMCL `mods/`;
3. Start HMCL; the NSUM category appears.

## NSUM page

| Action | Description |
|---|---|
| Install/Update Runtime | Download `NSUM_Runtime_CLI_<version>.zip` from metadata, verify and extract into `.hmcl/nsum/runtime/` |
| Install from downloads | Use a zip placed in `.hmcl/nsum/downloads/` (offline/manual) |
| Open repository | Open the instance repository in the editor |
| Version info | Show bridge and Runtime versions |

## Import .nsumpack

1. HMCL "Install modpack" or drag in `.nsumpack`;
2. The type is recognized as **NSUM modpack**;
3. Choose the instance directory and confirm;
4. bridge writes `<instance>/.NSUM/instance.json` (repo, branches, versions, `packFormat`, `sourceCommit`);
5. If the pack has `embedded[]`, files are installed via the SHA-256 cache.

## Branch selection

On first launch, choose inside the task dialog:

1. Git branch;
2. Modpack branch;
3. Confirm.

The choice is stored; you can change it in instance settings.

## Instance marker

```json
{
  "repo": "https://github.com/owner/repo.git",
  "branch": "client",
  "gitBranch": "main",
  "packFormat": 2,
  "sourceCommit": "7ee64d8...",
  "sourceGitBranch": "main",
  "defaultBranch": "main",
  "branchType": "client",
  "launchOnSyncFailure": false
}
```

## Notes

- Removing the bridge jar removes the NSUM page but keeps instances and repository caches;
- Do not edit the repository URL in `.NSUM/instance.json` manually;
- Runtime updates do not affect instance content.

## Related

- [Task chain](./tasks.md)
- [Downloads](./download.md)
