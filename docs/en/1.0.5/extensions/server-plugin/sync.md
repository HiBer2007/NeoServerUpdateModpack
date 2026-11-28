---
title: Server Sync
description: How the MCDR plugin syncs the server branch and merges plugin directories.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/extensions/server-plugin/sync.md
---

# Server Sync

## Flow

```mermaid
sequenceDiagram
    participant M as MCDR plugin
    participant R as NSUM Runtime
    participant D as Server directory
    M->>R: exec build --format hmcl --export <server>
    R->>D: Write via sync_policies
    R-->>M: exit 0 / logs
    M->>D: Merge plugin/<name>/ → plugins/<name>/
    M->>M: Rewrite config.yml
```

## Command

```powershell
NeoServerUpdateModpack.exe exec build --repo <repository> --modpack server --git-branch main `
    --format hmcl --export <server directory>
```

## Plugin merge

Repository layout:

```text
plugin/MyPlugin/
├─ plugin.yml
└─ ...
```

After sync it is merged into `plugins/MyPlugin/` and the intermediate directory is deleted.

## Protection

- `mirror` protects server runtime files (`logs/`, `world*`, `server.properties`, per policy);
- Prefer `incremental_add`/`incremental_overwrite` on servers to avoid deleting saves;
- Stop the server or use `save-off`/`save-all` before syncing.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| Server won't start after sync | Core/plugin version mismatch | Check logs and core config |
| Duplicate plugins | Both `plugins/` and `plugin/` exist | Keep only one layout |
| Config overwritten | `full` policy | Use `partial` to preserve local keys |

## Related

- [Config](./config.md)
- [Sync rules](../../pack-author/sync-rules.md)
