---
title: Server Installation
description: MCDR plugin core selection, download and installation (including BuildTools/API fallbacks).
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/extensions/server-plugin/install.md
---

# Server Installation

## Prerequisites

- MCDR and Python installed;
- Bundled Git (four layouts) or system Git reachable;
- Writable server directory.

## Core selection

| Core | Install method |
|---|---|
| Spigot | `BuildTools.jar --rev <mc> --compile spigot` |
| Pufferfish | Jenkins artifact `ci.pufferfish.host/job/Pufferfish-<mc>` |
| Mohist | `mohistmc.com/api/v2` (falls back to GitHub) |
| Arclight / Cardboard | GitHub Releases asset matching |
| Paper / Purpur / Folia | Official API/download |
| Fabric / Quilt / NeoForge | Installer/official API |
| Velocity / BungeeCord | Official download |
| Geyser | Official download |

## Install flow

1. Validate the core and `branchType=server`;
2. Download the core jar;
3. Copy to `server.jar` (or the configured launch jar);
4. Write/update MCDR `config.yml`;
5. First launch generates configuration files.

## BuildTools notes

- Requires JDK 21 and bundled Git on PATH;
- Windows certificate chains: `-Djavax.net.ssl.trustStoreType=WINDOWS-ROOT`;
- Cache `<server>/.nsum/cache` (~876 MB) must not be committed;
- Compilation takes ~5 minutes.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `unknown core` | Typo/unsupported version | Check the core list |
| BuildTools hangs | Network/certs/Java version | Set trustStoreType, check JDK |
| 404 | No asset for the version | Change version or pre-place the cache |

## Related

- [Server plugin](./index.md)
- [Config](./config.md)
