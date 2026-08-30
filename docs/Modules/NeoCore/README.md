# NeoCore 说明文档

## 概述

NeoCore 是 NeoServerUpdateModpack 的基础库模块，承载项目插件体系的核心契约与基础设施。它定义插件契约接口（`IConfigParser`、`IPluginPointer`、`IModpackExporter`、`IPointerEditorExtension`、`IConfigEditorExtension`、`IDownloadMethod`）与宿主回调接口（`IBuildProgress`），提供插件加载器（`PluginLoader`，扫描 `parsers/` 目录加载配置解析器 DLL），以及通用工具：取消令牌（`CancelToken`）、统一错误码（`ErrorCode`）、Git 错误诊断（`AnalyzeGitError`）。作为 STATIC 库，NeoCore 被工作区引擎（NeoWorkspace）、构建引擎（NeoBuild）、GUI（GUIWorker）、CLI（NeoCLI）、编辑器（NeoWorkspaceEditor）与全部插件 DLL 共享，是「宿主 ↔ 插件」之间唯一稳定的契约宿主。

## 设计目标

- **插件体系契约宿主**：把插件必须实现、宿主必须回调的接口集中在单一模块，避免接口散落各模块导致的循环依赖或重复定义；插件 DLL 只需依赖 NeoCore（+ 日志）即可接入。
- **接口与实现分离**：NeoCore 只定义接口与纯工具，不放任何具体业务逻辑；具体解析器/指针/导出器由 `NeoParser_*` / `NeoPointer_*` / `NeoExporter_*` / `NeoEditorExtension_*` 插件 DLL 各自实现，宿主可替换、可裁剪。
- **基础能力下沉**：取消检查（`CancelToken`）、错误码（`ErrorCode`）、Git 错误诊断（`AnalyzeGitError`）被多个模块复用，集中于此避免重复实现。
- **契约演进可控**：修改 NeoCore 头文件即修改契约，配合「改共享头文件后 clean-first 全量重建」的工程纪律（见 AGENTS.md），保证宿主与全部插件同步演进。

## 模块边界

**做什么：**

- 定义插件契约：`IConfigParser`、`IPluginPointer`、`IModpackExporter`、`IBuildProgress`、`IDownloadMethod`、`IPointerEditorExtension`、`IConfigEditorExtension` 及关联数据结构（`ParserCapability`、`BuildTarget`、`PointerFileData` 等）。
- 插件加载：`PluginLoader` 扫描目录中的 `.meta.json` → 加载 DLL → `GetProcAddress("CreateParser")` → 建实例 → 按扩展名注册 → `FindParser` 查找。
- 通用基础设施：`CancelToken`（原子取消令牌，header-only）、`ErrorCode`（分组错误码与中文文案）、`AnalyzeGitError`（Git stderr → 中文诊断）。

**不做什么：**

- 不实现任何具体解析器/指针/导出器（由插件 DLL 实现）。
- 不提供日志实现本体（日志已迁移至 CommonLoggerCPP 模块，NeoCore 经 PUBLIC 链接传递暴露其头文件）。
- 不做崩溃上报（属于 CrashTrackerHandleLib / `HiBerCTM`）。
- 不做工作区/Git 执行/构建引擎（属于 NeoWorkspace / NeoBuild）；`AnalyzeGitError` 仅做错误文本诊断，不执行 git。
- 不依赖 GUI 组件库；`IPointerEditorExtension` / `IConfigEditorExtension` 仅在头文件层面引用 `QWidget` / `QJsonObject` 类型。

## 依赖关系

正向依赖（NeoCore → 依赖方），以 `modules/NeoCore/CMakeLists.txt` 的 `target_link_libraries` 为准：

| 依赖 | 类型 | 说明 |
|---|---|---|
| CommonLoggerCPP | STATIC 库（PUBLIC 链接） | 日志系统：`CLogger`、`ILogSink`、`LoggerLogSink`、`plugin_log_sink.h`（`NEO_DECLARE_PLUGIN_LOG_SINK` 宏）。CMake 层面唯一显式依赖，PUBLIC 传递至所有消费方 |
| Qt6（Widgets） | 第三方，头文件层 | `IPointerEditorExtension.h` / `IConfigEditorExtension.h` include `<QWidget>` / `<QJsonObject>`。NeoCore 自身 CMakeLists 未链接 Qt，由消费方提供 Qt include（实际消费方均链接 `Qt6::Core` / `Qt6::Widgets`） |
| nlohmann/json | 第三方，头文件层 | `IModpackExporter.h`、`IPluginPointer.h`、`IConfigEditorExtension.h` include `<nlohmann/json.hpp>`。同样由消费方提供（如 NeoCLI 链接 `nlohmann_json::nlohmann_json`） |

反向依赖（谁依赖 NeoCore，据各模块 CMakeLists）：

| 消费方 | target 类型 | 链接方式 | 主要用途 |
|---|---|---|---|
| NeoWorkspace | STATIC | PUBLIC | `git_operations` 用 `AnalyzeGitError`；`sync_engine` / `file_scanner` / `workspace_manager` 用 `IConfigParser` / `IPluginPointer` / `CancelToken` |
| NeoBuild | STATIC | PUBLIC | `build_engine`、`modpack_exporter`、`pointer_downloader`、`serverconfig_sync`、`sync_policy_executor`、`umd_generator`、`branch_merger` 大量使用契约 |
| GUIWorker | STATIC | PUBLIC | `build_page`（`IBuildProgress` / `CancelToken`）、`config_file_editor`、`pointer_editor`、`editor_extension_registry` 等 |
| NeoCLI | STATIC | PUBLIC | `cli_dispatcher` 使用 `CancelToken` / `PluginLoader` / `IConfigParser` / `IModpackExporter` / `IPluginPointer` / `error_codes` |
| NeoWorkspaceEditor | EXE | PRIVATE | 编辑器可执行 |
| PowerHelper | EXE | PRIVATE | 文档阅读器外壳（另链 CrashTrackerHandleLib） |
| 插件 DLL（`NeoParser_*` / `NeoPointer_*` / `NeoExporter_*` / `NeoEditorExtension_*`） | SHARED | PRIVATE | 实现 NeoCore 接口并导出 `CreateXxx`；另自行链接 spdlog / nlohmann / Qt 以满足头文件编译 |

