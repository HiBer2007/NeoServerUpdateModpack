---
title: 指针解析器扩展
description: 编辑器指针解析器扩展（NeoEditorExtension_Pointer_*）：在 GUI 中配置 resolver 与 metadata。
lang: zh
status: complete
updated: 2026-11-27
---

# 指针解析器扩展

指针解析器扩展为编辑器的「指针编辑器」提供字段表单与验证，让作者无需手写 JSON。

## 内置扩展

| 扩展 | resolver | 文件 |
|---|---|---|
| NeoEditorExtension_Pointer_DirectURL | `direct_url` | `editor/extension/pointers/` |
| NeoEditorExtension_Pointer_Modrinth | `modrinth` | 同上 |

## 接口

实现 `NeoCore::IPointerEditorExtension`：

| 成员 | 作用 |
|---|---|
| `resolver_name()` | 对应运行时解析器名 |
| `fields()` | 表单字段（key/label/required/placeholder） |
| `validate(metadata)` | 校验 metadata 合法性 |
| `build_metadata(values)` | 生成指针 metadata JSON |

> 逐字签名见 `modules/NeoCore/include/IPointerEditorExtension.h`。

## 使用

1. 打开「指针编辑器」（工具栏/右键）；
2. 选择 resolver（DirectURL/Modrinth）；
3. 填写字段（URL 或 project/version）；
4. 保存后写入 `branch_config/<sha>.pointer`，并更新 `branch_config/<branch>.json` 的 `file_manifest` 与 `pointer_files`。

## 校验

- URL 必须为 `http(s)://`；
- SHA-256 必须为 64 位十六进制；
- Modrinth 的 `version_id` 与 `project_id` 不能为空；
- 校验失败时不允许保存（避免生成不可解析的指针）。

## 相关

- [编辑器扩展总览](./index.md)
- [指针解析器](../pointers/index.md)
