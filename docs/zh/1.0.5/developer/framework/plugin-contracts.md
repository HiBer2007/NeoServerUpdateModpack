---
title: 插件契约
description: parser、pointer、exporter、editor-extension 四类插件接口、工厂与 meta.json 字段。
lang: zh
status: complete
updated: 2026-11-27
---

# 插件契约

## 1. 配置解析器（parser）

```cpp
extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser();
```

`IConfigParser` 方法：

| 方法 | 签名 |
|---|---|
| `capability()` | `ParserCapability capability() const` |
| `can_handle()` | `bool can_handle(const std::string& filepath) const` |
| `parse_entries()` | `std::vector<ConfigEntry> parse_entries(const std::string& filepath)` |
| `merge_entries()` | `std::string merge_entries(const std::string& filepath, const std::vector<std::string>& tracked_keys, const std::string& remote_content, const std::string& local_content)` |
| `parse_lines()` | `std::vector<LineEntry> parse_lines(const std::string& filepath)`（可选） |
| `merge_lines()` | `std::string merge_lines(...)`（可选） |
| `list_keys()` | `std::vector<std::string> list_keys(const std::string& filepath)` |

## 2. 指针解析器（pointer）

```cpp
extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer();
```

| 方法 | 签名 |
|---|---|
| `name()` | `std::string name() const` |
| `can_handle()` | `bool can_handle(const PointerInfo& ptr) const` |
| `resolve_url()` | `std::string resolve_url(const PointerInfo& ptr)` |
| `validate()` | `bool validate(...)` |
| `supported_download_methods()` | `std::vector<std::string>`（可选） |
| `can_batch_search()` / `batch_search()` | 批量搜索（可选） |

## 3. 导出器（exporter）

```cpp
extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter();
```

| 方法 | 签名 |
|---|---|
| `format_name()` | `std::string format_name() const` |
| `file_extension()` | `std::string file_extension() const` |
| `format_description()` | `std::string format_description() const` |
| `build_modpack()` | `BuildResult build_modpack(const BuildTarget&, IBuildProgress*, CancelToken*)` |
| `export_modpack()` | `bool export_modpack(const std::string& build_dir, const std::string& output_path, const ExportMetadata&)` |

## 4. 编辑器扩展

| 类型 | 工厂 | 接口 |
|---|---|---|
| 配置编辑器 | `CreateConfigEditor` | `IConfigEditorExtension` |
| 指针编辑器 | `CreateEditorExtension` | `IPointerEditorExtension` |

两者都提供 `QWidget* createEditor(QWidget* parent)`，由编辑器嵌入。

## meta.json

```json
{
  "name": "JSON Config Parser",
  "version": "1.0.0",
  "dll": "NeoParser_JSON.dll",
  "capability": {
    "name": "JSON",
    "extensions": [".json", ".json5", ".jsonc"],
    "supported_modes": ["full_sync", "config_merge", "no_sync"],
    "line_tracking": false,
    "priority": 100
  }
}
```

导出器额外字段：`format`、`extension`、`description`、`fields[]`（GUI 表单）。

## 日志接入（可选）

```cpp
NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_JSON")
```

导出 `SetPluginLogSink(ILogSink*)`，宿主加载时注入。

## 相关

- [扩展注册表](./extension-registry.md)
- [ABI](./abi.md)
