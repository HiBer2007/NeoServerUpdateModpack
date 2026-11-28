---
title: NeoPointer_Modrinth 使用
description: NeoPointer_Modrinth 的解析器名称、metadata 与 API。
lang: zh
status: complete
updated: 2026-11-27
---

# NeoPointer_Modrinth 使用

## 快速开始

```cpp
NeoCore::PluginLoader loader; // 仅 parser；指针加载由 PointerDownloader 完成
NeoBuild::PointerDownloader downloader;
auto result = downloader.resolve(pointerPath, repoDir);
```

## 能力

| 项 | 值 |
|---|---|
| resolver | `modrinth` |
| metadata 字段 | `project_id`（必填）、`version_id`（推荐） |

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `name` | `std::string name() const` | 解析器名 |
| `can_handle` | `bool can_handle(const PointerInfo&) const` | 是否支持 |
| `resolve_url` | `std::string resolve_url(const PointerInfo&)` | 解析地址 |
| `validate` | `bool validate(...)` | 下载后校验 |

## 注意事项

- URL/版本必须长期可用；
- 解析失败应返回错误，不静默；
- 与编辑器扩展 `NeoEditorExtension_Pointer_*` 配对发布。

## 相关

- [README.md](./README.md)