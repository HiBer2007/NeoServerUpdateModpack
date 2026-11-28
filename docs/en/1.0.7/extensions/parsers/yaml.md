---
title: YAML Parser
description: NeoParser_YAML: key-level merging for `.yml` `.yaml`.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/extensions/parsers/yaml.md
---

# YAML Parser

`NeoParser_YAML` handles `.yml` `.yaml` and merges configs by key.

## Capability

| Item | Value |
|---|---|
| name | `YAML` |
| extensions | `.yml` `.yaml` |
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

- YAML comments/anchors may be normalized on merge.
- Invalid content fails the merge and the caller falls back to a full-file strategy;
- See [parser overview](./index.md).

## Related

- [Sync rules](../../pack-author/sync-rules.md)