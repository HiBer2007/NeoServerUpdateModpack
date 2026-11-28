---
title: Modrinth Exporter
description: NeoExporter_Modrinth generates .mrpack (Modrinth Modpack Format).
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/extensions/exporters/modrinth.md
---

# Modrinth Exporter

Generates `.mrpack`: `modrinth.index.json` + `overrides/` (optional `client-overrides/`, `server-overrides/`).

## Metadata

| Item | Value |
|---|---|
| format | `modrinth` |
| extension | `.mrpack` |
| required fields | `name`, `version` |
| optional fields | `summary`, `author`, `description` |

## Layout

```text
<pack>.mrpack
├─ modrinth.index.json
├─ overrides/
├─ client-overrides/
└─ server-overrides/
```

`modrinth.index.json` fields:

| Field | Description |
|---|---|
| `formatVersion` | 1 |
| `game` | `minecraft` |
| `versionId` | From `version` |
| `name` | Pack name |
| `files[]` | `path`, `hashes`, `downloads`, `env` |
| `dependencies` | Minecraft/loader versions |

## Usage

```powershell
NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main `
    --format modrinth --export out.mrpack
```

## Notes

- Pointers become `files[].downloads`;
- Unresolved files go into `overrides/`;
- Check `env` and dependency versions before publishing.

## Related

- [Exporters](./index.md)
- [Build and export](../../pack-author/build-and-export.md)
