---
title: NeoExporter_HMCL 使用
description: NeoExporter_HMCL 的格式、字段与调用方式。
lang: zh
status: complete
updated: 2026-11-27
---

# NeoExporter_HMCL 使用

## 快速开始

```cpp
auto* exporter = NeoBuild::ModpackExporter::create("exporters", "hmcl");
NeoCore::BuildTarget target;
target.branch = "base";
target.git_branch = "main";
target.metadata.name = "My Pack";
target.metadata.version = "1.0.0";
auto r = exporter->build_modpack(target, &progress, &cancel);
exporter->export_modpack(buildDir, "out", target.metadata);
```

## 格式

| 项 | 值 |
|---|---|
| format | `hmcl` |
| extension | `（目录）` |
| 字段 | 无 |

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `format_name` | `std::string format_name() const` | 格式名 |
| `file_extension` | `std::string file_extension() const` | 扩展名 |
| `format_description` | `std::string format_description() const` | 描述 |
| `build_modpack` | `BuildResult build_modpack(const BuildTarget&, IBuildProgress*, CancelToken*)` | 构建 |
| `export_modpack` | `bool export_modpack(const std::string&, const std::string&, const ExportMetadata&)` | 导出 |

## 注意事项

- NSUM 导出器默认只写清单；`embed_patterns` 才会内嵌；
- HMCL 导出器写目录而非文件；
- 字段声明（meta）决定 GUI 表单与 CLI 额外参数。

## 相关

- [README.md](./README.md)