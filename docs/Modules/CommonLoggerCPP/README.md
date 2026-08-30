# CommonLoggerCPP 说明文档

## 概述

CommonLoggerCPP 是项目独立的日志模块（2026-08-07 从 NeoCore 提取），提供统一的静态日志入口 `CLogger`。它基于 spdlog 封装：`CLogger::Init` 一次初始化「控制台（stdout 彩色）+ 文件」双 sink 的 logger，全仓任意位置以 `CLogger::Info("... {}", arg)` 一行完成带格式化参数的日志输出。

模块同时定义**插件日志注册约定**（`plugin_log_sink.h`）：插件 DLL 通过 `NEO_DECLARE_PLUGIN_LOG_SINK("插件名")` 宏一次性展开出 `SetPluginLogSink(ILogSink*)` 导出符号与 `PluginLog(level, msg)` 便捷函数，由宿主（`PluginLoader` / `ModpackExporter::loadExporter` / `PointerDownloader`）经 `GetProcAddress("SetPluginLogSink")` 注入带 `[插件名]` 前缀的日志回调；未注入时自动回退到 spdlog 共享 registry 的 default_logger，保证跨模块日志始终可达。

模块在项目中的角色：为本仓库全部可执行程序、引擎库与插件 DLL 提供唯一统一的日志通道（grep 统计全仓 `.h/.cpp` 中 `CLogger::` 调用约 541 处），并为插件体系提供零侵入的日志接入点。

## 设计目标

- **独立构建单元**：`CommonLoggerCPP` 不依赖项目内任何其它模块，仅依赖 spdlog（vcpkg）与 C++17 标准库，可被任何宿主直接复用。
- **跨 DLL 日志共享**：CommonLoggerCPP 是 STATIC 库，每个插件 DLL 各自持有一份 `CLogger::instance_`（恒为空，插件从不 `Init`）。`CLogger::Resolve()` 在 `instance_` 为空时回退到 spdlog 共享 DLL 全局 registry 的 `default_logger`（宿主 `Init` 后经 `spdlog::set_default_logger` 注入），实现「插件零配置接入宿主日志」。
- **插件可扩展优先**：日志注册走**导出符号**（`SetPluginLogSink`）而非固定接口——新增插件类型零侵入，旧插件找不到符号自动跳过，仍靠 `CLogger` 回退通道输出日志。
- **调用面统一**：类名 `CLogger`、全局命名空间（无 `namespace` 包裹），调用即 `CLogger::Xxx`，全仓数百处调用无需额外前缀。

## 模块边界

**做什么：**

- 日志初始化（`Init`，幂等）、级别控制（`SetLevel`）、追加自定义 sink（`AddSink`）。
- 模板化格式化日志方法 `Trace / Debug / Info / Warn / Error`（spdlog fmt 语法，`{}` 占位符）。
- 空指针安全的 logger 获取（`Resolve`，跨模块回退通道）。
- 插件日志注册约定：`ILogSink` 回调接口、`LoggerLogSink` 默认实现、`NEO_DECLARE_PLUGIN_LOG_SINK` 宏。
- `NSUM_LOG_LEVEL` 环境变量（trace/debug/info/warn/error/off）提级详细日志，用于排障。

**不做什么：**

- 不包含崩溃报告体系（属于 `CrashTrackerHandleLib` / `HiBerCTM` 的 crash_reporter，独立发布）。
- 不提供 GUI 日志面板/日志查看器（GUI 侧由宿主自行消费日志）。
- 不做日志轮转、归档等高级策略（仅控制台 + 单文件双 sink）。
- 不含任何领域业务日志文案（构建/工作区/网络等日志内容由调用方组织）。
- 不依赖 Qt，接口全面使用 `std::string` 与 spdlog 类型。

## 依赖关系

| 依赖 | 类型 | 说明 |
|------|------|------|
| spdlog (`spdlog::spdlog`) | 第三方库（vcpkg，PUBLIC） | 日志后端（logger/sink/format） |
| C++17 标准库 | 语言 | `<memory>` / `<string>` 等 |
| Qt | 无 | 本模块不依赖 Qt |
| 宿主进程 | 运行时 | 主程序 / NeoWorkspaceEditor 负责 `CLogger::Init` 并注入 default_logger |

