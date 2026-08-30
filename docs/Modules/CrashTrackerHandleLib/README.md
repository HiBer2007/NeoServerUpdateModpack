# CrashTrackerHandleLib 说明文档

> **⚠️ 2026-08-30 独立仓库化**：本模块已拆分为独立 GitHub 仓库
> `git@github.com:HiBer2007/CrashTrackerHandleLibCPP.git`（协议 LGPL-2.1，含 examples），
> 主仓库以 **git submodule** 引用（见 `.gitmodules`）。源码改动请在其独立仓库内提交后
> `git submodule update --remote`；主仓库 clone 须 `--recursive` /
> `git submodule update --init --recursive`。本文档描述模块在项目中的角色与用法。

## 概述

CrashTrackerHandleLib 是 NeoServerUpdateModpack 项目的**崩溃捕获静态库**（C++17，Windows SDK），对外暴露 `HiBerCTM` 命名空间的 6 个自由函数。它解决"进程崩溃时如何可靠地留下现场证据并自动拉起分析工具"这一问题：通过挂载 SEH 未处理异常过滤器（`SetUnhandledExceptionFilter`）与可选的 CRT 报告钩子（`_CrtSetReportHookW2`），在崩溃瞬间输出 **minidump（.dmp）+ 调用栈追踪（.trace）+ 元数据（.meta）** 到 `<exe目录>/crash-report/<date_time>/`，随后拉起同目录下的 `CrashTracker.exe` 分析工具（GUI 模式弹窗展示，CLI 模式转发报告到当前终端）。

在项目中的角色：**通用崩溃捕获基础设施**，被主程序 `NeoServerUpdateModpackHandler`、`NeoWorkspaceEditor`、`PowerHelper` 三个 EXE 共享链接。它**不依赖 Qt**（头文件仅包含 `<string>`，链接仅 `dbghelp`），可被任何 Windows C++ 程序复用。

## 设计目标

- **零 Qt 依赖**：静态库仅依赖 `dbghelp.lib`（Windows SDK 自带），任何 Windows C++17 项目可直接 `add_subdirectory` + 链接，无框架绑定。
- **免配置接入**：一个 `InstallCrashHandler()` 调用即完成 SEH 挂载、栈空间预留、报告目录初始化；其余 API 均为可选定制。
- **双通道捕获**：普通异常走 SEH 过滤器；`HEAP_CORRUPTION`、`_CrtIsValidHeapPointer` 断言等**不经过** `SetUnhandledExceptionFilter` 的 CRT 堆故障，通过显式启用的 `InstallCrtReportHook()` 绝对抓取。
- **崩溃现场不依赖进程健康**：CRT 钩子路径在堆可能已损坏的前提下工作——钩子内**禁止一切堆分配**，只用栈缓冲 + Win32 API 落盘（见使用文档"注意事项"）。
- **报告可分析**：.dmp 供 CrashTracker 解析异常/系统/模块信息，.trace 是与 .dmp 配套的调用栈文本，.meta 携带应用名、排查指南、崩溃类型描述，全部为简单文本格式。

## 模块边界

**做什么**

- 安装 SEH 未处理异常过滤器（`InstallCrashHandler`，含 64KB 线程栈预留 `SetThreadStackGuarantee`）。
- 可选安装 CRT 报告钩子（`InstallCrtReportHook`，仅 `_DEBUG` 构建生效）。
- 报告落盘：`crash_<ts>.dmp`（`MiniDumpWriteDump`，`MiniDumpNormal`）、`crash_<ts>.trace`（`StackWalk64` + `SymFromAddr`，最多 64 帧）、`crash_<ts>.meta`（AppName / 排查指南 / 崩溃类型描述）、`crash_signal.txt`（记录 dump 与 trace 路径）。
- 崩溃后拉起 `CrashTracker.exe`（`CreateProcessA`，GUI 模式传 dump 路径，CLI 模式传 `--cli` 并转发输出）。
- 崩溃最终以 `TerminateProcess` 强制退出（SEH 用真实异常码，CRT 钩子用 `0xC0000374`）。

**不做什么**

- 不做崩溃报告的解析/展示/分析——那属于 CrashTracker.exe（GUI/CLI）职责。
- 不做报告上传、邮件通知等网络上报。
- 不在进程存活期常驻后台线程/定时器（仅安装过滤器，崩溃时才执行）。
- 不捕捉程序正常退出路径（无 atexit/清理逻辑）。

