---
title: ADR 0001 · Pointer files and content-addressed cache
description: Pointer files and content-addressed cache
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/adr/0001-pointer-cache.md
---

# ADR 0001 · Pointer files and content-addressed cache

## Status

Accepted (2026-11-27).

## Context

Large jars should not live in Git, yet builds need reproducible inputs and offline reuse.

## Decision

Store `<sha256>.pointer` placeholders in `branch_config/`; map paths to hashes in `file_manifest`; resolve downloads via plugins; cache by SHA-256 and re-verify before every use.

## Alternatives

| Option | Pros | Cons |
|---|---|---|
| Keep the previous approach | Less work now | Does not solve the problem |
| Adopt the decision | Solves the problem | Migration and maintenance cost |

## Consequences

- Positive: Small repositories, deduplicated downloads
- Negative:  requires resolvers and verification on every use.

## Related

- [ADR index](./index.md)