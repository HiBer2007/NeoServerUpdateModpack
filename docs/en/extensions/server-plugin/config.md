---
title: Server Config
description: MCDR config.yml handler rewrite and server launch parameters.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/extensions/server-plugin/config.md
---

# Server Config

## Handler mapping

MCDR's `config.yml` must set `handler` per core:

| Core type | handler |
|---|---|
| paper / spigot / purpur / folia | `bukkit` |
| fabric / quilt | `fabric` |
| forge / neoforge | `forge` |
| velocity | `velocity` |
| bungeecord / waterfall | `bungeecord` |

The plugin rewrites this field when installing/switching cores.

## Other keys

| Key | Description |
|---|---|
| `working_directory` | Server directory |
| `start_command` | Launch command (`java -jar server.jar` etc.) |
| `handler` | See above |
| `rcon` | Enable and set a password if needed |

## Suggested launch flags

```text
-Xms4G -Xmx4G -XX:+UseG1GC -jar server.jar nogui
```

- Tune memory to player/mod count;
- Quote paths containing spaces;
- Match the Java version to Minecraft/core requirements.

## Troubleshooting

| Symptom | Check |
|---|---|
| MCDR exits immediately | `start_command` and `working_directory` |
| Plugins not loaded | `plugins/` path and plugin.yml |
| Wrong core detected | `handler` match |

## Related

- [Server plugin](./index.md)
- [Install](./install.md)
