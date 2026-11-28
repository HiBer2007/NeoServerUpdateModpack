---
title: Plugin Contracts
description: Parser, pointer, exporter and editor-extension interfaces, factories and meta.json fields.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/framework/plugin-contracts.md
---

# Plugin Contracts

## 1. Config parser

```cpp
extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser();
```

`IConfigParser` methods:

| Method | Signature |
|---|---|
| `capability()` | `ParserCapability capability() const` |
| `can_handle()` | `bool can_handle(const std::string& filepath) const` |
| `parse_entries()` | `std::vector<ConfigEntry> parse_entries(const std::string& filepath)` |
| `merge_entries()` | `std::string merge_entries(const std::string& filepath, const std::vector<std::string>& tracked_keys, const std::string& remote_content, const std::string& local_content)` |
| `parse_lines()` | `std::vector<LineEntry> parse_lines(const std::string& filepath)` (optional) |
| `merge_lines()` | `std::string merge_lines(...)` (optional) |
| `list_keys()` | `std::vector<std::string> list_keys(const std::string& filepath)` |

## 2. Pointer resolver

```cpp
extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer();
```

| Method | Signature |
|---|---|
| `name()` | `std::string name() const` |
| `can_handle()` | `bool can_handle(const PointerInfo& ptr) const` |
| `resolve_url()` | `std::string resolve_url(const PointerInfo& ptr)` |
| `validate()` | `bool validate(...)` |
| `supported_download_methods()` | `std::vector<std::string>` (optional) |
| `can_batch_search()` / `batch_search()` | batch search (optional) |

## 3. Exporter

```cpp
extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter();
```

| Method | Signature |
|---|---|
| `format_name()` | `std::string format_name() const` |
| `file_extension()` | `std::string file_extension() const` |
| `format_description()` | `std::string format_description() const` |
| `build_modpack()` | `BuildResult build_modpack(const BuildTarget&, IBuildProgress*, CancelToken*)` |
| `export_modpack()` | `bool export_modpack(const std::string& build_dir, const std::string& output_path, const ExportMetadata&)` |

## 4. Editor extensions

| Type | Factory | Interface |
|---|---|---|
| Config editor | `CreateConfigEditor` | `IConfigEditorExtension` |
| Pointer editor | `CreateEditorExtension` | `IPointerEditorExtension` |

Both provide `QWidget* createEditor(QWidget* parent)` embedded by the editor.

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

Exporters add `format`, `extension`, `description`, `fields[]` (GUI form).

## Logging (optional)

```cpp
NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_JSON")
```

Exports `SetPluginLogSink(ILogSink*)`; the host injects it when loading.

## Related

- [Extension registry](./extension-registry.md)
- [ABI](./abi.md)
