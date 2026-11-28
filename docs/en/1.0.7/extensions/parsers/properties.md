---
title: Properties Parser
description: NeoParser_Properties: key-level merging for `.properties`.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/extensions/parsers/properties.md
---

# Properties Parser

`NeoParser_Properties` handles `.properties` and merges configs by key.

## Capability

| Item | Value |
|---|---|
| name | `Properties` |
| extensions | `.properties` |
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

- Java properties escaping is honored; comments may be lost.
- Invalid content fails the merge and the caller falls back to a full-file strategy;
- See [parser overview](./index.md).

## Related

- [Sync rules](../../pack-author/sync-rules.md)