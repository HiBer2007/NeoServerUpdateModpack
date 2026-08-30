# NeoParser_Properties 说明文档

## 概述

NeoParser_Properties 是 NeoServerUpdateModpack 的 Java Properties 风格配置解析器插件（SHARED DLL）。它实现 `NeoCore::IConfigParser` 接口，负责对 `.properties` 文件做**键值条目解析**、**按追踪键合并（partial merge）**与**键枚举**，供 serverconfig 同步与配置文件（如 `server.properties`）的 partial 合并使用。构建产物 `NeoParser_Properties.dll` 与 `NeoParser_Properties.meta.json` 由根 CMakeLists 的 `neo_deploy` 部署到 `deploy/parsers/`，运行时由 `NeoCore::PluginLoader::ScanDirectory` 加载。

## 设计目标

- 覆盖 `.properties` 扩展名（大小写不敏感判定），支持 Java Properties 常见三种键值分隔形式：`=`、`:` 与**空白分隔**（`key value`）。
- 键值条目解析 + 行级键值合并：以本地为基线、仅替换被追踪键所在**整行**（取远端同名键行），注释/空行/行顺序保留——面向 `server.properties` 这类"每行一键"文件。
- 注释规则对齐 Java Properties 习惯：行首 `#`/`!` 为注释；行内 `#` 注释要求前置空白（值首字符除外）。

## 模块边界

- **做**：纯解析/合并逻辑——`parse_entries`、`merge_entries`、`list_keys`、`can_handle`。
- **不做**：不回写文件；不含编辑器 UI（编辑器扩展在 `NeoEditorExtension_Parser_Properties`）；不做行级追踪（`supports_line_tracking = false`，`parse_lines`/`merge_lines` 保持接口默认空实现——行模式请用 `NeoParser_TXT`）。

## 依赖关系

| 依赖 | 类型 | 用途 | 来源 |
|------|------|------|------|
| NeoCore | 静态库 | `IConfigParser` 接口定义 | modules/NeoCore（PRIVATE 链接） |
| spdlog | 第三方库 | 插件日志（经插件日志注册机制接入宿主） | vcpkg，target `spdlog::spdlog` |
| （无第三方解析库） | — | 属性解析为手写实现 | 标准库（`<fstream>/<sstream>/<unordered_set>/<unordered_map>`） |

> 以 `modules/NeoParser_Properties/CMakeLists.txt` 为准：`PRIVATE NeoCore spdlog::spdlog`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库目标定义与依赖链接 |
| `NeoParser_Properties.meta.json` | 插件能力描述（name/version/dll/capability） |
| `src/parser_properties.cpp` | 全部实现：属性行解析（=、:、空白分隔、行内注释）、合并、`PropertiesConfigParser`、`CreateParser`、日志 sink |

## 构建集成

- 目标：`add_library(NeoParser_Properties SHARED src/parser_properties.cpp)`。
- 导出符号（必须带 `__declspec(dllexport)`）：
  - `extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser()`，返回 `new PropertiesConfigParser()`；
  - `NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_Properties")` 追加导出 `SetPluginLogSink(ILogSink*)`。
- 部署：根 CMakeLists `PARSER_TARGETS` 含本目标，`neo_deploy` POST_BUILD 将 DLL 与 meta.json 复制到 `${CMAKE_BINARY_DIR}/deploy/parsers/`。
- 版本资源：`nsum_add_version_info(NeoParser_Properties "NSUM 配置解析器插件 (NeoParser_Properties)" "NSUM构建工具")`。

## 公共符号

| 符号 | 说明 |
|------|------|
| `CreateParser()` | DLL 导出工厂，返回 `NeoCore::IConfigParser*` |
| `PropertiesConfigParser` | 实现类（匿名 namespace，DLL 内部可见） |

运行期 capability（`PropertiesConfigParser::capability()`）：

| 字段 | 值 |
|------|------|
| `name` | `"Properties"` |
| `extensions` | `{".properties"}` |
| `supported_modes` | `{TrackingMode::FullSync, TrackingMode::ConfigMerge, TrackingMode::NoSync}` |
| `supports_line_tracking` | `false` |
| `priority` | `80` |

> 注意：`.properties` 同时被本插件（priority 80）与 `NeoParser_TXT`（priority 60）声明；✅ 2026-08-30 起 `PluginLoader::RegisterParser` 按 `capability().priority` 仲裁（高者胜），本插件（80）更高，`.properties` **稳定归本插件**，不再取决于目录迭代顺序，详见 usage.md 注意事项。