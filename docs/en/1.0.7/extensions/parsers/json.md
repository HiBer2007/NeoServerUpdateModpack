---
title: JSON Parser
description: NeoParser_JSON: key-level merging for `.json` `.json5` `.jsonc`.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/extensions/parsers/json.md
---

# JSON Parser

`NeoParser_JSON` handles `.json` `.json5` `.jsonc` and merges configs by key.

## Capability

| Item | Value |
|---|---|
| name | `JSON` |
| extensions | `.json` `.json5` `.jsonc` |
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

- JSON has no comments; `.jsonc` comments may be lost on merge.
- Invalid content fails the merge and the caller falls back to a full-file strategy;
- See [parser overview](./index.md).

## Related

- [Sync rules](../../pack-author/sync-rules.md)