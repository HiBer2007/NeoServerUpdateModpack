---
title: Releases
description: NSUM version highlights and release links.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/releases/index.md
---

# Releases

Full release history: [GitHub Releases](https://github.com/HiBer2007/NeoServerUpdateModpack/releases).

## 1.0.5 (2026-11-27)

- Headless editor CLI: `info` / `exec` / `help` with built-in Git commands for AI/scripts;
- `.nsumpack` became manifest-only `pack_format 2`: `nsum.index.json` by default, embedded files require explicit `--embed-patterns` and record their source commit;
- bridge `0.4.4.17`: installs `embedded[]` files and caches them by SHA-256;
- Documentation rebuilt (four sections, bilingual, Nextra-ready).

## 1.0.4 (2026-11-26)

- Multi-threaded Range downloads for runtime and HIC dependencies (single-stream fallback);
- bridge parallel downloader.

## 1.0.3

- Fixed non-ASCII path crashes (UTF-8 argv/path end-to-end);
- Improved bundled Git detection and auto-update.

## Older releases

See GitHub Releases and the corresponding tags.

## Related

- [Updates](../user/updates.md)
- [Release process](../developer/release.md)
