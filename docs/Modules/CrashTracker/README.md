# CrashTracker 说明文档

> **⚠️ 2026-08-30 独立仓库化**：本模块已拆分为独立 GitHub 仓库
> `git@github.com:HiBer2007/CrashTracker.git`（协议 LGPL-2.1），主仓库以 **git submodule**
> 引用（见 `.gitmodules`）。源码改动请在其独立仓库内提交后 `git submodule update --remote`；
> 主仓库 clone 须 `--recursive` / `git submodule update --init --recursive`。独立构建见该仓库
> 自包含 CMakeLists（`ct_add_version_info` + `../CA.ico`）。本文档描述模块在项目中的角色与用法。

## 概述

CrashTracker 是 NeoServerUpdateModpack 项目的**崩溃转储分析工具**（独立 GUI EXE，WIN32 子系统），用于展示与分析 `CrashTrackerHandleLib` 在崩溃时生成的报告（`.dmp` / `.trace` / `.meta`）。它解决"崩溃现场如何被人类或自动化脚本读懂"这一问题：GUI 模式下解析 minidump 的异常流、系统信息、模块列表，与 .meta/.trace 合成一份完整可读的崩溃报告（含 "What happened" / "What to do" / Call Stack / Loaded Modules，以及内嵌的 **AI Work Prompt** 辅助段）；CLI 模式下把报告直接输出到 stdout，供服务器、自动化脚本、无图形界面环境使用。

在项目中的角色：**崩溃链路的分析端**。它被 `CrashTrackerHandleLib` 崩溃时以 `CreateProcessA` 拉起（同目录部署），自身也支持独立运行（打开 dump/trace、`--watch` 监控模式）。依赖 Qt6（Core + Widgets）+ dbghelp；**不链接 HandleLib**，两者通过文件协议（`crash-report/` 目录 + 命令行参数）协作。

## 设计目标

- **文件协议解耦**：与 HandleLib 不共享任何代码/头文件，仅通过 `crash-report/` 目录结构与命令行参数协作——HandleLib 只需在同目录放一个 CrashTracker.exe 即可获得分析能力。
- **双模式输出**：GUI 可视化（任务栏闪烁提示、一键复制/导出 TXT）与 CLI 文本输出（`--cli` / `--list` / `--latest`）共用同一套解析与报告生成逻辑。
- **解析自包含**：minidump 解析由本 EXE 用 `MiniDumpReadDumpStream` 直接完成（异常流、系统信息流、模块流），不依赖外部调试工具链；调用栈文本直接读 .trace。
- **可投喂 AI**：报告尾部内嵌 `==== AI Work Prompt ====` 段，指导 AI 协助用户分析崩溃并引导复现/上报。
- **监控模式**：`--watch` 参数支持在宿主程序死亡后自动打开最新 dump（定时轮询信号文件或进程存活状态），用于非异常退出场景的现场收集。

## 模块边界

**做什么**

- 解析 `crash_*.dmp`（`ExceptionStream` / `SystemInfoStream` / `ModuleListStream`，含 CPU 名、RAM、OS 版本、模块列表）与 `crash_*.trace` 调用栈文本。
- 读取同目录 `crash_*.meta`：应用名、排查指南（HelpText）、CRT 断言原文（`===CRT_MESSAGE===`）、崩溃类型描述（`===CRASH_DESCRIPTIONS===`）。
- 合成崩溃报告（`CrashWindow::generateReport()`），GUI 展示 / CLI stdout 输出；CLI 模式额外存档 `crash_<ts>_report.txt` 到报告所在目录。
- 加载 dump 后任务栏闪烁（`FlashWindowEx`）。
- CLI：`--cli <file>` 定向分析、`--cli --list` 列出全部报告、`--cli --latest` 输出最新报告。
- GUI 监控：`--watch=<signalFile>` / `--watch-pid=<pid> --watch-dir=<dir>` 轮询，宿主死亡后自动打开近 120s 内的最新 dump。
- 版本信息注入：`nsum_add_version_info`（"HiBer2007系列软件通用崩溃追踪工具"）。

**不做什么**

- 不参与崩溃捕获/落盘（那是 HandleLib 的职责），自身崩溃也无自恢复逻辑。
- 不做崩溃统计、上报、去重等长期聚合。
- 不做符号化栈回溯（.trace 已由 HandleLib 在崩溃侧生成，本工具只解析文本）；也未内置 dump 上传。

## 依赖关系

