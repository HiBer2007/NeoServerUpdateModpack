# NeoParser_SNBT 说明文档

## 概述

NeoParser_SNBT 是 NeoServerUpdateModpack 的 SNBT（Stringified NBT）配置解析器插件（SHARED DLL），主要用于 Minecraft 数据包/存档类 `.snbt` 配置。它实现 `NeoCore::IConfigParser` 接口，负责对 `.snbt` 文件做**读取解析（平面化条目）**、**按追踪键合并（partial merge）**与**键枚举**；其合并过程由内置的 `SnbtPreserving`（保序写入器，`snbt_preserving_writer.cpp`）实现——以本地文件为基线、仅替换被追踪键的值，**保留注释、空行、缩进与结构顺序**，避免整树重序列化破坏可读性。构建产物 `NeoParser_SNBT.dll` 与 `NeoParser_SNBT.meta.json` 由根 CMakeLists 的 `neo_deploy` 部署到 `deploy/parsers/`，运行时由 `NeoCore::PluginLoader::ScanDirectory` 加载。

## 设计目标

- 覆盖 `.snbt` 扩展名；解析基于本地 `nbtcpp` 库（NBT/SNBT 解析 + `to_snbt` 序列化）。
- key-path 平面化：Compound 嵌套以点路径（`a.b`）呈现；叶子值按 NBT 类型加后缀（byte `b`/short `s`/long `L`/float `f`/double `d`，字符串带引号）。
- **保序合并**：区别于 JSON/YAML/TOML 的重序列化式合并，SNBT 的 merge 是**行级保序写入**——逐行遍历本地文件，仅把被追踪的 `key: value` 行的值替换为远端值，注释/空行/结构行/缩进原样保留，最大限度保护手写 SNBT 的可读性与 diff 友好度。

## 模块边界

- **做**：纯解析/合并逻辑——`parse_entries`、`merge_entries`（委托 `SnbtPreserving::merge`）、`list_keys`、`can_handle`。
- **不做**：不回写文件；不含编辑器 UI（编辑器扩展在 `NeoEditorExtension_Parser_SNBT`）；不做行级追踪（`supports_line_tracking = false`，`parse_lines`/`merge_lines` 保持接口默认空实现）；不解析二进制 NBT（.dat）——仅 SNBT 文本。

## 依赖关系

| 依赖 | 类型 | 用途 | 来源 |
|------|------|------|------|
| NeoCore | 静态库 | `IConfigParser` 接口定义 | modules/NeoCore（PRIVATE 链接） |
| spdlog | 第三方库 | 插件日志（经插件日志注册机制接入宿主） | vcpkg，target `spdlog::spdlog` |
| nbtcpp | 本地源码库 | SNBT 解析（`nbtcpp::snbt::parse`）、NBT 标签类型、`to_snbt` 序列化 | 仓库根 `nbtcpp/` 子目录，include `../../nbtcpp/include`，target `nbtcpp` |

> 以 `modules/NeoParser_SNBT/CMakeLists.txt` 为准：`PRIVATE NeoCore spdlog::spdlog nbtcpp`；`nbtcpp` 由根 CMakeLists `add_subdirectory(nbtcpp EXCLUDE_FROM_ALL)` 引入，构建需先 `vcvars64.bat`（否则 nbtcpp/zlib 编译报 C1083 缺 stddef.h/stdio.h）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库目标定义、nbtcpp include 路径与依赖链接（两个源文件） |
| `NeoParser_SNBT.meta.json` | 插件能力描述（name/version/dll/capability） |
| `src/parser_snbt.cpp` | `SnbtConfigParser`：SNBT 解析、平面化、`CreateParser`、日志 sink |
| `src/snbt_preserving_writer.cpp` | `SnbtPreserving` 命名空间：保序合并（`merge`）实现，含行分类/词法化 |

## 构建集成

- 目标：`add_library(NeoParser_SNBT SHARED src/parser_snbt.cpp src/snbt_preserving_writer.cpp)`。
- 导出符号（必须带 `__declspec(dllexport)`）：
  - `extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser()`，返回 `new SnbtConfigParser()`；
  - `NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_SNBT")` 追加导出 `SetPluginLogSink(ILogSink*)`。
- 部署：根 CMakeLists `PARSER_TARGETS` 含本目标，`neo_deploy` POST_BUILD 将 DLL 与 meta.json 复制到 `${CMAKE_BINARY_DIR}/deploy/parsers/`。
- 版本资源：`nsum_add_version_info(NeoParser_SNBT "NSUM 配置解析器插件 (NeoParser_SNBT)" "NSUM构建工具")`。

## 公共符号

| 符号 | 说明 |
|------|------|
| `CreateParser()` | DLL 导出工厂，返回 `NeoCore::IConfigParser*` |
| `SnbtConfigParser` | 实现类（匿名 namespace，DLL 内部可见） |
| `SnbtPreserving::merge(const std::string& localContent, const std::string& remoteContent, const std::vector<std::string>& trackedKeys)` | 保序合并入口（DLL 内部跨 TU 使用） |

运行期 capability（`SnbtConfigParser::capability()`）：

| 字段 | 值 |
|------|------|
| `name` | `"SNBT"` |
| `extensions` | `{".snbt"}` |
| `supported_modes` | `{TrackingMode::FullSync, TrackingMode::ConfigMerge, TrackingMode::NoSync}` |
| `supports_line_tracking` | `false` |
| `priority` | `70` |