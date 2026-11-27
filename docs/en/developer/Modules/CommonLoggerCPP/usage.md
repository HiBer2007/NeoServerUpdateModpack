---
title: CommonLoggerCPP usage
description: CommonLoggerCPP public APIs and typical usage.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/CommonLoggerCPP/usage.md
---

# CommonLoggerCPP usage

## Quick start

See the module headers and the corresponding Chinese usage page for the full walkthrough.

## Public API

| Symbol | Description |
|---|---|
| `CLogger` | class | Log facade |\n| `ILogSink` | interface | Sink |\n| `LoggerLogSink` | class | Host sink |

## Notes

- Rebuild all dependent targets after changing shared headers;
- Prefer the module's public headers over internal sources;
- Log English messages only, and keep paths UTF-8.

## Related

- [README.md](./README.md)