| 依赖 | 类型 | 说明 |
|------|------|------|
| `Qt6::Core` | PRIVATE | `QCoreApplication` / `QDir` / `QTimer` / 文件 IO |
| `Qt6::Widgets` | PRIVATE | `CrashWindow`（`QMainWindow`）+ 控件 |
| `dbghelp.lib` | PRIVATE | `MiniDumpReadDumpStream` 解析 minidump |
| `Windows SDK` | 编译期 | `windows.h`、注册表读 CPU 名、`FlashWindowEx` 等 |
| `CrashTrackerHandleLib` | **不链接** | 纯文件协议协作 |

**反向依赖（谁拉起 CrashTracker）**：

| 发起方 | 方式 |
|--------|------|
| `CrashTrackerHandleLib`（宿主崩溃时） | `CreateProcessA` 启动**与宿主 exe 同目录**的 `CrashTracker.exe`；CLI 模式下带 `--cli <dump>` 参数并转发 stdout/stderr 到宿主控制台 |
| 用户/脚本（独立运行） | 直接执行，参数见 usage.md |

**部署依赖**：根工程 `neo_deploy` 目标将 `$<TARGET_FILE:CrashTracker>` 复制到 `deploy/`（与主程序等 EXE 同目录），保证崩溃拉起路径成立；另支持静态 Qt 独立打包（`CrashTracker_ONLY_BUILD` 预设，见 docs/deploy/CrashTracker.md 第十节）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `src/main.cpp` | 入口：CLI 参数解析（`--cli`/`--list`/`--latest`/文件参数）、GUI 参数解析（`--watch=`/`--watch-pid=`/`--watch-dir=`/直接 .dmp）、`scanReports()` 报告扫描、watch 轮询 |
| `src/crash_window.h` | `CrashWindow`（`QMainWindow`）声明 + `CrashInfo` 结构（异常/系统信息/模块/调用栈等字段） |
| `src/crash_window.cpp` | GUI 构建、`parseMinidump`（`MiniDumpReadDumpStream`）、异常名映射、报告生成（含 AI Work Prompt） |
| `src/app.rc` | 版本资源（经 `nsum_add_version_info`） |
| `CMakeLists.txt` | WIN32 EXE 构建描述 |

## 构建集成

- **target 类型**：`add_executable(CrashTracker WIN32 src/main.cpp src/crash_window.cpp src/app.rc)`——GUI 子系统 EXE（无控制台窗口，CLI 输出靠 `AttachConsole`/`AllocConsole`/重定向句柄）。
- **链接**：`target_link_libraries(CrashTracker PRIVATE Qt6::Core Qt6::Widgets dbghelp)`；include 仅 PRIVATE `src`。
- **版本资源**：`nsum_add_version_info(CrashTracker "HiBer2007系列软件通用崩溃追踪工具" "NSUM构建工具")`。
- **构建入口**：根 `CMakeLists.txt` 中 `add_subdirectory(modules/CrashTracker)`（与 HandleLib 一并加入）。
- **部署**：`neo_deploy` 自定义命令把 `CrashTracker.exe` 复制到 `deploy/`；独立发布可参考 `CrashTracker-static` 预设（静态 Qt、单文件、无 Qt DLL，见权威文档）。
- **运行前置**：崩溃报告目录固定为 `QCoreApplication::applicationDirPath()/crash-report`——即 CrashTracker.exe 所在目录的 `crash-report/`。

## 命名空间与公共符号

CrashTracker 是**独立 EXE，非库**：无导出的 C++/DLL 接口，不提供库级公共符号。对外界面是**命令行参数 + GUI**：

| 入口 | 形式 |
|------|------|
| CLI 输出 | `CrashTracker.exe --cli <file.dmp|file.trace|file.txt>` / `--cli --list` / `--cli --latest` |
| GUI 直接分析 | `CrashTracker.exe <file.dmp>`（打开后立即加载并显示） |
| GUI 监控模式 | `--watch=<signalFile>`；`--watch-pid=<pid> --watch-dir=<dir>` |
| GUI 空白窗口 | 无参数（显示带打开对话框的空白窗口） |

内部类 `CrashWindow`（`src/crash_window.h`）提供 `loadDump(path)` / `loadTraceFile(path)` / `generateReport()` / `info()`，供 CLI 路径在无窗口渲染的情况下复用报告生成逻辑。

> 命令行参数语义、返回码与完整示例见 [usage.md](usage.md)；与 HandleLib 的协作协议及输出格式见 [docs/deploy/CrashTracker.md](../../deploy/CrashTracker.md)。