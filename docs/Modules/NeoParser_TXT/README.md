# NeoParser_TXT 说明文档

## 概述

NeoParser_TXT 是 NeoServerUpdateModpack 的纯文本配置解析器插件（SHARED DLL），是**唯一支持行级追踪（LineByLine）的配置解析器**。它实现 `NeoCore::IConfigParser` 接口，对 `.txt` / `.properties` / `.cfg` 三类文件提供**键值条目解析（仅 .properties）**与**逐行解析/合并（全部三类）**，供 serverconfig 同步与配置文件的行级 partial 合并使用。构建产物 `NeoParser_TXT.dll` 与 `NeoParser_TXT.meta.json` 由根 CMakeLists 的 `neo_deploy` 部署到 `deploy/parsers/`，运行时由 `NeoCore::PluginLoader::ScanDirectory` 加载。

## 设计目标

- 覆盖 `.txt` / `.properties` / `.cfg`，不依赖第三方解析库（手写实现，标准库即可）。
- **行级追踪**：`parse_lines` 逐行产出 `LineEntry`（1 起始行号），`merge_lines` 按行号把被追踪行整体替换为远端行——适合日志类、无结构化 key 的文本。
- **键值合并**（.properties 语义）：`parse_entries` 解析 `key = value` / `key : value` 行，`merge_entries` 按键把被追踪键的**整行**替换为远端行，注释/空行/顺序保留。

## 模块边界

- **做**：纯解析/合并逻辑——`parse_entries`、`merge_entries`、`parse_lines`、`merge_lines`、`list_keys`、`can_handle`。
- **不做**：不回写文件；不含编辑器 UI（编辑器扩展在 `NeoEditorExtension_Parser_TXT`）；`parse_entries` 仅对 `.properties` 文件产出键值条目（`.txt`/`.cfg` 返回空条目列表，行级能力不受影响）。

## 依赖关系

| 依赖 | 类型 | 用途 | 来源 |
|------|------|------|------|
| NeoCore | 静态库 | `IConfigParser` 接口定义 | modules/NeoCore（PRIVATE 链接） |
| spdlog | 第三方库 | 插件日志（经插件日志注册机制接入宿主） | vcpkg，target `spdlog::spdlog` |
| （无第三方解析库） | — | 文本/属性解析为手写实现 | 标准库（`<fstream>/<sstream>/<unordered_set>/<unordered_map>`） |

> 以 `modules/NeoParser_TXT/CMakeLists.txt` 为准：`PRIVATE NeoCore spdlog::spdlog`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库目标定义与依赖链接 |
| `NeoParser_TXT.meta.json` | 插件能力描述（name/version/dll/capability） |
| `src/parser_txt.cpp` | 全部实现：属性行解析、行级解析/合并、`TxtConfigParser`、`CreateParser`、日志 sink |

## 构建集成

- 目标：`add_library(NeoParser_TXT SHARED src/parser_txt.cpp)`。
- 导出符号（必须带 `__declspec(dllexport)`）：
  - `extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser()`，返回 `new TxtConfigParser()`；
  - `NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_TXT")` 追加导出 `SetPluginLogSink(ILogSink*)`。
- 部署：根 CMakeLists `PARSER_TARGETS` 含本目标，`neo_deploy` POST_BUILD 将 DLL 与 meta.json 复制到 `${CMAKE_BINARY_DIR}/deploy/parsers/`。
- 版本资源：`nsum_add_version_info(NeoParser_TXT "NSUM 配置解析器插件 (NeoParser_TXT)" "NSUM构建工具")`。

## 公共符号

| 符号 | 说明 |
|------|------|
| `CreateParser()` | DLL 导出工厂，返回 `NeoCore::IConfigParser*` |
| `TxtConfigParser` | 实现类（匿名 namespace，DLL 内部可见） |

运行期 capability（`TxtConfigParser::capability()`）：

| 字段 | 值 |
|------|------|
| `name` | `"TXT"` |
| `extensions` | `{".txt", ".properties", ".cfg"}` |
| `supported_modes` | `{TrackingMode::FullSync, TrackingMode::LineByLine, TrackingMode::NoSync}` |
| `supports_line_tracking` | `true` |
| `priority` | `60` |

> 注意：`.properties` 同时被 `NeoParser_TXT`（priority 60）与 `NeoParser_Properties`（priority 80）声明；✅ 2026-08-30 起 `PluginLoader::RegisterParser` 按 `capability().priority` 仲裁（高者胜），`NeoParser_Properties`（80）更高，`.properties` **稳定归 Properties 插件**（本插件保留 `.txt`/`.cfg`），详见 usage.md 注意事项。