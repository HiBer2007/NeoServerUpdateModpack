---
title: ADR 0005 · Launch injection (bridge)
description: Launch injection (bridge)
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/developer/adr/0005-bridge-injection.md
---

# ADR 0005 · Launch injection (bridge)

## Status

Accepted (2026-11-27).

## Context

NSUM must sync before the game starts, without users manually running commands.

## Decision

Inject `NSumSyncTask` via Mixin into `LauncherHelper.checkGameState`; show branch selection and progress in the task dialog; abort on failure.

## Alternatives

| Option | Pros | Cons |
|---|---|---|
| Keep the previous approach | Less work now | Does not solve the problem |
| Adopt the decision | Solves the problem | Migration and maintenance cost |

## Consequences

- Positive: Seamless updates
- Negative:  depends on matching HMCL/FLF versions.

## Related

- [ADR index](./index.md)