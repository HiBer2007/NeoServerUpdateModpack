---
title: NSUM Exporter
description: NeoExporter_NSUM generates manifest-only .nsumpack (pack_format 3) files.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/extensions/exporters/nsum.md
---

# NSUM Exporter

`NeoExporter_NSUM` produces `.nsumpack`: **manifest-only by default**; files are synced from the repository. Only explicitly declared `embed_patterns` are placed in `overrides/`.

## Metadata

| Item | Value |
|---|---|
| format | `nsum` |
| extension | `.nsumpack` |
| required | `name`, `version` |
| optional | `author`, `description`, `embed_patterns` (advanced, multiline) |

## Layout

```text
<pack>.nsumpack
├─ nsum.index.json          # manifest (~1 KB)
└─ overrides/**             # only with embed_patterns matches
```

`nsum.index.json` (`pack_format: 3`) fields:

| Field | Description |
|---|---|
| `pack_format` / `schema` | `3` (file structure version, since 1.0.6) |
| `type` | `nsum` |
| `name` / `version` / `author` / `description` | Pack metadata |
| `gameVersion` / `modloader` / `modloaderVersion` | Game and loader |
| `repository.url` | Primary repository URL (legacy) |
| `repository.urls` | SSH/HTTPS/HTTP URL list (since 1.0.6) |
| `repository.default_branch` | Default Git branch |
| `repository.branch` | Modpack branch |
| `repository.git_branch` | Git branch used to build |
| `repository.commit` | Build commit |
| `files[]` | `path`/`size`/`sha256`/`commit`/`git_branch` (metadata only) |
| `embedded[]` | Embedded files + `pattern` + `sha256` + source commit |
| `embed_patterns[]` | Declared regexes |
| `runtime.minVersion` | Minimum Runtime version |

## Embed rules

- Regexes match repository-relative paths (ECMAScript `search`);
- Separate multiple regexes with newlines or `;`;
- `save/` is never embedded;
- Invalid regex fails the export.

```powershell
# Default: manifest only
NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main `
    --format nsum --export plain.nsumpack

# Explicitly embed config JSON
NeoServerUpdateModpack.exe exec build --repo <repository> --modpack base --git-branch main `
    --format nsum --embed-patterns '^config/.*\.json$' --export embed.nsumpack
```

## Export sequence

```mermaid
sequenceDiagram
    participant B as BuildEngine
    participant E as NeoExporter_NSUM
    participant G as Git
    B->>E: export(manifest, buildDir, extra)
    E->>G: rev-parse HEAD / branch
    E->>E: collectFileIndex(files + commit)
    alt no embed_patterns
        E->>E: write nsum.index.json only
    else matches
        E->>E: write overrides/** + embedded[]
    end
    E-->>B: output path
```

## Client behavior

- bridge reads `repository` and `embedded[]`;
- Embedded files are cached by SHA-256 in `.hmcl/nsum/cache/files/`;
- Legacy packs (no `embedded`) extract all `overrides/`.

## Related

- [`.nsumpack` format](../../pack-author/nsumpack.md)
- [Build and export](../../pack-author/build-and-export.md)
