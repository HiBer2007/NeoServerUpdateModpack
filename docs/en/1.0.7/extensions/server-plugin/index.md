---
title: Server Plugin
description: NSUM's MCDR plugin: server core install, config rewrite, repository sync and launch.
lang: en
status: complete
2026-11-28
translation_of: /zh/1.0.7/extensions/server-plugin/index.md
---

# Server Plugin

The NSUM server side is driven by an MCDR plugin (separate repository): it installs the core, syncs repository content and launches the server.

- Repository: [HiBer2007/nsum-bridge-mcdr](https://github.com/HiBer2007/nsum-bridge-mcdr)
- Current version: **0.4.0**
- Requires NSUM Runtime **>= 1.0.7** (older runtimes still sync, but without fastest selection/timeout fallback)

## Capabilities

| Capability | Description |
|---|---|
| Core install | vanilla / plugin / modded / hybrid / proxy / geyser |
| Config rewrite | Update MCDR `config.yml` handler per core |
| Repository sync | Run the Runtime sync for the `server` branch |
| Multi-address selection | `repo_preference` is forwarded to the Runtime: fastest / timeout fallback / manual |
| Plugin merge | Merge `plugin/<name>/` into `plugins/<name>/` |
| Bundled Git | Resolve bundled Git via `gitpath` and prepend PATH |

## Plugin configuration

```json
{
  "repo": "git@github.com:you/your-server-repo.git",
  "repo_preference": "auto",
  "repo_timeout_seconds": 0
}
```

- `repo_preference`: `auto` (default, fastest) / `ssh` / `https` / `http` / `manual` (only `repo`);
- `repo_timeout_seconds`: per-address probe/clone timeout; `0` uses the Runtime defaults (8s probe, 120s clone).

The file lives in the MCDR plugin data directory (`config/nsum_bridge/config.json`); the CLI sync also accepts `--repo-preference` / `--repo-timeout`.

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
