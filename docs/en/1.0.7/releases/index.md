---
title: Releases
description: NSUM version highlights and release links.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/releases/index.md
---

# Releases

Full release history: [GitHub Releases](https://github.com/HiBer2007/NeoServerUpdateModpack/releases).

## 1.0.7 (2026-11-28)

- **Automatic fastest address**: with multiple URLs the runtime CLI probes each address and the bridge probes in parallel and prefer the lowest-latency one;
- **Timeout fallback**: per-address timeouts (8s probe / 120s clone / 300s sync) automatically try the next address;
- **Manual selection**: `--repo-preference auto|ssh|https|http|manual` (runtime), `.hmcl/nsum/repo.properties` `preference=auto|ssh|https|http|manual|<URL>` (bridge), editor primary selector and explicit URL input;
- The main program repository page adds an **Address priority** selector (auto fastest / prefer SSH / HTTPS / HTTP); a clone attempt times out after 60s and falls back.

## 1.0.6 (2026-11-28)

- **Multiple repository URLs**: `workspace.json` is now `version: 1.1` with `git.remotes[]` (SSH/HTTPS/HTTP, primary first); `.nsumpack` moves to `pack_format` / `schema` **3** with `repository.urls[]`; legacy `url` is still written;
- **SSH key management**: **SSH Key...** beside the main-program repository input, the editor Git menu, and `exec git-ssh-key` (list/generate/public-key/test/configure) in both CLIs; `GIT_SSH_COMMAND` is injected automatically, fixing the 300s clone timeout caused by bundled MinGit without SSH configuration;
- **Repository URL management**: `exec repo-remotes`; the editor Repository settings tab now has SSH/HTTPS/HTTP fields plus a primary selector;
- **System Git priority**: `use_system_git=true` in `install.conf` now really skips bundled Git;
- bridge `0.4.5.17`: probes `repository.urls` in order, falls back on failure and reuses an existing checkout via `git remote set-url`.

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
