# CommonLoggerCPP 使用文档

## 快速开始

根 `CMakeLists.txt` 已 `add_subdirectory(modules/CommonLoggerCPP)`。宿主 target 链接：

```cmake
# 独立宿主（不经 NeoCore）：
target_link_libraries(MyTarget PRIVATE CommonLoggerCPP)

# 本仓库内：经 NeoCore 传递（NeoCore PUBLIC 链 CommonLoggerCPP），无需显式链接
target_link_libraries(MyTarget PRIVATE NeoCore)
```

源码侧：

```cpp
#include <logger.h>             // CLogger
// 插件场景再包含：
#include <plugin_log_sink.h>    // ILogSink / LoggerLogSink / NEO_DECLARE_PLUGIN_LOG_SINK
```

## 公共 API

### 1. `logger.h` — CLogger

**枚举 `CLogger::Level`：**

| 枚举值 | 值 | 说明 |
|--------|-----|------|
| `Level::Trace` | 0 | 追踪级（`NSUM_LOG_LEVEL=trace` 或 `SetLevel` 启用） |
| `Level::Debug` | 1 | 调试级 |
| `Level::Info` | 2 | 信息级（**默认**） |
| `Level::Warn` | 3 | 警告级 |
| `Level::Error` | 4 | 错误级 |
| `Level::Off` | 5 | 关闭日志 |

数值与 spdlog `level_enum` 对齐（无 Critical 档），`SetLevel` 直接 `static_cast` 转换。

**静态方法：**

| 方法 | 签名 | 说明 |
|------|------|------|
| `Init` | `static void Init(const std::string& logFile = "core.log", const std::string& loggerName = "core")` | 初始化（幂等：`instance_` 非空直接返回）；stdout 彩色 sink + 文件 sink |
| `Get` | `static std::shared_ptr<spdlog::logger> Get()` | 取内部 logger（未初始化时为空） |
| `Resolve` | `static std::shared_ptr<spdlog::logger> Resolve()` | 空指针安全取 logger；`instance_` 为空时回退并缓存 `spdlog::default_logger()` |
| `SetLevel` | `static void SetLevel(Level level)` | 设置日志级别 |
| `AddSink` | `static void AddSink(const std::shared_ptr<spdlog::sinks::sink>& sink)` | 向当前 logger 追加自定义 sink |

**模板方法（spdlog fmt 格式化，`{}` 占位符）：**

| 方法 | 签名 | 说明 |
|------|------|------|
| `Trace` | `template<typename... Args> static void Trace(const char* fmt, Args&&... args)` | 追踪级 |
| `Debug` | 同上 | 调试级 |
| `Info` | 同上 | 信息级 |
| `Warn` | 同上 | 警告级 |
| `Error` | 同上 | 错误级 |

内部统一走 `Resolve()`，logger 为空时静默跳过，不抛异常。

### 2. `plugin_log_sink.h` — 插件日志注册约定

**`class ILogSink`（宿主注入的日志回调接口）：**

| 成员 | 签名 | 说明 |
|------|------|------|
| 析构 | `virtual ~ILogSink() = default;` | 虚析构 |
| `log` | `virtual void log(int level, const std::string& message, const char* pluginName) = 0;` | 日志回调；level 约定 0=Trace 1=Debug 2=Info 3=Warn 4=Error |

**`class LoggerLogSink : public ILogSink`（宿主默认实现）：**

| 成员 | 签名 | 说明 |
|------|------|------|
| `log` | `void log(int level, const std::string& message, const char* pluginName) override` | 转发到 `CLogger`，输出格式 `[{插件名}] {消息}`；`pluginName` 为空时回退 `"plugin"` |

**`#define NEO_DECLARE_PLUGIN_LOG_SINK(PLUGIN_NAME)` 宏**（在插件 DLL 源码中调用一次）展开为：

| 展开符号 | 类型 | 说明 |
|----------|------|------|
| `g_pluginSink` | `static ILogSink*` | 模块内静态指针，保存宿主注入的回调 |
| `SetPluginLogSink` | `extern "C" __declspec(dllexport) void SetPluginLogSink(ILogSink* sink)` | 宿主注入点（`GetProcAddress("SetPluginLogSink")` 查找） |
| `PluginLog` | `inline void PluginLog(int level, const std::string& msg)` | 便捷函数：`g_pluginSink` 非空走富接口（带插件名前缀），为空回退 `CLogger::Xxx` |

> level 约定与 `ILogSink::log` 相同：0=Trace 1=Debug 2=Info 3=Warn 4=Error。

## 典型用法

### 1. 进程入口初始化（按进程命名）

```cpp
#include <logger.h>

int main(int argc, char** argv) {
    // 日志器按进程命名：CLI/构建器 -> "builder"（builder.log）、GUI -> "gui"（gui.log）、
    // 编辑器 -> "editor"（workspace_editor.log）、core 库默认 -> "core"（core.log）
    CLogger::Init("builder.log", "builder");
    CLogger::Info("CLI Mode Started, working dir: {}", workDir);
    return 0;
}
```

