---
title: Architecture Decision Records
description: NSUM's key design decisions, alternatives and consequences.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/adr/index.md
---

# Architecture Decision Records

ADRs record **why** a design exists, what was rejected, and what it costs. Write one whenever a decision changes interfaces, formats, or operational behavior.

## When to write one

- A new format or protocol (e.g. `.nsumpack` v2);
- A change to sync semantics or file layouts;
- A new extension mechanism or ABI change;
- A release/versioning policy change.

## Format

Status → Context → Decision → Alternatives → Consequences → Related.

## Index

| ADR | Title |
|---|---|
| [0001](./0001-pointer-cache.md) | Pointer files and content-addressed cache |
| [0002](./0002-l1-l2-l3-sync.md) | Three-tier sync (L1/L2/L3) |
| [0003](./0003-mclhr-store.md) | MCLHR mod store and hard-link projection |
| [0004](./0004-hic-flow.md) | Declarative HIC install flow |
| [0005](./0005-bridge-injection.md) | Launch injection (bridge) |
| [0006](./0006-nsumpack-v2.md) | Manifest-only `.nsumpack` (pack_format 2) |
| [0007](./0007-parallel-download.md) | HTTP Range parallel downloads |
| [0008](./0008-editor-cli.md) | Headless editor CLI |
| [0009](./0009-bilingual-docs.md) | Bilingual docs |
| [0010](./0010-module-boundaries.md) | Module boundaries and standalone repos |

## Related

- [Architecture](../architecture/index.md)
- [Contributing](../contributing.md)
