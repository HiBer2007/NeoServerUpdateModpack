---
title: ADR 0003 · MCLHR mod store and hard-link projection
description: MCLHR mod store and hard-link projection
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/adr/0003-mclhr-store.md
---

# ADR 0003 · MCLHR mod store and hard-link projection

## Status

Accepted (2026-11-27).

## Context

Multiple instances share the same mods; copying wastes disk and breaks deduplication.

## Decision

Use MCLHR stores plus `CreateHardLinkW` projection into `mods/`, falling back to copies across volumes; write `.mclhr/mods.json`.

## Alternatives

| Option | Pros | Cons |
|---|---|---|
| Keep the previous approach | Less work now | Does not solve the problem |
| Adopt the decision | Solves the problem | Migration and maintenance cost |

## Consequences

- Positive: Major disk savings
- Negative:  hard links do not survive cross-volume moves.

## Related

- [ADR index](./index.md)