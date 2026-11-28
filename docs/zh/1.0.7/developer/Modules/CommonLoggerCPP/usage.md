---
title: CommonLoggerCPP 使用
description: CLogger 初始化、级别与 sink 控制、控制台持有，以及插件 DLL 的日志注入写法。
lang: zh
status: complete
updated: 2026-11-27
---

# CommonLoggerCPP 使用

## 快速开始

```cpp
#include <logger.h>

int main()
{
    CLogger::HoldConsole();                       // 先持有终端（UTF-8 + VT + 无缓冲）
    CLogger::Init("builder.log", "builder");      // 日志文件 / logger 名，幂等
    CLogger::Info("NSUM start, version={}", NSUM_EXTERNAL_VERSION);   // 版本宏由根 CMake 注入
    CLogger::Warn("fallback to single stream: {}", reason);
    return 0;
}
```

进程内日志文件名与 logger 名的既有约定（照此命名，便于排障时区分）：

| 进程 | `Init` 调用 |
|---|---|
| 主程序 CLI / 构建器 | `CLogger::Init("builder.log", "builder")` |
| 主程序 GUI | `CLogger::Init("gui.log", "gui")` |
| 仓库编辑器 | `CLogger::Init("workspace_editor.log", "editor")` |
| 未指定（库默认值） | `CLogger::Init()` → `core.log` / `core` |

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `CLogger::Init` | `static void Init(const std::string& logFile = "core.log", const std::string& loggerName = "core");` | 建 stdout 彩色 sink + 文件 sink；已初始化则直接返回 |
| `CLogger::Get` | `static std::shared_ptr<spdlog::logger> Get() { return instance_; }` | 取当前 logger（可能为空，不保证已 Init） |
| `CLogger::Resolve` | `static std::shared_ptr<spdlog::logger> Resolve();` | 空指针时回退 `spdlog::default_logger()` 并缓存 |
| `CLogger::HoldConsole` | `static void HoldConsole();` | 幂等：UTF-8 码页、`ENABLE_VIRTUAL_TERMINAL_PROCESSING`、stdout/stderr 二进制无缓冲 |
| `CLogger::SetLevel` | `static void SetLevel(Level level);` | 经 `Resolve()` 生效 |
| `CLogger::AddSink` | `static void AddSink(const std::shared_ptr<spdlog::sinks::sink>& sink);` | 追加自定义 sink（GUI 日志面板等） |
| `CLogger::Trace` | `template<typename... Args> static void Trace(const char* fmt, Args&&... args);` | trace 级 |
| `CLogger::Debug` | `template<typename... Args> static void Debug(const char* fmt, Args&&... args);` | debug 级 |
| `CLogger::Info` | `template<typename... Args> static void Info(const char* fmt, Args&&... args);` | info 级 |
| `CLogger::Warn` | `template<typename... Args> static void Warn(const char* fmt, Args&&... args);` | warn 级 |
| `CLogger::Error` | `template<typename... Args> static void Error(const char* fmt, Args&&... args);` | error 级 |
| `CLogger::Level` | `enum class Level { Trace = 0, Debug, Info, Warn, Error, Off };` | 级别枚举（可直接转 spdlog 等级） |
| `ILogSink::log` | `virtual void log(int level, const std::string& message, const char* pluginName) = 0;` | 插件日志回调（level 0..4） |
| `LoggerLogSink::log` | `void log(int level, const std::string& message, const char* pluginName) override;` | 转发 `CLogger`，前缀 `[插件名]` |
| `NEO_DECLARE_PLUGIN_LOG_SINK` | `#define NEO_DECLARE_PLUGIN_LOG_SINK(PLUGIN_NAME)` | 生成 `SetPluginLogSink` 导出 + `PluginLog` |
| `PluginLog` | `inline void PluginLog(int level, const std::string& msg)` | 宏产物；无 sink 时回退 `CLogger` |

## 典型用法

### 运行期提级排障

`Init` 默认 info 级，可用环境变量提级（不必改代码、不必重编）：

```powershell
$env:NSUM_LOG_LEVEL = "debug"    # trace | debug | info | warn | error | off
```

### 追加 GUI 日志面板 sink

```cpp
class PanelSink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override { /* 喂给面板 */ }
    void flush_() override {}
};

CLogger::AddSink(std::make_shared<PanelSink>());   // Init 之后挂
CLogger::SetLevel(CLogger::Level::Debug);
```

### 插件 DLL 侧：声明日志通道

```cpp
#include <plugin_log_sink.h>

NEO_DECLARE_PLUGIN_LOG_SINK("NeoExporter_MCBBS")   // 文件作用域一次

void doWork()
{
    PluginLog(2, "export started");                // 2 = Info
    PluginLog(4, "cannot write output");           // 4 = Error
}
```

宏展开出 `extern "C" __declspec(dllexport) void SetPluginLogSink(ILogSink*)`：宿主加载插件时统一 `GetProcAddress("SetPluginLogSink")` 注入自己的 `ILogSink`（主程序三处加载点：`PluginLoader::LoadPlugin`、`ModpackExporter::loadExporter`、`PointerDownloader`）。插件没导出该符号时不会报错——`PluginLog` 自动回退 `CLogger`，经 spdlog 共享 `default_logger` 到达宿主日志。

### 宿主侧：按需替换 sink 实现

```cpp
class QtLogSink : public ILogSink {
public:
    void log(int level, const std::string& message, const char* pluginName) override
    {
        emit line(QStringLiteral("[%1] %2")
            .arg(QString::fromUtf8(pluginName ? pluginName : "plugin"),
                 QString::fromStdString(message)));
    }
    // ... signal 声明
};
```

## 注意事项

- **`Init` 必须先于任何日志调用**：`CLogger::Xxx` 先走 `Resolve()`，未 Init 且宿主也未 `set_default_logger` 时 spdlog 会给默认 logger，但日志文件不会生成——CLI/插件场景请照上表显式 `Init`。
- **同名 logger 二次注册是崩溃源**：`Init` 已内置 `if (instance_) return;` 幂等保护，不要绕过它自己 `spdlog::register_logger`。
- **插件 DLL 内不要 `Init`**：静态库形态下 DLL 各有一份 `instance_`，在插件里 Init 只会让插件日志写进另一个文件/与控制台打架；插件应只用 `PluginLog`（或 `CLogger` 回退通道）。
- **`HoldConsole` 只让一个「主人」调用**：它负责 UTF-8 码页与 VT 设置，多处重复调用会互相覆盖状态；约定由进程入口（`main`）调一次。
- **日志文案全英文**：仓库规范要求所有 `CLogger::*` 消息使用英文/ASCII，中文只出现在 GUI 界面文本。
- **文件 sink 是追加模式**：同一份 `*.log` 会持续增长，不自动轮转；排障后自行清理。
- **`AddSink` 不接管所有权语义**：sink 以 `shared_ptr` 传入并追加到 logger 的 sink 列表，宿主需自行保证其生命周期覆盖使用期。

## 相关文档

- [README.md](./README.md)
- [插件契约](../../framework/plugin-contracts.md)
- [扩展与插件注册](../../framework/extension-registry.md)
