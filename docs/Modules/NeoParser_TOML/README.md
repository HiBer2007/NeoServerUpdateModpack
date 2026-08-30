# NeoParser_TOML 说明文档

## 概述

NeoParser_TOML 是 NeoServerUpdateModpack 的 TOML 配置解析器插件（SHARED DLL）。它实现 `NeoCore::IConfigParser` 接口，负责对 `.toml` 文件做**读取解析（平面化条目）**、**按追踪键合并（partial merge）**与**键枚举**，供 serverconfig 同步与配置文件的 partial 合并使用。构建产物 `NeoParser_TOML.dll` 与 `NeoParser_TOML.meta.json` 由根 CMakeLists 的 `neo_deploy` 部署到 `deploy/parsers/`，运行时由 `NeoCore::PluginLoader::ScanDirectory` 加载，宿主经 `PluginLoader::FindParser` 按扩展名取用。

## 设计目标

- 覆盖 `.toml` 扩展名（含 table 嵌套、array of tables 结构）。
- key-path 平面化：table 以点路径（`a.b`）、array of tables 以下标（`servers[0]`）呈现，供 GUI 键列表与 partial 键勾选。
- partial 合并以**本地内容为基线**：仅被追踪键按类型写回远端值，未追踪内容与本地结构保持不变（输出经 tomlplusplus 序列化）。

## 模块边界

- **做**：纯解析/合并逻辑——`parse_entries`、`merge_entries`、`list_keys`、`can_handle`。
- **不做**：不回写文件；不含编辑器 UI（编辑器扩展在 `NeoEditorExtension_Parser_TOML`）；不做行级追踪（`supports_line_tracking = false`）；注释在合并输出中不保留（tomlplusplus 重序列化）。

## 依赖关系

| 依赖 | 类型 | 用途 | 来源 |
|------|------|------|------|
| NeoCore | 静态库 | `IConfigParser` 接口定义 | modules/NeoCore（PRIVATE 链接） |
| spdlog | 第三方库 | 插件日志（经插件日志注册机制接入宿主） | vcpkg，target `spdlog::spdlog` |
| tomlplusplus | 第三方库 | TOML 解析（`toml::parse_file`/`toml::parse`）与序列化（`operator<<`） | vcpkg，target `tomlplusplus::tomlplusplus` |

> 以 `modules/NeoParser_TOML/CMakeLists.txt` 为准：`PRIVATE NeoCore spdlog::spdlog tomlplusplus::tomlplusplus`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库目标定义与依赖链接 |
| `NeoParser_TOML.meta.json` | 插件能力描述（name/version/dll/capability） |
| `src/parser_toml.cpp` | 全部实现：平面化、导航、类型化合并、`TomlConfigParser`、`CreateParser`、日志 sink |

## 构建集成

- 目标：`add_library(NeoParser_TOML SHARED src/parser_toml.cpp)`。
- 导出符号（必须带 `__declspec(dllexport)`）：
  - `extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser()`，返回 `new TomlConfigParser()`；
  - `NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_TOML")` 追加导出 `SetPluginLogSink(ILogSink*)`。
- 部署：根 CMakeLists `PARSER_TARGETS` 含本目标，`neo_deploy` POST_BUILD 将 DLL 与 meta.json 复制到 `${CMAKE_BINARY_DIR}/deploy/parsers/`。
- 版本资源：`nsum_add_version_info(NeoParser_TOML "NSUM 配置解析器插件 (NeoParser_TOML)" "NSUM构建工具")`。

## 公共符号

| 符号 | 说明 |
|------|------|
| `CreateParser()` | DLL 导出工厂，返回 `NeoCore::IConfigParser*` |
| `TomlConfigParser` | 实现类（匿名 namespace，DLL 内部可见） |

运行期 capability（`TomlConfigParser::capability()`）：

| 字段 | 值 |
|------|------|
| `name` | `"TOML"` |
| `extensions` | `{".toml"}` |
| `supported_modes` | `{TrackingMode::FullSync, TrackingMode::ConfigMerge, TrackingMode::NoSync}` |
| `supports_line_tracking` | `false` |
| `priority` | `80` |