# NeoParser_JSON 说明文档

## 概述

NeoParser_JSON 是 NeoServerUpdateModpack 的 JSON 系列配置解析器插件（SHARED DLL）。它实现 `NeoCore::IConfigParser` 接口，负责对 `.json` / `.json5` / `.jsonc` 三类文件做**读取解析（平面化条目）**、**按追踪键合并（partial merge）**与**键枚举**，为 serverconfig 同步与配置文件的 partial 合并提供能力。构建产物 `NeoParser_JSON.dll` 与 `NeoParser_JSON.meta.json` 由根 CMakeLists 的 `neo_deploy` 部署到 `deploy/parsers/`，运行时由 `NeoCore::PluginLoader::ScanDirectory` 扫描加载，宿主（配置编辑器 `ConfigFileEditor`、serverconfig 同步等）经 `PluginLoader::FindParser` 按扩展名取用。

## 设计目标

- 一份实现覆盖 `.json`/`.json5`/`.jsonc` 三种扩展名：JSON5/JSONC 允许注释，直接兼容常见含注释配置文件。
- key-path 平面化：对象叶子键以点路径（`a.b.c`）、数组元素以下标（`list[0]`）呈现，供 GUI 键列表展示与 partial 键勾选。
- partial 合并以**本地内容为基线**，仅用远端覆盖被追踪键，未追踪内容与本地结构保持不变。

## 模块边界

- **做**：纯解析/合并逻辑——`parse_entries`（文件→条目列表）、`merge_entries`（remote/local 两内容串按 tracked_keys 合并）、`list_keys`（键列表）、`can_handle`（格式判定）。
- **不做**：不回写文件（合并结果由调用方落盘）；不含编辑器 UI 与语法高亮（编辑器扩展在 `NeoEditorExtension_Parser_JSON`）；不做行级追踪（`supports_line_tracking = false`，`parse_lines`/`merge_lines` 保持接口默认空实现）。

## 依赖关系

| 依赖 | 类型 | 用途 | 来源 |
|------|------|------|------|
| NeoCore | 静态库 | `IConfigParser` 接口定义 | modules/NeoCore（PRIVATE 链接） |
| spdlog | 第三方库 | 插件日志（经插件日志注册机制接入宿主） | vcpkg，target `spdlog::spdlog` |
| nlohmann_json | 第三方库 | JSON 解析/序列化 | vcpkg，target `nlohmann_json::nlohmann_json` |

> 依赖以 `modules/NeoParser_JSON/CMakeLists.txt` 为准：`target_link_libraries(... PRIVATE NeoCore spdlog::spdlog nlohmann_json::nlohmann_json)`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库目标定义、include 路径与依赖链接 |
| `NeoParser_JSON.meta.json` | 插件能力描述（name/version/dll/capability） |
| `src/parser_json.cpp` | 全部实现：注释剥离、平面化、合并、`JsonConfigParser`、`CreateParser`、日志 sink |

## 构建集成

- 目标：`add_library(NeoParser_JSON SHARED src/parser_json.cpp)`。
- 导出符号（必须带 `__declspec(dllexport)`，否则 `GetProcAddress` 找不到）：
  - `extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser()`，返回 `new JsonConfigParser()`；
  - `NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_JSON")` 追加导出 `SetPluginLogSink(ILogSink*)`（宿主可选注入日志 sink）。
- meta.json 与 DLL 同放 `deploy/parsers/`：根 CMakeLists 中 `PARSER_TARGETS` 含本目标，`neo_deploy` POST_BUILD 用 `copy_if_different` 把 `$<TARGET_FILE:NeoParser_JSON>` 与 `modules/NeoParser_JSON/NeoParser_JSON.meta.json` 复制到 `${CMAKE_BINARY_DIR}/deploy/parsers/`。
- 版本资源：`nsum_add_version_info(NeoParser_JSON "NSUM 配置解析器插件 (NeoParser_JSON)" "NSUM构建工具")`。

## 公共符号

| 符号 | 说明 |
|------|------|
| `CreateParser()` | DLL 导出工厂，返回 `NeoCore::IConfigParser*` |
| `JsonConfigParser` | 实现类（匿名 namespace，DLL 内部可见） |

运行期 capability（`JsonConfigParser::capability()`）：

| 字段 | 值 |
|------|------|
| `name` | `"JSON"` |
| `extensions` | `{".json", ".json5", ".jsonc"}` |
| `supported_modes` | `{TrackingMode::FullSync, TrackingMode::ConfigMerge, TrackingMode::NoSync}` |
| `supports_line_tracking` | `false` |
| `priority` | `100` |