---
title: ADR 0004 · Declarative HIC install flow
description: Declarative HIC install flow
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/adr/0004-hic-flow.md
---

# ADR 0004 · Declarative HIC install flow

## Status

Accepted (2026-11-27).

## Context

Installer steps must be reusable across GUI/TUI/CLI and product-specific variants.

## Decision

Describe installs as JSON flows with Lua conditions and extension-provided step types; shells only drive the flow engine.

## Alternatives

| Option | Pros | Cons |
|---|---|---|
| Keep the previous approach | Less work now | Does not solve the problem |
| Adopt the decision | Solves the problem | Migration and maintenance cost |

## Consequences

- Positive: One flow drives all shells
- Negative:  flows become another artifact to maintain.

## Related

- [ADR index](./index.md)