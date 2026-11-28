---
title: NSUM Docs
description: Official NSUM (NeoServerUpdateModpack) documentation: launcher usage, pack authoring, development and extensions.
lang: en
status: complete
updated: 2026-11-27
---

# NSUM Docs

NSUM (NeoServerUpdateModpack) is a **Git-repository based build and auto-update** toolchain for Minecraft server modpacks:
the server maintains a Git repository, and clients sync and launch through HMCL + nsum-bridge + NSUM Runtime.

## Start here

| Who you are | Where to go |
|---|---|
| Player / server member | [User Guide](./user/index.md): install, import a pack, sync and launch |
| Pack author | [Pack Authors](./pack-author/index.md): repository layout, Editor, builds, `.nsumpack` |
| Developer / contributor | [Developers](./developer/index.md): architecture, design goals, buses and extension interfaces, ABI, module reference |
| Looking for an extension | [Extensions](./extensions/index.md): parsers, pointers, exporters, installer, bridge, MCDR, libraries |

> Documentation is available in Chinese and English. English pages mirror the Chinese structure one-to-one.

## Version

These docs target **NSUM Runtime 1.0.5 / internal 0.1.11**. Page-level version notes are added where behavior differs.

## Related links

- Main repository: [HiBer2007/NeoServerUpdateModpack](https://github.com/HiBer2007/NeoServerUpdateModpack)
- bridge repository: [HiBer2007/nsum-bridge](https://github.com/HiBer2007/nsum-bridge)
- Metadata site: [metadata.hibernet.top](https://metadata.hibernet.top)
