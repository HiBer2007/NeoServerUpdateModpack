---
title: SNBT 解析器
description: NeoParser_SNBT：.snbt（NBT 文本表示）的键级合并与保留写回。
lang: zh
status: complete
updated: 2026-11-27
---

# SNBT 解析器

`NeoParser_SNBT` 处理 `.snbt`（SNBT/NBT 文本格式，常见于结构、数据包与部分模组配置）。

## 能力

| 项 | 值 |
|---|---|
| 能力名 | `SNBT` |
| 扩展名 | `.snbt` |
| 合并模式 | `full_sync`、`config_merge`、`no_sync` |
| 行级跟踪 | 否 |
| 优先级 | 70 |

## 特点

- 使用项目内置的 `nbtcpp`（git submodule）解析；
- 通过 `SnbtPreservingWriter` 尽量保留未修改部分的原始写法；
- 键路径使用 NBT 复合标签的层级（如 `Data.server.motd`）。

## 示例

```snbt
{
  Data: {
    motd: "NSUM Server",
    maxPlayers: 20
  }
}
```

`tracked_keys: ["Data.motd"]` 时只替换 `Data.motd`，`maxPlayers` 保留本地值。

## 注意

- 类型前缀（`1b`、`1.0f` 等）由解析器规范化；
- 数组/列表作为整体键处理；
- 非法 SNBT 会使合并失败并记录日志；
- 大文件（>10 MB）合并耗时明显，建议 `full_sync`。

## 相关

- [解析器总览](./index.md)
- [同步规则](../../pack-author/sync-rules.md)
