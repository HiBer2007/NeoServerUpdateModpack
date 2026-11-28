---
title: 解析器扩展
description: 编辑器配置解析器扩展（NeoEditorExtension_Parser_*）：语法高亮、格式化与键列表。
lang: zh
status: complete
updated: 2026-11-27
---

# 解析器扩展

编辑器配置扩展让整合包作者在 GUI 中直接编辑仓库里的配置文件，并提供键列表用于选择同步键。

## 内置扩展

| 扩展 | 扩展名 | 文件 |
|---|---|---|
| NeoEditorExtension_Parser_JSON | `.json` `.json5` `.jsonc` | `editor/extension/parsers/` |
| NeoEditorExtension_Parser_YAML | `.yml` `.yaml` | 同上 |
| NeoEditorExtension_Parser_TOML | `.toml` | 同上 |
| NeoEditorExtension_Parser_SNBT | `.snbt` | 同上 |
| NeoEditorExtension_Parser_TXT | `.txt` `.properties` `.cfg` | 同上 |
| NeoEditorExtension_Parser_Properties | `.properties` | 同上 |

## 接口

实现 `NeoCore::IConfigEditorExtension`：

| 方法 | 作用 |
|---|---|
| `capability()` | 名称/扩展名 |
| `parse(text)` | 解析为可编辑结构 |
| `serialize()` | 写回文本 |
| `list_keys()` | 键列表（供同步策略选择） |

> 具体签名以 `modules/NeoCore/include/IConfigEditorExtension.h` 为准；不同扩展可提供额外格式化能力。

## 行为

- 双击配置类文件时按扩展名路由到对应编辑器；
- 保存时写回仓库文件并标记 Git 变更；
- 键列表用于「同步策略」中勾选 `tracked_keys`；
- 解析失败时显示错误并保留原文（不破坏文件）。

## 新增

1. 实现接口并导出工厂；
2. 提供 `*.meta.json`；
3. 放入 `editor/extension/parsers/`；
4. 运行时同时提供对应 `NeoParser_*`（若需要同步合并）。

## 相关

- [编辑器扩展总览](./index.md)
- [配置解析器](../parsers/index.md)