### 2. 日志级别控制

```cpp
#include <logger.h>

// 代码内提级：
CLogger::SetLevel(CLogger::Level::Debug);
CLogger::Debug("candidate git at {}", path);

// 或运行前设环境变量提级（无需改代码）：
//   NSUM_LOG_LEVEL=trace|debug|info|warn|error|off
// 默认 info；trace/debug 用于详细日志排障。
```

### 3. 插件 DLL 内注册日志 sink 并输出

```cpp
// 位于插件 DLL 源码（如 NeoParser_JSON.cpp）：
#include <plugin_log_sink.h>
NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_JSON")   // 宏：一次展开全部符号

// 插件内任意位置：
void parse(const std::string& path) {
    PluginLog(2, "parsed config: " + path);      // 宿主注入后输出 "[NeoParser_JSON] parsed config: ..."
    PluginLog(4, "config error at line " + line); // level 4 = Error
}
```

宿主的三个加载点（`PluginLoader::LoadPlugin` / `ModpackExporter::loadExporter` / `PointerDownloader`）已内置注入逻辑：`GetProcAddress("SetPluginLogSink")` 找到则注入静态 `LoggerLogSink`，找不到（旧插件/第三方插件）自动跳过。**插件侧只需要声明宏，无需任何宿主代码**。

### 4. 自研宿主注入插件日志（仅当需要自定义宿主）

```cpp
#include <plugin_log_sink.h>

static LoggerLogSink g_pluginLogSink;   // 或自实现 ILogSink

// 加载插件 DLL 后：
if (auto setSink = reinterpret_cast<void (*)(ILogSink*)>(
        GetProcAddress(handle, "SetPluginLogSink"))) {
    setSink(&g_pluginLogSink);          // 注入；插件此后 PluginLog 走富接口
}
```

### 5. 自定义 sink（如追加 Qt 侧转发）

```cpp
#include <logger.h>
#include <spdlog/sinks/qt_sinks.h>      // spdlog 提供的 Qt sink（可选）

CLogger::Init("gui.log", "gui");
CLogger::AddSink(std::make_shared<spdlog::sinks::qt_sink_mt>());
CLogger::Warn("GUI mode started");      // 同时写入文件、控制台、Qt sink
```

## 注意事项

- **`Init` 幂等**：`instance_` 非空后再次 `Init` 直接返回。日志文件、loggerName、级别均以**首次** `Init` 为准；进程内多次 Init 不会重建。
- **STATIC 库跨 DLL 隔离**：每个插件 DLL 持有独立的 `CLogger::instance_`（插件从不 `Init`，恒为空）。插件日志必须走 `PluginLog` / `CLogger::Resolve` 回退通道——`spdlog::set_default_logger` 在宿主 `Init` 时注入 spdlog 共享 registry，插件才能到达宿主日志。宿主未 `Init` 时，插件日志落到 spdlog 默认 logger。
- **格式串与占位符**：`Trace/Debug/Info/Warn/Error` 首参是 `const char*` 字面量，占位符用 spdlog fmt 语法 `{}`（不是 printf 的 `%d`）。`std::move` 后的对象不得再作为参数读取。
- **日志文件每次启动截断重建（属当前实现现状，非缺陷）**：`Init` 内 `basic_file_sink_mt(logFile, true)` 第二参传 `true`（spdlog truncate 语义）——每次进程启动日志文件清空重写，历史日志不保留；当前设计即按进程会话留存日志、重启重建。
- **自动 flush**：`Init` 时 `flush_on(spdlog::level::info)`，Info 及以上级别自动落盘。
- **级别对应**：`CLogger::Level` 数值与 spdlog `level_enum` 对齐（Trace=0 … Error=4，无 Critical），但 `Level::Off` 值为 5，对应 spdlog **critical** 而非 spdlog `off`(6)——`SetLevel(Off)` 因 CLogger 从不产生 critical 消息而等效静默，若依赖数值语义需注意 [实现观察]。
- **日志消息全英文（项目铁律）**：全仓 `Logger::*` / `PluginLog` / qInfo 等消息必须英文（ANSI 安全），GUI 界面文本才用中文。
- **共享头文件变更后 clean-first 全量重建**：修改 `logger.h` 签名等被多 target 共享的头后，插件 DLL 若不重编会链接旧符号 LNK2019。
- **不要重复实现日志**：新增日志文件/修改 logger 头一律走本模块，勿在其它模块内再造 logger（如历史 `Logger::Init` 二次注册同名 logger 曾触发 `register_logger` 抛 0xE06D7363——现由幂等保护规避）。

## 相关文档

- [README.md](README.md) — 模块说明、设计目标、依赖关系、构建集成
- `AGENTS.md` — 日志全英文要求、日志器按进程命名（builder/gui/editor/core）、跨 DLL 日志共享修复模式
- `PLAN.md` — 功能更新记录表与已知问题表
- `docs/deploy/CrashTracker.md` — 崩溃报告体系（`HiBerCTM`）独立于本日志模块