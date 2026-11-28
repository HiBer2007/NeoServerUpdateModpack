---
title: Properties 解析器
description: NeoParser_Properties：.properties 键值配置的键级合并。
lang: zh
status: complete
updated: 2026-11-27
---

# Properties 解析器

`NeoParser_Properties` 处理 Java `.properties`（服务器 `server.properties`、插件配置等）。

## 能力

| 项 | 值 |
|---|---|
| 能力名 | `Properties` |
| 扩展名 | `.properties` |
| 合并模式 | `full_sync`、`config_merge`、`no_sync` |
| 行级跟踪 | 否 |
| 优先级 | 80 |

## 合并示例

```properties
# branches/base/server.properties（远端）
motd=NSUM Server
max-players=20
online-mode=true
```

策略：

```json
{
  "sync_policies": {
    "files": {
      "server.properties": {
        "mode": "partial",
        "tracked_keys": ["motd", "max-players"]
      }
    }
  }
}
```

- 被跟踪键采用远端值；
- `online-mode` 等未列出键保留本地值（适合让玩家/服主自行决定）。

## 注意

- 键值分隔符 `=`、`:`、空格均由解析器规范化；
- 注释（`#`/`!`）在写回时可能丢失；
- `.properties` 扩展名同时被 TXT 解析器支持（优先级低于 Properties，故由本解析器处理）；
- `\uXXXX` 转义按 Java Properties 规则解析。

## 相关

- [解析器总览](./index.md)
- [同步规则](../../pack-author/sync-rules.md)
