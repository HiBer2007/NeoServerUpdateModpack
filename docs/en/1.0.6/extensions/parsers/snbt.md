---
title: SNBT Parser
description: NeoParser_SNBT: key-level merging for `.snbt`.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/extensions/parsers/snbt.md
---

# SNBT Parser

`NeoParser_SNBT` handles `.snbt` and merges configs by key.

## Capability

| Item | Value |
|---|---|
| name | `SNBT` |
| extensions | `.snbt` |
| modes | full_sync / config_merge / no_sync |

## Usage

```json
{
  "sync_policies": {
    "files": {
      "config/example": { "mode": "partial", "tracked_keys": ["server.port"] }
    }
  }
}
```

- Tracked keys take the remote value;
- Other keys keep the player-local value.

## Notes

- Uses nbtcpp with a preserving writer; large files are slow.
- Invalid content fails the merge and the caller falls back to a full-file strategy;
- See [parser overview](./index.md).

## Related

- [Sync rules](../../pack-author/sync-rules.md)