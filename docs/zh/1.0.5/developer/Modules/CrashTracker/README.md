---
title: CrashTracker 说明
description: 崩溃报告分析工具（CrashTracker.exe）：解析 minidump/trace 生成可读报告，支持 CLI 与监视模式。
lang: zh
status: complete
updated: 2026-11-27
---

# CrashTracker 说明

## 概述

`CrashTracker` 是仓库自带的崩溃报告分析工具，产物是 `CrashTracker.exe`（无 Qt 之外的外部依赖，链接 `dbghelp`）。它读取 `CrashTrackerHandleLib` 落下的 `crash-report/<时间戳>/` 三件套，解析 minidump 的异常流与系统信息，配合 `.meta` 里的应用名/帮助文本/异常解释，生成一份可读的中文报告。

两种使用姿势：**图形界面**（双击打开后选 dump，或由宿主崩溃时自动拉起）与 **CLI**（`--cli` 直接打印报告，供终端排障与自动化）。

## 设计目标

- **现场可读**：把 minidump 二进制翻译成异常码/异常地址/模块/调用栈/系统信息；
- **无宿主也能用**：可独立启动、用文件对话框打开任意 dump；
- **可脚本化**：`--cli` 输出纯文本报告，并归档为同目录 `crash_<ts>_report.txt`；
- **自动跟随崩溃**：支持「信号文件」与「监视进程 PID」两种触发，宿主崩溃后自动弹窗；
- **无副产物依赖**：报告只在 dump 同目录追加一份 `_report.txt`，不改动 `.dmp/.trace/.meta`。

## 模块边界

**做什么**

- `CrashWindow`：解析 minidump（`MiniDumpReadDumpStream` 读 Exception/SystemInfo 流）、读 `.trace` 文本栈、读 `.meta`（应用名、帮助、CRT 消息、异常解释），`generateReport()` 生成报告文本；
- 启动模式：独立窗口（含"打开 dump"入口）、直接给 dump 路径、`--watch=<signalFile>` / `--watch-pid=<pid>` / `--watch-dir=<dir>` 监视模式；
- CLI 模式（`--cli`）：`<file.dmp|file.trace|file.txt>` 单份报告、`--list` 列 `crash-report/` 下全部报告、`--latest` 取最新一份；
- 报告归档：把生成内容写成 `<dump/trace 基名>_report.txt`。

**不做什么**

- 不安装任何异常钩子（那是 `CrashTrackerHandleLib` 的职责）；
- 不做符号解析/源码行号（无 PDB 下载、无 symbol server），调用栈只给地址与模块；
- 不上传报告、不做统计汇总；
- 不做实时调试器（不能附加断点/继续执行被调试进程）。

## 依赖关系

```mermaid
flowchart LR
  Lib[CrashTrackerHandleLib] -. 写 . .-> Files["crash-report/<ts>/crash_*.dmp/.trace/.meta"]
  CT[CrashTracker.exe] --> Files
  Lib -. 拉起 .-> CT
  CT --> Qt[Qt6::Core / Qt6::Widgets]
  CT --> DbgHelp[dbghelp]
  CT --> Report["crash_<ts>_report.txt / 终端输出"]
```

| 依赖 | 类型 | 用途 |
|---|---|---|
| `Qt6::Core` / `Qt6::Widgets` | Qt（PRIVATE） | `QApplication`、`CrashWindow` 界面、文件列表与扫描 |
| `dbghelp` | 系统库（PRIVATE） | `MiniDumpReadDumpStream` 读取 minidump |
| `crash-report/` 目录 | 运行时输入 | 扫描 `<exe 目录>/crash-report/*/crash_*.dmp` |

## 文件组成

| 文件 | 作用 |
|---|---|
| `src/main.cpp` | 入口：CLI 模式解析（`--cli`/`--list`/`--latest`/文件参数）、GUI 参数（`--watch*`、直接 dump）、监视定时器、报告归档 |
| `src/crash_window.h` | `CrashInfo` 结构与 `CrashWindow` 声明 |
| `src/crash_window.cpp` | minidump/trace 解析、`.meta` 读取、报告生成与界面展示 |
| `src/app.rc` | EXE 资源（图标/版本块） |
| `CMakeLists.txt` | `add_executable(CrashTracker WIN32 ...)`、`ct_add_version_info` |

## 构建集成

- target：`CrashTracker`，**EXE**（`add_executable(CrashTracker WIN32 src/main.cpp src/crash_window.cpp src/app.rc)`），链接 `Qt6::Core`、`Qt6::Widgets`、`dbghelp`（均 PRIVATE）；
- 版本资源：`ct_add_version_info(CrashTracker "HiBer2007系列软件通用崩溃追踪工具" "CrashTracker")`；
- 部署位置：根 `neo_deploy` 把 `$<TARGET_FILE:CrashTracker>` 复制到 `build/deploy/CrashTracker.exe`，与主程序同目录（Qt 运行时 DLL 复用同目录 `windeployqt` 的产物，CrashTracker 自身不单独执行 `windeployqt`）；
- 它是 `neo_deploy` 的依赖项之一（`DEPLOY_DEPS` 列表），因此 `--target neo_deploy` 会一并构建。

## 公共符号概览

本 target 是 EXE，没有供其他模块链接的公共头文件；可复用的只有「报告格式 + 目录约定」。内部符号：

| 符号 | 类型 | 说明 |
|---|---|---|
| `CrashInfo` | struct | 异常名/异常码/异常地址/系统信息/崩溃模块/帮助文本/CRT 消息/异常解释表/模块列表/调用栈 |
| `CrashWindow` | class（QMainWindow） | `loadDump(path)`、`loadTraceFile(path)`、`generateReport()`、`info()` |
| `--cli` | CLI | `CrashTracker.exe --cli <file.dmp\|file.trace>` 打印并归档报告 |
| `--cli --list` / `--cli --latest` | CLI | 列出 `crash-report/` 报告 / 输出最新一份 |
| `--watch` / `--watch-pid` / `--watch-dir` | CLI（GUI 侧） | 监视信号文件 / 监视进程退出 / 监视目录 |

## 相关文档

- [usage.md](./usage.md)
- [CrashTrackerHandleLib 说明](../CrashTrackerHandleLib/README.md)
- [测试与崩溃冒烟](../../testing.md)
