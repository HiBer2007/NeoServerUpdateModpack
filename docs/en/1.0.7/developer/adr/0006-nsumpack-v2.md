---
title: ADR 0006 · Manifest-only .nsumpack (pack_format 2)
description: Manifest-only .nsumpack (pack_format 2)
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/adr/0006-nsumpack-v2.md
---

# ADR 0006 · Manifest-only .nsumpack (pack_format 2)

> **Update**: since 1.0.6 `pack_format` / `schema` are 3 with `repository.urls`; the manifest-only decision still stands, see [.nsumpack format](../../pack-author/nsumpack.md).

## Status

Accepted (2026-11-27).

## Context

v1 packs embedded all build output, making distribution files huge and duplicated with the repository.

## Decision

Default to `nsum.index.json` only: repository info, file index, commit metadata; embedding requires explicit `embed_patterns` and is recorded in `embedded[]`.

## Alternatives

| Option | Pros | Cons |
|---|---|---|
| Keep the previous approach | Less work now | Does not solve the problem |
| Adopt the decision | Solves the problem | Migration and maintenance cost |

## Consequences

- Positive: ~1 KB packs and incremental sync
- Negative:  clients need repository access and Runtime support.

## Related

- [ADR index](./index.md)