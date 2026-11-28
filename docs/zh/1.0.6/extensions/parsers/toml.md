---
title: TOML 解析器
description: NeoParser_TOML：.toml 配置的键级合并。
lang: zh
status: complete
updated: 2026-11-27
---

# TOML 解析器

`NeoParser_TOML` 处理 `.toml`（常见于 Fabric/NeoForge 模组与工具配置）。

## 能力

| 项 | 值 |
|---|---|
| 能力名 | `TOML` |
| 扩展名 | `.toml` |
| 合并模式 | `full_sync`、`config_merge`、`no_sync` |
| 行级跟踪 | 否 |
| 优先级 | 80 |

## 合并示例

```toml
# branches/base/config/example.toml（远端）
[server]
motd = "NSUM Server"
max_players = 20

[features]
experimental = false
```

`tracked_keys: ["server.motd", "features.experimental"]` 时，键按表路径（`table.key`）匹配；未列入的键保留本地值。

## 注意

- 注释在写回时可能丢失；
- 表数组（`[[items]]`）整体作为数组键处理；
- 日期/时间类型按 TOML 序列化规则写回；
- 非法 TOML 会使合并失败并记录日志。

## 相关

- [解析器总览](./index.md)
- [同步规则](../../pack-author/sync-rules.md)