**反向依赖：**

| 消费者 | 链接方式 | 用途 |
|--------|----------|------|
| NeoCore | `PUBLIC` 链接 CommonLoggerCPP | 传递链：全仓 500+ 调用点经 NeoCore 获得 CLogger（`NeoCore/CMakeLists.txt:13`） |
| NeoWorkspace / NeoBuild / GUIWorker / NeoCLI / NeoWorkspaceEditor / 主程序 `src/main.cpp` | 经 NeoCore 传递 | 各引擎与可执行程序的日志输出 |
| 11 个插件 DLL（NeoParser_JSON / YAML / TOML / SNBT / TXT / Properties、NeoPointer_Modrinth / DirectURL、NeoExporter_MCBBS / Modrinth / HMCL） | 经 NeoCore(PRIVATE) 传递 | `NEO_DECLARE_PLUGIN_LOG_SINK("插件名")` + `PluginLog` |

## 文件组成

| 文件 | 说明 |
|------|------|
| `include/logger.h` | `CLogger` 声明：`Level` 枚举、静态方法 `Init/Get/Resolve/SetLevel/AddSink`、模板方法 `Trace/Debug/Info/Warn/Error` |
| `include/plugin_log_sink.h` | 插件日志注册约定：`ILogSink` 接口、`LoggerLogSink` 默认实现、`NEO_DECLARE_PLUGIN_LOG_SINK` 宏 |
| `src/logger.cpp` | `CLogger` 实现：`Init`（幂等）、`Resolve`（回退 default_logger）、`SetLevel`、`AddSink` |
| `CMakeLists.txt` | STATIC target 定义：`PUBLIC include` + `PRIVATE src`，`PUBLIC spdlog::spdlog` |

## 构建集成

- CMake target：`CommonLoggerCPP`，类型 **STATIC**（`add_library(CommonLoggerCPP STATIC ...)`）。
- 根 `CMakeLists.txt:92` 已 `add_subdirectory(modules/CommonLoggerCPP)`。
- 本仓库内宿主一般**无需显式链接**：NeoCore 以 `PUBLIC` 链接 CommonLoggerCPP，全仓传递（含插件 DLL 经 NeoCore(PRIVATE) 传递）。
- 独立宿主（不经过 NeoCore）直接：

  ```cmake
  target_link_libraries(YourTarget PRIVATE CommonLoggerCPP)
  ```

- 头文件路径通过 `target_include_directories(CommonLoggerCPP PUBLIC include)` 对外暴露，`#include <logger.h>` / `#include <plugin_log_sink.h>` 即可。

## 命名空间与公共符号

模块**不引入命名空间**（全局命名空间）。公共符号概览：

| 符号 | 类别 | 说明 |
|------|------|------|
| `CLogger` | class | 静态日志门面 |
| `CLogger::Level` | enum class | `Trace=0, Debug, Info, Warn, Error, Off`（与 spdlog level_enum 数值对齐） |
| `CLogger::Init / Get / Resolve / SetLevel / AddSink` | 静态方法 | 初始化与配置 |
| `CLogger::Trace / Debug / Info / Warn / Error` | 模板静态方法 | 格式化日志输出 |
| `ILogSink` | 抽象类 | 宿主注入的日志回调接口：`virtual void log(int level, const std::string& message, const char* pluginName) = 0;` |
| `LoggerLogSink` | class | `ILogSink` 默认实现，转发到 `CLogger`（带 `[插件名]` 前缀） |
| `NEO_DECLARE_PLUGIN_LOG_SINK(PLUGIN_NAME)` | 宏 | 展开 `g_pluginSink` 静态 + `extern "C" __declspec(dllexport) void SetPluginLogSink(ILogSink*)` + `inline void PluginLog(int level, const std::string& msg)` |

详细签名与用法见 [usage.md](usage.md)。

> 注：`CMakeLists.txt` 内注释「命名空间沿用 NeoCore」与实际头文件（全局命名空间、类名 `CLogger`）不一致，应为历史遗留描述，文档以实际头文件为准。