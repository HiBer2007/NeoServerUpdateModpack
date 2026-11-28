---
title: CommonLoggerCPP usage
description: CLogger initialization, level and sink control, console ownership, and how plugin DLLs inject their log channel.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/Modules/CommonLoggerCPP/usage.md
---

# CommonLoggerCPP usage

## Quick start

```cpp
#include <logger.h>

int main()
{
    CLogger::HoldConsole();                       // take ownership of the terminal first (UTF-8 + VT + unbuffered)
    CLogger::Init("builder.log", "builder");      // log file / logger name, idempotent
    CLogger::Info("NSUM start, version={}", NSUM_EXTERNAL_VERSION);   // version macro injected by the root CMake
    CLogger::Warn("fallback to single stream: {}", reason);
    return 0;
}
```

Existing per-process conventions for the log file name and logger name (follow them so logs are easy to tell apart when debugging):

| Process | `Init` call |
|---|---|
| Main application CLI / builder | `CLogger::Init("builder.log", "builder")` |
| Main application GUI | `CLogger::Init("gui.log", "gui")` |
| Workspace editor | `CLogger::Init("workspace_editor.log", "editor")` |
| Unspecified (library default) | `CLogger::Init()` -> `core.log` / `core` |

## Public API

| API | Signature | Description |
|---|---|---|
| `CLogger::Init` | `static void Init(const std::string& logFile = "core.log", const std::string& loggerName = "core");` | creates the colored stdout sink and the file sink; returns immediately if already initialized |
| `CLogger::Get` | `static std::shared_ptr<spdlog::logger> Get() { return instance_; }` | returns the current logger (may be null; `Init` is not guaranteed to have run) |
| `CLogger::Resolve` | `static std::shared_ptr<spdlog::logger> Resolve();` | falls back to `spdlog::default_logger()` when the pointer is null and caches it |
| `CLogger::HoldConsole` | `static void HoldConsole();` | idempotent: UTF-8 code page, `ENABLE_VIRTUAL_TERMINAL_PROCESSING`, binary unbuffered stdout/stderr |
| `CLogger::SetLevel` | `static void SetLevel(Level level);` | takes effect through `Resolve()` |
| `CLogger::AddSink` | `static void AddSink(const std::shared_ptr<spdlog::sinks::sink>& sink);` | appends a custom sink (GUI log panel and similar) |
| `CLogger::Trace` | `template<typename... Args> static void Trace(const char* fmt, Args&&... args);` | trace level |
| `CLogger::Debug` | `template<typename... Args> static void Debug(const char* fmt, Args&&... args);` | debug level |
| `CLogger::Info` | `template<typename... Args> static void Info(const char* fmt, Args&&... args);` | info level |
| `CLogger::Warn` | `template<typename... Args> static void Warn(const char* fmt, Args&&... args);` | warn level |
| `CLogger::Error` | `template<typename... Args> static void Error(const char* fmt, Args&&... args);` | error level |
| `CLogger::Level` | `enum class Level { Trace = 0, Debug, Info, Warn, Error, Off };` | level enum (converts directly to a spdlog level) |
| `ILogSink::log` | `virtual void log(int level, const std::string& message, const char* pluginName) = 0;` | plugin log callback (level 0..4) |
| `LoggerLogSink::log` | `void log(int level, const std::string& message, const char* pluginName) override;` | forwards to `CLogger` with a `[plugin-name]` prefix |
| `NEO_DECLARE_PLUGIN_LOG_SINK` | `#define NEO_DECLARE_PLUGIN_LOG_SINK(PLUGIN_NAME)` | generates the exported `SetPluginLogSink` plus `PluginLog` |
| `PluginLog` | `inline void PluginLog(int level, const std::string& msg)` | macro product; falls back to `CLogger` when no sink is injected |

## Typical usage

### Raising the level at runtime

`Init` defaults to the info level; the environment variable raises it without editing or rebuilding code:

```powershell
$env:NSUM_LOG_LEVEL = "debug"    # trace | debug | info | warn | error | off
```

### Attaching a GUI log panel sink

```cpp
class PanelSink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override { /* feed the panel */ }
    void flush_() override {}
};

CLogger::AddSink(std::make_shared<PanelSink>());   // attach after Init
CLogger::SetLevel(CLogger::Level::Debug);
```

### Plugin DLL side: declaring the log channel

```cpp
#include <plugin_log_sink.h>

NEO_DECLARE_PLUGIN_LOG_SINK("NeoExporter_MCBBS")   // once at file scope

void doWork()
{
    PluginLog(2, "export started");                // 2 = Info
    PluginLog(4, "cannot write output");           // 4 = Error
}
```

The macro expands to `extern "C" __declspec(dllexport) void SetPluginLogSink(ILogSink*)`: when the host loads the plugin it uniformly calls `GetProcAddress("SetPluginLogSink")` and injects its own `ILogSink` (the three host load points are `PluginLoader::LoadPlugin`, `ModpackExporter::loadExporter` and `PointerDownloader`). A plugin that does not export the symbol does not fail - `PluginLog` automatically falls back to `CLogger`, reaching the host log through spdlog's shared `default_logger`.

### Host side: replacing the sink implementation

```cpp
class QtLogSink : public ILogSink {
public:
    void log(int level, const std::string& message, const char* pluginName) override
    {
        emit line(QStringLiteral("[%1] %2")
            .arg(QString::fromUtf8(pluginName ? pluginName : "plugin"),
                 QString::fromStdString(message)));
    }
    // ... signal declaration
};
```

## Notes

- **`Init` must run before any logging call**: `CLogger::Xxx` goes through `Resolve()`; if neither the host nor the process called `Init`, spdlog hands out its default logger but no log file is created - CLI and plugin scenarios should call `Init` explicitly as in the table above.
- **Registering the same logger name twice is a crash source**: `Init` already contains the `if (instance_) return;` idempotent guard; do not bypass it with your own `spdlog::register_logger`.
- **Do not call `Init` inside a plugin DLL**: in the static library layout every DLL has its own `instance_`, so initializing inside a plugin only sends plugin logs to another file or fights the console; plugins should use `PluginLog` (or the `CLogger` fallback channel).
- **Let only one owner call `HoldConsole`**: it owns the UTF-8 code page and VT setup, and repeated calls from several places overwrite each other's state; the convention is one call in the process entry point (`main`).
- **All log messages are English**: repository rules require every `CLogger::*` message to be English/ASCII; Chinese appears only in GUI text.
- **The file sink is append-only**: the same `*.log` grows indefinitely and is never rotated; clean it up yourself after debugging.
- **`AddSink` does not take over ownership semantics**: the sink is passed as a `shared_ptr` and appended to the logger's sink list, so the host must keep it alive for as long as it is used.

## Related

- [README.md](./README.md)
- [Plugin contracts](../../framework/plugin-contracts.md)
- [Extension and plugin registry](../../framework/extension-registry.md)
