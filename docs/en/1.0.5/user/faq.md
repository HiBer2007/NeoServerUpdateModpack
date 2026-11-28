---
title: FAQ
description: Quick answers about NSUM installation, importing, syncing, launching and updates.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/user/faq.md
---

# FAQ

## Installation

**Q: Where is the NSUM page?**
A: After nsum-bridge is installed, the NSUM category and page appear in HMCL's left sidebar.

**Q: Do I need the Editor?**
A: No. The Editor is for pack authors; if the installer does not tick it, the author docs and editor are not released.

**Q: Do I need to install Git myself?**
A: No. Runtime bundles Git (MinGit); `exec git-update` installs or updates it.

## Import and branches

**Q: Why is the `.nsumpack` so small?**
A: It is a manifest-only pack: by default it contains repository info and a file index; files are synced by Runtime. Files are embedded only when explicitly declared.

**Q: It asks for two branches on launch; which one?**
A: The Git branch is the code branch (e.g. `main`); the modpack branch is the content branch (e.g. `base`/`client`). Follow the server instructions; you can change it later.

**Q: Can I install several NSUM packs?**
A: Yes. Each instance is independent and shares `.hmcl/nsum/repos` and caches.

## Sync

**Q: The first sync is slow.**
A: It downloads all mods; later syncs are incremental. Check your network/proxy.

**Q: What happens when sync fails?**
A: Launch aborts by default with an error; logs are in `.hmcl/nsum/logs/`.

## Launch

**Q: Sync succeeded but the game crashes.**
A: Check the Java version and that game/loader versions match the server; inspect HMCL logs and `crash-reports`.

**Q: What is MCLHR?**
A: A mod-store projector that hard-links shared mods into the instance `mods/` after sync.

## Updates

**Q: Does updating Runtime lose my instance?**
A: No. Runtime lives in `.hmcl/nsum/runtime/`; instance content is separate.

**Q: Bundled Git update fails.**
A: Check the network and retry with `exec git-update`.

## FLFHMCL compatibility

> **FLFHMCL docs**: compatibility, launch failures and log diagnostics: [FLFHMCL FAQ](https://github.com/HiBer2007/FLFHMCL/blob/docs/bilingual/docs/en/users/flfhmcl/faq.mdx) and [diagnostics](https://github.com/HiBer2007/FLFHMCL/blob/docs/bilingual/docs/en/users/flfhmcl/diagnostics.mdx).

**Q: Which HMCL version is required?**
A: bridge 0.4.x targets HMCL 3.17.2 / FLF API 2.x; older bridges may target lower versions, see release notes.

## Other

**Q: Where are the logs?**
A: Runtime logs in its working directory (`builder.log`); bridge sync logs in `.hmcl/nsum/logs/`; HMCL logs in HMCL's own log directory.

**Q: How to report a problem?**
A: Include the Runtime version (`info version`), HMCL version, sync log and a screenshot of the error.
