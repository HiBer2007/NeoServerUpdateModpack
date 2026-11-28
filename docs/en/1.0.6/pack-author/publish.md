---
title: Publishing
description: Pack-author publishing checklist: build, self-check, distribution and client compatibility.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/pack-author/publish.md
---

# Publishing

Authors ship the **modpack recipe** (repository + `.nsumpack`), not the game itself. The operations/version release flow is covered by the online developer docs.

## Checklist

1. **Freeze content**: merge all changes, clean `git status`;
2. **Validate**:

```powershell
NeoWorkspaceEditor.exe exec validate --repo <repository>
```

3. **Preview**:

```powershell
NeoServerUpdateModpack.exe info preview --repo <repository> --modpack base --format nsum --json
```

4. **Version**: update `export.metadata.version` in `workspace.json` (or fill it in the wizard);
5. **Export**:

```powershell
NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main `
    --format nsum --export release.nsumpack
```

6. **Self-check**:

| Check | Expected |
|---|---|
| Package size | ~1 KB without embedding |
| `repository.commit` | Current release commit |
| `files[]` count | Matches preview |
| `embedded[]` | Only intended files |

7. **Distribute**: Release/cloud drive/server announcement with repository and branch notes.

## Versioning

- Content updates → bump the pack `version`;
- Breaking changes (branches/loader/layout) → major/minor and notify players;
- Keep the `.nsumpack` and matching Git tag for every release.

## Client compatibility

| Item | Requirement |
|---|---|
| Runtime | `runtime.minVersion` |
| bridge | Compatible pack format (pack_format 2 needs ≥0.4.4.17) |
| HMCL/FLF | Per bridge release notes |
| Java | Per loader/mod requirements |

## Rollback

- Repository: `git revert` or return to the previous tag;
- Players: re-import the previous `.nsumpack` and resync;
- Never delete historical tags.

## Related

- [Build and export](./build-and-export.md)
- [Best practices](./best-practices.md)
