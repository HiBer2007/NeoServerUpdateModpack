---
title: Design Goals
description: NSUM's goals, constraints, tradeoffs and non-goals.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/architecture/design-goals.md
---

# Design Goals

## Goals

1. **Auditable server content**: everything lives in Git, reviewable and revertible;
2. **Configure once, follow automatically**: players import once, then sync on every launch;
3. **No redundant large-file transfer**: content addressing + pointers + cache;
4. **Small distribution**: `.nsumpack` is manifest-only by default;
5. **Visible failure**: network/verification/branch errors are explicit, never silent stale content;
6. **No system dependencies**: bundled Git and runtime, offline-capable install.

## Constraints

| Constraint | Impact |
|---|---|
| Windows-first | Path encoding and process creation need special handling (UTF-8 argv, `GetModuleFileNameW`) |
| HMCL ecosystem | Injected via bridge; must match HMCL/FLF versions |
| Git repo is the only source | No client-private patches; patches also live in the repo |
| No central server | Metadata only for version discovery |
| Plugin-based | Formats/parsers/downloaders extend without recompiling the main program |

## Tradeoffs

| Decision | Choice | Rejected |
|---|---|---|
| Distribution | Manifest-only `.nsumpack` + repository sync | Full bundle download (saves bandwidth, requires reachable repo) |
| Large files | Pointers + content-addressed cache | Shipping jars in the pack (simple but huge) |
| Sync | Three tiers (L1/L2/L3) + policies | Full overwrite (simple but destroys player settings) |
| Failures | Abort launch | Silent degradation (players may join with wrong content) |
| Plugins | C ABI factories + exported symbols | Language-agnostic process isolation |

## Non-goals

- A game content distribution platform (does not host jars);
- A server control panel;
- Replacing HMCL instance management;
- Binary compatibility across major HMCL versions (bridge must match).

## Verifiable goals

| Goal | How to verify |
|---|---|
| Manifest size | `.nsumpack` without embedding ≈ 1 KB |
| Zero trust | Cache hits still recompute SHA-256 |
| Bundled Git | `info git` reports bundled |
| Visible failure | Launch aborts and logs an error on sync failure |

## Related

- [Overview](./overview.md)
- [Security](./security.md)
