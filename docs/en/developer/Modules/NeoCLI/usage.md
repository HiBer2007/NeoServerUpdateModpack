---
title: NeoCLI usage
description: NeoCLI public APIs and typical usage.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoCLI/usage.md
---

# NeoCLI usage

## Quick start

See the module headers and the corresponding Chinese usage page for the full walkthrough.

## Public API

| Symbol | Description |
|---|---|
| `ArgParser` | class | Parse args |\n| `CliDispatcher` | class | Dispatch commands |\n| `CliOutput` | class | JSON output |

## Notes

- Rebuild all dependent targets after changing shared headers;
- Prefer the module's public headers over internal sources;
- Log English messages only, and keep paths UTF-8.

## Related

- [README.md](./README.md)