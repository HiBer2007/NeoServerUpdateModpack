---
title: User Guide
description: Install NSUM Runtime, import a modpack, sync and launch, update and troubleshoot.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/user/index.md
---

# User Guide

This section is for **players and server members**: install NSUM Runtime, import the server-provided modpack into HMCL, and each launch syncs automatically.

## Three pieces you will use

| Component | Role | How to get it |
|---|---|---|
| HMCL + FLFHMCL | Launcher and Fabric runtime injection | See FLFHMCL docs (TBD) |
| nsum-bridge | NSUM modpack type, launch strategy and NSUM page for HMCL | Provided by the server or the metadata site |
| NSUM Runtime | CLI/runtime that syncs the repository and checks before launch | Installed automatically by bridge, or via the installer |

## Suggested reading order

1. [Installation](./installation.md): install Runtime and bridge;
2. [Quick start](./quick-start.md): import a pack and join the game;
3. [Launcher UI](./launcher-guide.md): wizard, build page and common actions;
4. [Sync and launch](./sync-and-launch.md): what happens on every launch;
5. [Updates](./updates.md), [FAQ](./faq.md), [Troubleshooting](./troubleshooting.md).

## Key concepts (one line each)

- **Repository**: a Git repository maintained by the server; it stores the modpack recipe.
- **Modpack branch**: branches such as `base` or `client` decide which files you receive.
- **Sync**: Runtime updates your game instance from the repository.
- **`.nsumpack`**: the pack file the server distributes; by default it only contains repository info and a file index, so it is tiny.
