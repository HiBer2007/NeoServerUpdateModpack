---
title: ADR 0007 · HTTP Range parallel downloads
description: HTTP Range parallel downloads
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/adr/0007-parallel-download.md
---

# ADR 0007 · HTTP Range parallel downloads

## Status

Accepted (2026-11-27).

## Context

Large downloads were slow; some networks stall with concurrency.

## Decision

Probe Range support, chunk into segments with a per-server cap, verify, and fall back to single stream on any failure; enforce a total deadline.

## Alternatives

| Option | Pros | Cons |
|---|---|---|
| Keep the previous approach | Less work now | Does not solve the problem |
| Adopt the decision | Solves the problem | Migration and maintenance cost |

## Consequences

- Positive: Faster on good networks
- Negative:  extra complexity and fallback paths.

## Related

- [ADR index](./index.md)