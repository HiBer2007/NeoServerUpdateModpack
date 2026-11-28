---
title: ADR 0008 · Headless editor CLI
description: Headless editor CLI
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/adr/0008-editor-cli.md
---

# ADR 0008 · Headless editor CLI

## Status

Accepted (2026-11-27).

## Context

AI and scripts need to read/modify repositories without a GUI.

## Decision

Add `info`/`exec`/`help` modes to NeoWorkspaceEditor with a single JSON output, repository-scoped paths, and safe Git subcommands.

## Alternatives

| Option | Pros | Cons |
|---|---|---|
| Keep the previous approach | Less work now | Does not solve the problem |
| Adopt the decision | Solves the problem | Migration and maintenance cost |

## Consequences

- Positive: Automation-friendly
- Negative:  must keep JSON contract stable.

## Related

- [ADR index](./index.md)