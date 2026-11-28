---
title: Server Plugin
description: NSUM's MCDR plugin: server core install, config rewrite, repository sync and launch.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/extensions/server-plugin/index.md
---

# Server Plugin

The NSUM server side is driven by an MCDR plugin (separate repository): it installs the core, syncs repository content and launches the server.

<!-- slot:MCDR_PLUGIN_REPO -->

> **TBD**: repository/release links will be added when available.

## Capabilities

| Capability | Description |
|---|---|
| Core install | vanilla / plugin / modded / hybrid / proxy / geyser |
| Config rewrite | Update MCDR `config.yml` handler per core |
| Repository sync | Run the Runtime sync for the `server` branch |
| Plugin merge | Merge `plugin/<name>/` into `plugins/<name>/` |
| Bundled Git | Resolve bundled Git via `gitpath` and prepend PATH |

## Core categories

| Category | Examples |
|---|---|
| vanilla | vanilla |
| plugin | paper / purpur / folia |
| modded | forge / neoforge / fabric / quilt |
| hybrid | mohist / arclight / cardboard |
| proxy | velocity / bungeecord / waterfall |
| geyser | geyser |

> Only branches with `branchType=server` may install a plugin-server core.

## Flow

```mermaid
flowchart LR
    A[Read instance/branch config] --> B[Resolve core]
    B --> C[Install/update core]
    C --> D[Sync server branch]
    D --> E[Rewrite config.yml]
    E --> F[Launch server]
```

## Pages

- [Install](./install.md): core selection and installation;
- [Sync](./sync.md): server sync flow;
- [Config](./config.md): config.yml rewrite and plugin directories.

## Related

- [Client bridge](../client-bridge/index.md)
- [Sync and launch](../../user/sync-and-launch.md)
