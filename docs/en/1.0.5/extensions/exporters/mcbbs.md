---
title: MCBBS Exporter
description: NeoExporter_MCBBS generates MCBBS / PCL / HMCL universal modpack zips.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/extensions/exporters/mcbbs.md
---

# MCBBS Exporter

Generates a `manifest.json` + `mcbbs.packmeta` + `overrides/` zip importable by MCBBS, PCL and HMCL.

## Metadata

| Item | Value |
|---|---|
| format | `mcbbs` |
| extension | `.zip` |
| required fields | `name`, `version` |
| optional fields | `author`, `description` |

## Layout

```text
<pack>.zip
├─ manifest.json
├─ mcbbs.packmeta
└─ overrides/
```

## Usage

```powershell
NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main `
    --format mcbbs --export out.zip
```

## Notes

- Contains all files (large); good for forums/cloud drives;
- Excludes the game and loader;
- Updates require re-downloading the whole pack (unlike `.nsumpack` increments).

## Related

- [Exporters](./index.md)
- [Build and export](../../pack-author/build-and-export.md)
