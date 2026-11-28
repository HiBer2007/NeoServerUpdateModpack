---
title: CrashTracker 使用
description: CrashTracker.exe 的 CLI 与 GUI 用法、报告内容解读、监视模式与排障注意事项。
lang: zh
status: complete
updated: 2026-11-27
---

# CrashTracker 使用

## 快速开始

```powershell
# 看最新一份崩溃报告（在终端直接打印可读文本）
CrashTracker.exe --cli --latest

# 列出全部报告
CrashTracker.exe --cli --list

# 分析指定 dump
CrashTracker.exe --cli .\crash-report\20261127_181530\crash_20261127_181530.dmp
```

GUI 用法：双击 `CrashTracker.exe` 打开窗口 → 选 `*.dmp`；或直接把 `.dmp` 路径当参数传入。

## 公共 API

本工具是 EXE，对外契约是**命令行参数 + 报告文本格式**。

| 参数 | 形式 | 说明 |
|---|---|---|
| `--cli` | `--cli <file.dmp\|file.trace\|file.txt>` | 分析单份文件，报告打印到 stdout 并归档 `_report.txt` |
| `--cli --list` | `--cli --list` | 列出 `<exe>/crash-report/*/crash_*.dmp`，按时间倒序 |
| `--cli --latest` | `--cli --latest` | 取最新一份并输出报告（含 `.meta` 中的 CRT 消息） |
| 裸路径 | `CrashTracker.exe <file.dmp>` | GUI 直接打开该 dump |
| `--watch` | `--watch=<signalFile>` | 监视信号文件：文件出现即读其中路径并打开（每 500 ms 轮询） |
| `--watch-pid` | `--watch-pid=<pid>` | 监视进程：进程退出后扫描最近 120 s 内的 dump 并打开 |
| `--watch-dir` | `--watch-dir=<dir>` | 配合 `--watch-pid` 指定扫描目录 |
| 无参数 | `CrashTracker.exe` | 打开空窗口，手动选择 dump |

退出码：`0` 成功（`--list` 无报告也是 0）；`1` 加载失败或 CLI 用法错误。

## 典型用法

### 崩溃现场取证流程

```powershell
# 1) 程序崩溃后，报告目录结构如下（CrashTrackerHandleLib 写入）
#    <exe>/crash-report/20261127_181530/crash_20261127_181530.dmp
#                                  /crash_20261127_181530.trace
#                                  /crash_20261127_181530.meta
#                                  /crash_signal.txt

# 2) 生成可读报告并归档到同目录
CrashTracker.exe --cli .\crash-report\20261127_181530\crash_20261127_181530.dmp

# 3) 归档文件（脚本可直接读这段文本做归因）
Get-Content .\crash-report\20261127_181530\crash_20261127_181530_report.txt
```

### 报告里能拿到什么

| 区块 | 内容 |
|---|---|
| 异常 | 异常名（如 `ACCESS_VIOLATION`）、异常码、异常地址；trace-only 报告为 `TRACE_ONLY` |
| 系统信息 | CPU 型号等（读注册表 `HARDWARE\DESCRIPTION\System\CentralProcessor\0`） |
| 崩溃模块 | `.meta` 首行的应用名与该次崩溃的模块信息 |
| 模块列表 | minidump 中记录的已加载模块 |
| 调用栈 | `地址 + 模块` 列表（无 PDB 符号，不含源码行号） |
| metadata | `.meta` 的 CRT 消息段（`===CRT_MESSAGE===` 到 `===CRT_HELP===` 之间） |
| 异常解释 | `.meta` 的 `===CRASH_DESCRIPTIONS===` 段（由宿主 `AddCrashTypeInfo` 写入） |

### 让宿主自动拉起分析

宿主侧（`CrashTrackerHandleLib`）安装好处理器后，崩溃时会自动 `CreateProcessA` 拉起同目录 `CrashTracker.exe <dmp>`；如果希望不弹 GUI 而直接把报告打到当前终端，宿主应先调 `HiBerCTM::SetCrashCliMode(true)`（库会以匿名管道跑 `--cli` 并转发输出）。

## 注意事项

- **运行目录决定扫描根**：报告根固定为 `QCoreApplication::applicationDirPath() + "/crash-report"`，所以把 `CrashTracker.exe` 放到别处时会找不到宿主崩溃记录——这也是部署时必须与主程序同目录的原因。
- **没有符号就没有行号**：报告只给地址与模块名。要定位到函数/行，需要用 `dumpbin`/WinDbg 配合对应的 PDB 另行分析。
- **`.meta` 是文本协议**：解析依赖 `===CRT_MESSAGE===` / `===CRT_HELP===` / `===CRASH_DESCRIPTIONS===` 这些分隔行；改 `CrashTrackerHandleLib` 的 `.meta` 写法必须同步本工具（`CrashWindow` 与 `cliShowEntry` 各有一处解析）。
- **CLI 模式下会自动附加控制台**：GUI 子系统 EXE 在无控制台环境启动时会 `AttachConsole(ATTACH_PARENT_PROCESS)`，失败则 `AllocConsole()`；脚本里重定向 stdout 得到的是纯文本报告。
- **`--watch-pid` 只看 120 秒内的 dump**：更早的记录会被忽略，避免误开旧报告。
- **归档报告会覆盖同名文件**：`_report.txt` 按 dump 基名生成，重复分析同一份 dump 会覆盖上一次的归档。
- **不要把它当实时调试器**：它不 attach 进程、不改变执行流；进程终止由崩溃处理器负责。
- **WER/系统弹窗仍可能出现**：若系统策略启用了 Windows 错误报告，`.dmp` 之外的 WER 提示与本工具无关。

## 相关文档

- [README.md](./README.md)
- [CrashTrackerHandleLib 说明](../CrashTrackerHandleLib/README.md)
- [CrashTrackerHandleLib 使用](../CrashTrackerHandleLib/usage.md)
- [测试与崩溃冒烟](../../testing.md)
