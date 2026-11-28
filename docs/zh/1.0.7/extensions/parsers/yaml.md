---
title: YAML 解析器
description: NeoParser_YAML：.yml/.yaml 配置的键级合并。
lang: zh
status: complete
updated: 2026-11-27
---

# YAML 解析器

`NeoParser_YAML` 处理 `.yml` / `.yaml`（常见于插件、代理与部分模组配置）。

## 能力

| 项 | 值 |
|---|---|
| 能力名 | `YAML` |
| 扩展名 | `.yml`、`.yaml` |
| 合并模式 | `full_sync`、`config_merge`、`no_sync` |
| 行级跟踪 | 否 |
| 优先级 | 90 |

## 合并示例

```yaml
# branches/base/config/example.yml（远端）
server:
  motd: "NSUM Server"
  max-players: 20
features:
  a: true
```

`tracked_keys: ["server.motd", "features.a"]` 时：

- `server.motd`、`features.a` 采用远端值；
- `server.max-players` 及未列出的键保留玩家本地值。

## 注意

- YAML 注释在合并写回时可能丢失（解析器重建文档）；
- 锚点/别名（`&`/`*`）在合并后会被展开；
- 多文档（`---`）建议拆分文件，合并仅按第一文档处理；
- 制表符缩进等非法 YAML 会使合并失败并记录日志。

## 相关

- [解析器总览](./index.md)
- [同步规则](../../pack-author/sync-rules.md)
