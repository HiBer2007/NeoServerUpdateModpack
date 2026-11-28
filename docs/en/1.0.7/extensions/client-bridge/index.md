---
title: Client Bridge
description: nsum-bridge (HMCL/FLF extension): NSUM page, modpack type, launch injection and Runtime install.
lang: en
status: complete
updated: 2026-11-28
translation_of: /zh/1.0.7/extensions/client-bridge/index.md
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
| Multi-address selection | Parallel fastest probe, timeout fallback, manual override |
| Embedded cache | `.hmcl/nsum/cache/files/<sha256>` |
| MCLHR | Project the mod store after sync |

## Version and dependencies

| Item | Value |
|---|---|
| Version | `0.4.6.17` (current) |
| Dependencies | HMCL 3.17.2 / FLF API 2.x |
| Java | Same as HMCL runtime (17+) |

## Address selection

Since 0.4.6.17 the bridge probes multiple `repository.urls[]` **in parallel** (`git ls-remote`) and prefers the lowest latency; each address has its own timeout and failure/timeout falls back to the next. Existing checkouts are reused via `git remote set-url`; it never clones one copy per address.

Optional `.hmcl/nsum/repo.properties`:

```properties
preference=auto
probeTimeoutSeconds=8
cloneTimeoutSeconds=180
syncTimeoutSeconds=300
```

- `preference`: `auto` (default, fastest) / `ssh` / `https` / `http` / `manual` (manifest order) / explicit URL;
- On sync timeout the bridge terminates that Runtime process and tries the next address;
- System properties `-Dnsum.repo.preference=...`, `-Dnsum.repo.probeTimeoutSeconds=...` override the file (environment variables such as `NSUM_REPO_PREFERENCE` also work);
- A successful address is stored as `activeRepoUrl` and preferred on the next launch, avoiding repeated probing.

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
| `.hmcl/nsum/repo.properties` | Repository address selection |

## Related

- [Sync and launch](../../user/sync-and-launch.md)
- [Troubleshooting](../../user/troubleshooting.md)
## Related FLF repositories

- [FLF API](https://github.com/HiBer2007/FLFAPI): Fabric-API-style extension layer for HMCL (events/registries/UI/accounts/downloads/modpacks). bridge depends on its API.
- [FLFHMCLauncher](https://github.com/HiBer2007/FLFHMCLauncher): shell/high-level entry for FLFHMCL, usable for installation and environment setup.