## 文件组成

`include/` 与 `src/` 文件一览：

| 文件 | 说明 |
|---|---|
| `include/IBuildProgress.h` | 构建进度结构 `BuildProgress` / `BuildResult` 与进度回调接口 `IBuildProgress`（主/子进度条、日志、取消检查） |
| `include/IConfigParser.h` | 配置解析器契约：`TrackingMode` / `ParserCapability` / `ConfigEntry` / `LineEntry` 与 `IConfigParser` |
| `include/IModpackExporter.h` | 导出器契约：`ExportMetadata` / `BuildTarget` 与 `IModpackExporter`（build / export / preview） |
| `include/IPluginPointer.h` | 指针解析器契约：`PointerInfo` / `PointerFileData`（含 JSON 序列化）/ `IPluginPointer` / `IDownloadMethod` |
| `include/IPointerEditorExtension.h` | 指针元数据编辑器扩展接口（Qt 控件，按 resolver 类型维度） |
| `include/IConfigEditorExtension.h` | 配置编辑器扩展接口（Qt 控件，按文件扩展名维度，含 `trackedLines` 行定位） |
| `include/PluginLoader.h` | 解析器插件加载器：`ScanDirectory` / `FindParser` / `ListParsers` / `ParserCount` |
| `include/cancel_token.h` | `CancelToken`（header-only，`std::atomic<bool>`） |
| `include/error_codes.h` | `ErrorCode` 枚举（分组错误码）与 `ErrorCodeToString` |
| `include/git_analyzer.h` | `AnalyzeGitError`：Git stderr → 中文诊断 |
| `src/plugin_loader.cpp` | 插件扫描/加载/注册/查找实现（Windows `LoadLibraryW` / POSIX `dlopen`，UTF-8 路径，`GetProcAddress` 取 `CreateParser` 与可选 `SetPluginLogSink`） |
| `src/cancel_token.cpp` | 占位源文件（`cancel_token` 为 header-only） |
| `src/error_codes.cpp` | `ErrorCodeToString` 中文文案实现 |
| `src/git_analyzer.cpp` | `AnalyzeGitError` 关键字匹配实现 |

## 构建集成

- **CMake target 类型**：`add_library(NeoCore STATIC ...)` — **STATIC 库**，不产出 DLL，代码内嵌进每个链接方；插件 DLL 各自 PRIVATE 链接 → 每份插件各持有一份 NeoCore 代码副本（这是「跨 DLL 日志共享」陷阱的根源，见 usage.md 注意事项）。
- **头文件目录**：`target_include_directories(NeoCore PUBLIC include PRIVATE src)` — 消费方链接 NeoCore 即自动获得 `include/` 下全部头文件，无需再手工加 include 路径（现存部分消费方 CMake 中 PRIVATE 加 include 属历史冗余写法）。
- **库依赖**：`target_link_libraries(NeoCore PUBLIC CommonLoggerCPP)` — PUBLIC 传递，消费方自动获得 `logger.h` / `plugin_log_sink.h`。
- **语言标准**：C++17（由顶层工程设置）；NeoCore 无 `Q_OBJECT` 类，无需 Qt MOC。
- **消费方用法**：`target_link_libraries(<target> PRIVATE/PUBLIC NeoCore)`。
- **契约变更纪律**：修改 NeoCore 头文件（接口签名/类布局）后，必须对引用它的所有 target（含插件 DLL）clean-first 全量重建——否则插件 DLL 链接旧符号报 LNK2019，类布局失配则表现为关闭时 HEAP CORRUPTION（AGENTS.md）。

## 命名空间与公共符号

所有头文件均使用 `namespace NeoCore`（已逐个核实）。公共符号概览：

| 类别 | 符号 |
|---|---|
| 接口 | `IBuildProgress`、`IConfigParser`、`IModpackExporter`、`IPluginPointer`、`IDownloadMethod`、`IPointerEditorExtension`、`IConfigEditorExtension` |
| 工具类 | `PluginLoader`、`CancelToken` |
| 数据结构 | `BuildProgress`、`BuildResult`、`ParserCapability`、`ConfigEntry`、`LineEntry`、`ExportMetadata`、`BuildTarget`、`PointerInfo`、`PointerFileData` |
| 枚举 | `TrackingMode`（`FullSync` / `ConfigMerge` / `LineByLine` / `NoSync`）、`ErrorCode`（0、1001–1005、2001–2003、3001–3003、4001–4002、5001–5003、9000、9999） |
| 工厂函数指针 | `CreateParserFunc`、`CreateExporterFunc`、`CreatePointerFunc`、`CreateDownloaderFunc`、`CreateEditorExtensionFunc`、`CreateConfigEditorFunc` |
| 自由函数 | `ErrorCodeToString(ErrorCode)`、`AnalyzeGitError(const std::string&)` |

各接口/类的完整方法签名见 [usage.md](usage.md)「公共 API」章节。