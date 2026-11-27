---
title: ADR 0009 · Bilingual docs
description: Bilingual docs
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/adr/0009-bilingual-docs.md
---

# ADR 0009 · Bilingual docs

## Status

Accepted (2026-11-27).

## Context

Documentation must serve Chinese and English readers and migrate to Nextra.

## Decision

Store `docs/zh` and `docs/en` as mirrored trees with per-directory `_meta.json`; Chinese is canonical; validate parity and links in CI-style checks.

## Alternatives

| Option | Pros | Cons |
|---|---|---|
| Keep the previous approach | Less work now | Does not solve the problem |
| Adopt the decision | Solves the problem | Migration and maintenance cost |

## Consequences

- Positive: Consistent navigation and translation tracking
- Negative:  doubles authoring effort.

## Related

- [ADR index](./index.md)