## 依赖关系

| 依赖 | 类型 | 说明 |
|------|------|------|
| Windows SDK `dbghelp.lib` | PUBLIC 链接（`target_link_libraries(... PUBLIC dbghelp)`） | `MiniDumpWriteDump` / `StackWalk64` / `SymFromAddr` 等符号 |
| `<windows.h>` / `<crtdbg.h>` / `<shellapi.h>` | 编译期 | 实现内部使用（`_WIN32` 保护，MSVC 下 PRIVATE 定义 `_WIN32`） |
| Qt | 无 | 库本身零 Qt 依赖 |

**反向依赖（谁链接 HandleLib）**——以根 CMakeLists.txt 及模块 CMakeLists.txt 为准：

| 链接方 | 位置 |
|--------|------|
| 主程序（`${PROJECT_NAME}` = NeoServerUpdateModpackHandler） | 根 `CMakeLists.txt` `target_link_libraries(... CrashTrackerHandleLib)` |
| NeoWorkspaceEditor | `modules/NeoWorkspaceEditor/CMakeLists.txt`（foreach 链接列表含 CrashTrackerHandleLib） |
| PowerHelper（外壳 EXE） | `modules/PowerHelper/CMakeLists.txt`（PRIVATE 链接 + include 路径） |

**谁拉起 CrashTracker**：`CrashTrackerHandleLib` 内部 `launchCrashTracker()` 在崩溃落盘后用 `CreateProcessA` 启动**与宿主 exe 同目录**的 `CrashTracker.exe`（CLI 模式通过匿名管道捕获其 stdout/stderr 并转发到宿主控制台）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `include/crash_reporter.h` | 公共头文件，`HiBerCTM` 命名空间全部 API 声明（仅包含 `<string>`） |
| `src/crash_reporter.cpp` | 实现：SEH/CRT 钩子、minidump/调用栈/meta 写入、CrashTracker 拉起与管道转发 |
| `CMakeLists.txt` | STATIC 库构建描述 |

## 构建集成

- **target 类型**：`add_library(CrashTrackerHandleLib STATIC src/crash_reporter.cpp)`。
- **include 传播**：`target_include_directories(... PUBLIC include)`，链接方直接 `#include <crash_reporter.h>`。
- **链接传播**：`target_link_libraries(... PUBLIC dbghelp)`，链接方无需自行添加 dbghelp。
- **MSVC 编译定义**：`target_compile_definitions(... PRIVATE _WIN32)`（仅 MSVC）。
- **集成方式**：根 `CMakeLists.txt` 中 `add_subdirectory(modules/CrashTrackerHandleLib)`，宿主 `target_link_libraries(MyApp PRIVATE CrashTrackerHandleLib)`。
- **部署**：HandleLib 是静态库，无独立部署产物；其运行期依赖的 `CrashTracker.exe`（供崩溃时拉起）经根工程 `neo_deploy` 目标复制到 `deploy/`（参见 CrashTracker 模块文档）。崩溃报告默认落在宿主 EXE 所在目录的 `crash-report/` 下。

## 命名空间与公共符号

全部公共符号位于 **`namespace HiBerCTM`**（CTM = CrashTracker Module），共 6 个自由函数，签名逐字摘自 `include/crash_reporter.h`：

| 函数 | 说明 |
|------|------|
| `InstallCrashHandler(const std::string& dumpDir = "")` | 安装 SEH 过滤器，`dumpDir` 为空时回退为宿主 EXE 目录 |
| `InstallCrtReportHook()` | 安装 CRT 报告钩子（`_DEBUG` 下生效，可选） |
| `SetCrashAppName(const std::string& name)` | 设置报告应用名，未设置时回退 EXE 文件名 / `"Crash"` |
| `SetCrashHelpText(const std::string& text)` | 自定义排查指南，显示在报告 "What to do" 段 |
| `AddCrashTypeInfo(const std::string& name, const std::string& description)` | 注册崩溃类型说明，显示在 "What happened" 段 |
| `SetCrashCliMode(bool enabled)` | 置 CLI 模式：崩溃后拉起 `CrashTracker.exe --cli` 而非 GUI |

> 更多实现细节与示例见 [usage.md](usage.md)；与 CrashTracker.exe 的协作关系见 [docs/deploy/CrashTracker.md](../../deploy/CrashTracker.md)。