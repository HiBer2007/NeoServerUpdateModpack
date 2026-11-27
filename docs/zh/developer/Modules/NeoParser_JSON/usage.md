---
title: NeoParser_JSON 使用
description: NeoParser_JSON 的能力与调用方式。
lang: zh
status: complete
updated: 2026-11-27
---

# NeoParser_JSON 使用

## 快速开始

```cpp
NeoCore::PluginLoader loader;
loader.ScanDirectory("parsers");
auto* parser = loader.FindParser("config/example");
auto keys = parser->list_keys("config/example");
auto merged = parser->merge_entries("config/example", {"a.b"}, remote, local);
```

## 能力

| 项 | 值 |
|---|---|
| 能力名 | `JSON` |
| extensions | `.json` `.json5` `.jsonc` |
| supported_modes | full_sync / config_merge / no_sync |
| line_tracking | false |
| priority | 100 |

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `capability` | `ParserCapability capability() const` | 能力声明 |
| `can_handle` | `bool can_handle(const std::string&) const` | 扩展名判断 |
| `parse_entries` | `std::vector<ConfigEntry> parse_entries(const std::string&)` | 解析 |
| `merge_entries` | `std::string merge_entries(const std::string&, const std::vector<std::string>&, const std::string&, const std::string&)` | 合并 |
| `list_keys` | `std::vector<std::string> list_keys(const std::string&)` | 键列表 |

## 注意事项

- 合并写回会规范化格式（注释可能丢失）；
- 非法内容返回失败，由调用方回退整文件；
- 修改 `IConfigParser` 后宿主与插件需同批重建。

## 相关

- [README.md](./README.md)