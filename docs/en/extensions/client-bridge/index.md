---
title: Client Bridge
description: nsum-bridge (HMCL/FLF extension): NSUM page, modpack type, launch injection and Runtime install.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/extensions/client-bridge/index.md
---

# Client Bridge

`nsum-bridge` is an HMCL extension (FLF API dependent) that integrates NSUM into the launcher: it recognizes `.nsumpack`, runs the Runtime sync before launch, and provides the NSUM page.

## Capabilities

| Capability | Description |
|---|---|
| NSUM page | Install/update Runtime, open repository, version info |
| Modpack type | Recognize `.nsumpack` and create instances |
| Launch injection | Insert `NSumSyncTask` into the HMCL launch flow |
| Branch selection | Three-step selection (embedded dialog) |
| Runtime install | Download/verify from metadata |
| Parallel downloads | Range chunks + single-stream fallback |
| Embedded cache | `.hmcl/nsum/cache/files/<sha256>` |
| MCLHR | Project the mod store after sync |

## Version and dependencies

| Item | Value |
|---|---|
| Version | `0.4.4.17` (current) |
| Dependencies | HMCL 3.17.2 / FLF API 2.x |
| Java | Same as HMCL runtime (17+) |

## Pages

- [Usage](./usage.md): install, NSUM page, importing packs;
- [Task chain](./tasks.md): launch injection and sync task;
- [Downloads](./download.md): parallel Range downloads, fallback and config.

## Data directories

| Path | Content |
|---|---|
| `.hmcl/nsum/runtime/` | Runtime deployment |
| `.hmcl/nsum/downloads/` | Runtime package cache |
| `.hmcl/nsum/repos/` | Repository clones |
| `.hmcl/nsum/logs/` | Sync logs |
| `.hmcl/nsum/cache/files/` | Embedded file cache |
| `.hmcl/nsum/download.properties` | Downloader config |

## Related

- [Sync and launch](../../user/sync-and-launch.md)
- [Troubleshooting](../../user/troubleshooting.md)
