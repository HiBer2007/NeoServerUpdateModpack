---
title: CrashTracker
description: NSUM 的崩溃捕获与离线分析工具：dump/trace 生成、报告目录与使用方式。
lang: zh
status: complete
updated: 2026-11-27
---

# CrashTracker

CrashTracker 负责在进程崩溃时采集现场（dump + trace + meta），并提供离线分析器读取报告。

## 组成

| 组件 | 作用 |
|---|---|
| `CrashTrackerHandleLib` | 静态库：安装 SEH/CRT 钩子、写报告、拉起分析器 |
| `CrashTracker.exe` | 分析器：读取 dump/trace/meta，生成可读报告 |

## 报告目录

```text
<exe>/crash-report/<date_time>/
├─ crash_<ts>.dmp
├─ crash_<ts>.trace
├─ crash_<ts>.meta
└─ crash_signal.txt
```

## 使用

- 主程序/编辑器启动时自动安装钩子；
- 崩溃后自动拉起 `CrashTracker.exe` 分析；
- 手动分析：`CrashTracker.exe <dump/trace/meta>`。

## 钩子行为

- SEH 未处理异常：写真实异常码；
- CRT 报告（HEAP CORRUPTION 等）：写 `0xC0000374`；
- 写完报告后 `TerminateProcess` 强制退出，避免二次崩溃/挂起；
- CRT 钩子内禁止堆分配（只用栈缓冲 + Win32 API）。

## 注意

- 报告默认在 exe 目录，打包发布时不要带入 `crash-report/`；
- 分析需要符号（PDB）才能定位源码行；
- 堆损坏场景下 ShellExecuteEx 可能死锁，已改为 `CreateProcessA`。

## 相关

- [基础库总览](./index.md)
- [模块参考](../../developer/Modules/CrashTracker/README.md)
