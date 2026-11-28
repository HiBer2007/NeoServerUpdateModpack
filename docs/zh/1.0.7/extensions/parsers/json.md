---
title: JSON 解析器
description: NeoParser_JSON：.json/.json5/.jsonc 配置的键级合并。
lang: zh
status: complete
updated: 2026-11-27
---

# JSON 解析器

`NeoParser_JSON` 处理 Minecraft 模组最常见的 `.json` 配置（含 `.json5`/`.jsonc` 扩展名）。

## 能力

| 项 | 值 |
|---|---|
| 能力名 | `JSON` |
| 扩展名 | `.json`、`.json5`、`.jsonc` |
| 合并模式 | `full_sync`、`config_merge`、`no_sync` |
| 行级跟踪 | 否 |
| 优先级 | 100 |

## 使用

1. 仓库中放入配置文件（如 `branches/base/config/example.json`）；
2. 在 `workspace.json` 的 `sync_policies.files` 中为该文件选择 `config_merge` 并列出 `tracked_keys`：

```json
{
  "sync_policies": {
    "files": {
      "config/example.json": {
        "mode": "partial",
        "tracked_keys": ["server.port", "features.a"]
      }
    }
  }
}
```

3. 同步时：被跟踪键取远端值，其余键保留玩家本地值。

## 键路径

- 嵌套对象使用点号或 `/` 分隔（解析器 `parse_entries` 生成的 `key_path`）；
- 数组默认整体视为一个键；需要逐项合并时，把数组拆成多个键或使用 `full_sync`。

## 编辑器

配置编辑器通过 `NeoEditorExtension_Parser_JSON` 提供语法高亮与格式化；保存后写回仓库文件。

## 限制

- JSON 不支持注释；`.jsonc` 的注释在合并写回时可能丢失；
- 数字/布尔类型按解析器序列化规则写回（可能改变空白与键顺序）；
- 非法 JSON 会导致该文件合并失败并回退为整文件策略（见日志 `parse failed`）。

## 相关

- [解析器总览](./index.md)
- [同步规则](../../pack-author/sync-rules.md)
