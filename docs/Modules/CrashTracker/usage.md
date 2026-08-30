# CrashTracker 使用文档

## 快速开始

CrashTracker 是**独立崩溃分析工具**（GUI EXE），由 `CrashTrackerHandleLib` 在宿主崩溃时自动拉起（同目录部署），也可独立运行。无需任何集成代码——直接执行即可：

```powershell
# 直接分析某份 dump（GUI 窗口展示）
CrashTracker.exe "K:\deploy\crash-report\20260728_133521\crash_20260728_133521.dmp"

# 空白窗口（手动打开 dump/trace）
CrashTracker.exe

# CLI：报告输出到 stdout，不弹 GUI（适合脚本/服务器/无图形环境）
CrashTracker.exe --cli "crash_20260728_133521.dmp"
CrashTracker.exe --cli --list      # 列出全部报告
CrashTracker.exe --cli --latest    # 输出最新一份完整报告
```

**部署要求**：`CrashTracker.exe` 必须与宿主 EXE **同目录**（HandleLib 崩溃时按宿主 exe 路径拼接 `CrashTracker.exe` 查找）；报告扫描目录 = CrashTracker.exe 所在目录的 `crash-report/`。项目内根工程 `neo_deploy` 已把 CrashTracker 复制到 `deploy/`。

## 公共 API

CrashTracker 是 EXE，对外接口为**命令行参数**；内部类 `CrashWindow`（`src/crash_window.h`）的公共方法签名如下（供理解 CLI 实现，不是库接口）：

### 命令行参数（`src/main.cpp`）

| 参数 | 模式 | 行为 |
|------|------|------|
| `--cli <file.dmp|file.trace|file.txt>` | CLI | 加载并分析指定文件，报告写 stdout；同时存档 `crash_<ts>_report.txt` 到文件同目录（`writeReportArchive`）。`.dmp` 走 `loadDump`，其余走 `loadTraceFile` |
| `--cli --list` | CLI | 扫描 `<exe目录>/crash-report/` 全部报告（按时间倒序），打印序号 + 时间戳 + .dmp 路径 |
| `--cli --latest` | CLI | 输出最新一份完整报告（含 .meta 中的 CRT 断言原文），并存档 `_report.txt` |
| `--cli`（无参数） | CLI | 打印 usage 到 stderr，返回 1 |
| `<file.dmp>`（无 `--cli`） | GUI | 立即打开并显示该 dump，进入事件循环 |
| `--watch=<signalFile>` | GUI | 监控模式：每 500ms 轮询信号文件（HandleLib 的 `crash_signal.txt` 格式：首行 dump 路径），读到后加载并删除，自动打开 dump |
| `--watch-pid=<pid>`（配合 `--watch-dir=<dir>`） | GUI | 监控模式：进程死亡后扫描 `--watch-dir` 下最近 120s 内的 `crash_*.dmp` 并打开最新一份 |

> 返回码：**0** = 成功输出报告；**1** = 无法加载 dump/trace 文件，或 `--cli` 缺参数（usage 错误）。

### CrashWindow（`src/crash_window.h`）

```cpp
explicit CrashWindow(QWidget* parent = nullptr);
bool loadDump(const std::string& path);          // 解析 minidump + 关联 .trace/.meta，成功返回 true
bool loadTraceFile(const std::string& path);     // 仅解析 .trace 调用栈文本（异常名固定 TRACE_ONLY）
std::string generateReport() const;              // 生成完整报告文本（含 AI Work Prompt 段）
CrashInfo info() const;                          // 返回解析结果结构
```

### CrashInfo 结构（`src/crash_window.h`）

| 字段 | 类型 | 说明 |
|------|------|------|
| `exceptionName` | `std::string` | 异常名（如 `ACCESS_VIOLATION` / `CPP_EXCEPTION` / `CRT_ASSERT (HEAP_CORRUPTION / invalid heap)`） |
| `exceptionCode` | `std::string` | 十六进制异常码（如 `0xc0000005`） |
| `exceptionAddress` | `uint64_t` | 异常地址 |
| `systemInfo` | `std::string` | CPU 架构/核数、OS 版本、CPU 型号、RAM（来自 minidump SystemInfoStream + 注册表） |
| `crashingModule` | `std::string` | 崩溃模块名（来自 .meta 首行 或模块列表首个模块，去扩展名） |
| `helpText` | `std::string` | "What to do" 排查指南（来自 .meta，宿主 `SetCrashHelpText` 写入） |
| `crtMessage` | `std::string` | CRT 断言原文（来自 .meta `===CRT_MESSAGE===` 段） |
| `crashDescriptions` | `std::map<std::string, std::string>` | 崩溃类型描述表（来自 .meta `===CRASH_DESCRIPTIONS===` 段） |
| `modules` | `std::vector<std::string>` | 已加载模块列表（名称、基址、大小 KB） |
| `callStack` | `std::vector<std::pair<uint64_t, std::string>>` | 调用栈（地址 + 格式化文本，来自 .trace） |

## 典型用法

### 服务器/自动化：CLI 输出并落档

