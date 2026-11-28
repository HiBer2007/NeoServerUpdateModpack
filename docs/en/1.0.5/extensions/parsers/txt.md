---
title: TXT Parser
description: NeoParser_TXT: key-level merging for `.txt` `.properties` `.cfg`.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/extensions/parsers/txt.md
---

# TXT Parser

`NeoParser_TXT` handles `.txt` `.properties` `.cfg` and merges configs by key.

## Capability

| Item | Value |
|---|---|
| name | `TXT` |
| extensions | `.txt` `.properties` `.cfg` |
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

- The only parser with line-level tracking (`tracked_lines`).
- Invalid content fails the merge and the caller falls back to a full-file strategy;
- See [parser overview](./index.md).

## Related

- [Sync rules](../../pack-author/sync-rules.md)