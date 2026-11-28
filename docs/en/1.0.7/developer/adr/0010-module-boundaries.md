---
title: ADR 0010 · Module boundaries and standalone repos
description: Module boundaries and standalone repos
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/adr/0010-module-boundaries.md
---

# ADR 0010 · Module boundaries and standalone repos

## Status

Accepted (2026-11-27).

## Context

Shared components (logger, GUI lib, crash handler, reader) are useful outside NSUM.

## Decision

Keep modules independently buildable with explicit interfaces; move reusable ones to standalone repositories and consume them as submodules.

## Alternatives

| Option | Pros | Cons |
|---|---|---|
| Keep the previous approach | Less work now | Does not solve the problem |
| Adopt the decision | Solves the problem | Migration and maintenance cost |

## Consequences

- Positive: Reuse and clearer boundaries
- Negative:  submodule coordination overhead.

## Related

- [ADR index](./index.md)