```powershell
# 列全部报告，取编号后定向分析
CrashTracker.exe --cli --list
#   Crash reports (3):
#     1. 20260728_133521  K:\deploy\crash-report\20260728_133521\crash_20260728_133521.dmp
CrashTracker.exe --cli "K:\deploy\crash-report\20260728_133521\crash_20260728_133521.dmp" > report.txt
# 同时会在 dump 同目录生成 crash_20260728_133521_report.txt 存档
```

### 宿主崩溃自动拉起（由 HandleLib 完成，无需人工）

宿主在崩溃时 `CreateProcessA` 启动：GUI 模式 `CrashTracker.exe "<dump路径>"`；CLI 模式（宿主调用了 `SetCrashCliMode(true)`）`CrashTracker.exe --cli "<dump路径>"`，且 HandleLib 通过匿名管道把 CrashTracker 的 stdout/stderr 转发到宿主控制台（最多等 30s），使报告出现在**发起崩溃的同一终端**。

### 监控宿主"非异常退出"的现场

宿主进程未被 SEH 捕获而死亡（如被 kill）时，可预启动 CrashTracker 监控：

```powershell
# 方式一：监听信号文件（HandleLib 写 crash_signal.txt 的场景）
CrashTracker.exe --watch="K:\deploy\crash-report\crash_signal.txt"

# 方式二：监听进程死亡 + 目录
CrashTracker.exe --watch-pid=12345 --watch-dir="K:\deploy\crash-report\20260728_133521"
```

### 报告内容示意（`generateReport()` 输出）

```
---- MyApp Crash Report ----
// Oops. The bits got twisted.

Time: 2026-07-28 13:35:21
Description: ACCESS_VIOLATION at 0x7ff6ebff5e08

What happened:
  A null or dangling pointer was dereferenced.         ← 宿主 AddCrashTypeInfo 定制，否则内置 explainException

System Details:
  CPU Architecture: x64 (AMD/Intel 64-bit)
  ...
What to do:
  1. Check config.json for syntax errors               ← 宿主 SetCrashHelpText 定制，否则内置默认指南
Call Stack:
  #0  MyApp!CrashLabel::tick+0x454 [0x7ff6ebff5e08]
  ...
-- End of Report --

==== AI Work Prompt ====
[Work Prompt]
...（指导 AI 分析崩溃并引导复现/上报）...
[End of Work Prompt]
```

## 注意事项

以下陷阱摘自 AGENTS.md 会话记录与实现代码：

- **GUI 子系统 EXE 的 CLI 输出陷阱**：`CrashTracker.exe` 是 WIN32（GUI 子系统）EXE。CLI 模式下 main() 先检查 `GetStdHandle(STD_OUTPUT_HANDLE)`，无效才 `AttachConsole(ATTACH_PARENT_PROCESS)` / `AllocConsole()` + `freopen_s` 重定向 `CONOUT$`。**经 PowerShell `$var = &` 捕获 stdout 时 console 可能脱离显示空**，应直接控制台运行或用 `Start-Process -RedirectStandardOutput` / cmd `>` 重定向取输出。
- **扫描目录语义**：`scanReports()` 固定扫 `QCoreApplication::applicationDirPath()/crash-report`——即 **CrashTracker.exe 所在目录**的 `crash-report/`。宿主 EXE 与 CrashTracker.exe 必须同目录，报告才互见。
- **信号文件格式**：`--watch=<signalFile>` 读取的是 HandleLib 写入的 `crash_signal.txt` 内容（**两行**：dump 路径、trace 路径），只取首行；读取成功后删除该文件。
- **`--watch-pid` 需配套 `--watch-dir`**：仅当监控的 PID 死亡后，才在 `--watch-dir` 中找最近 **120s** 内修改的 `crash_*.dmp`；无 `--watch-dir` 则无 dump 可扫。
- **报告存档**：CLI 分析（含 `--latest`）会把完整报告另存为 `crash_<ts>_report.txt`，位于**被分析文件同目录**（`completeBaseName() + "_report.txt"`），用于事后回转。
- **静态 Qt 部署**：无 Qt DLL 的环境需用静态编译版（`CrashTracker-static` 预设，单文件 EXE，CLI 模式可用）——见 docs/deploy/CrashTracker.md 第十节。
- **异常名映射**：`exceptionName()` 将异常码映射为名称；`0xC0000409`（STATUS_STACK_BUFFER_OVERRUN，CRT 钩子模拟写入）显示为 `CRT_ASSERT (HEAP_CORRUPTION / invalid heap)`，`0xE06D7363` 显示为 `CPP_EXCEPTION (std::exception)`，未知码显示 `UNKNOWN`。
- **报告目录冒烟测试**：验证崩溃链路时，以 `crash-report/` 目录（子目录）**数量是否新增**为判据；GUI 冒烟须等窗口建好再关（1-2s 就关可能漏检关闭期崩溃）。

## 相关文档

- [docs/deploy/CrashTracker.md](../../deploy/CrashTracker.md) — 崩溃追踪系统完整指南（架构、输出文件格式、CLI 命令、GUI 界面、分发部署、常见问题），本文与其保持一致。
- CrashTrackerHandleLib 模块文档：[README](../CrashTrackerHandleLib/README.md)、[usage](../CrashTrackerHandleLib/usage.md)——崩溃捕获端（本文是分析端）。