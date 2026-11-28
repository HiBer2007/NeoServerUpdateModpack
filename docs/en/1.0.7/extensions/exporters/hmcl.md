---
title: HMCL Exporter
description: NeoExporter_HMCL syncs builds directly into an HMCL instance directory.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/extensions/exporters/hmcl.md
---

# HMCL Exporter

`NeoExporter_HMCL` writes the build into a target directory (usually an HMCL instance). This is the format used by the runtime sync on clients.

## Metadata

| Item | Value |
|---|---|
| format | `hmcl` |
| extension | none (directory) |
| fields | none |

## Usage

```powershell
NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main `
    --format hmcl --export <instance directory>
```

bridge uses the equivalent internally:

```text
exec build --format hmcl --export <instance> --repo <repository> --modpack <branch> --git-branch <branch>
```

## Behavior

1. Merge branches and produce output;
2. Apply `sync_policies` into the target (mirror deletes extras);
3. Resolve pointers via cache/download;
4. Apply `serverconfig` (L3);
5. Optionally build the MCLHR store and hard-link into `mods/`.

## Protection

`mirror` protects before deletion:

- Root `options.txt`, `log4j2.xml`, `natives*`, root `*.jar`;
- `saves/`;
- `.NSUM/`, `<id>.json`, `icon.*`.

## Notes

- `--export` must be a directory;
- Never point at the HMCL installation directory itself;
- Sync logs report copied/deleted counts.

## Related

- [Exporters](./index.md)
- [Sync and launch](../../user/sync-and-launch.md)
