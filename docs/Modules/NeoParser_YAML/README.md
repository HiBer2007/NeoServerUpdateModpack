# NeoParser_YAML 说明文档

## 概述

NeoParser_YAML 是 NeoServerUpdateModpack 的 YAML 配置解析器插件（SHARED DLL）。它实现 `NeoCore::IConfigParser` 接口，负责对 `.yml` / `.yaml` 文件做**读取解析（平面化条目）**、**按追踪键合并（partial merge）**与**键枚举**，供 serverconfig 同步与配置文件的 partial 合并使用。构建产物 `NeoParser_YAML.dll` 与 `NeoParser_YAML.meta.json` 由根 CMakeLists 的 `neo_deploy` 部署到 `deploy/parsers/`，运行时由 `NeoCore::PluginLoader::ScanDirectory` 加载，宿主经 `PluginLoader::FindParser` 按扩展名取用。

## 设计目标

- 覆盖 `.yml` 与 `.yaml` 两种常见 YAML 扩展名。
- key-path 平面化：嵌套映射以点路径（`a.b`）呈现，便于 GUI 键列表与 partial 键勾选；序列整体作为叶子条目上的 Flow 风格字符串呈现。
- partial 合并以**本地内容为基线**：仅被追踪键取远端值覆写，未追踪内容与本地结构保持不变（重新序列化为块风格 YAML）。

## 模块边界

- **做**：纯解析/合并逻辑——`parse_entries`、`merge_entries`、`list_keys`、`can_handle`。
- **不做**：不回写文件（合并结果由调用方落盘）；不含编辑器 UI（编辑器扩展在 `NeoEditorExtension_Parser_YAML`）；不做行级追踪（`supports_line_tracking = false`）；解析失败时不会保留注释/原排版（yaml-cpp 重序列化）。

## 依赖关系

| 依赖 | 类型 | 用途 | 来源 |
|------|------|------|------|
| NeoCore | 静态库 | `IConfigParser` 接口定义 | modules/NeoCore（PRIVATE 链接） |
| spdlog | 第三方库 | 插件日志（经插件日志注册机制接入宿主） | vcpkg，target `spdlog::spdlog` |
| yaml-cpp | 第三方库 | YAML 解析（`YAML::Load`）与序列化（`YAML::Emitter`） | vcpkg，target `yaml-cpp::yaml-cpp` |

> 以 `modules/NeoParser_YAML/CMakeLists.txt` 为准：`PRIVATE NeoCore spdlog::spdlog yaml-cpp::yaml-cpp`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库目标定义与依赖链接 |
| `NeoParser_YAML.meta.json` | 插件能力描述（name/version/dll/capability） |
| `src/parser_yaml.cpp` | 全部实现：平面化、导航、合并、`YamlConfigParser`、`CreateParser`、日志 sink |

## 构建集成

- 目标：`add_library(NeoParser_YAML SHARED src/parser_yaml.cpp)`。
- 导出符号（必须带 `__declspec(dllexport)`）：
  - `extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser()`，返回 `new YamlConfigParser()`；
  - `NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_YAML")` 追加导出 `SetPluginLogSink(ILogSink*)`。
- 部署：根 CMakeLists `PARSER_TARGETS` 含本目标，`neo_deploy` POST_BUILD 将 DLL 与 meta.json 复制到 `${CMAKE_BINARY_DIR}/deploy/parsers/`。
- 版本资源：`nsum_add_version_info(NeoParser_YAML "NSUM 配置解析器插件 (NeoParser_YAML)" "NSUM构建工具")`。

## 公共符号

| 符号 | 说明 |
|------|------|
| `CreateParser()` | DLL 导出工厂，返回 `NeoCore::IConfigParser*` |
| `YamlConfigParser` | 实现类（匿名 namespace，DLL 内部可见） |

运行期 capability（`YamlConfigParser::capability()`）：

| 字段 | 值 |
|------|------|
| `name` | `"YAML"` |
| `extensions` | `{".yml", ".yaml"}` |
| `supported_modes` | `{TrackingMode::FullSync, TrackingMode::ConfigMerge, TrackingMode::NoSync}` |
| `supports_line_tracking` | `false` |
| `priority` | `90` |