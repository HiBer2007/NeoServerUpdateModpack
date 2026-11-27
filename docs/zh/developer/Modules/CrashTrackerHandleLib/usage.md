---
title: CrashTrackerHandleLib 使用
description: 崩溃兜底库的接入顺序、五个配置 API、产物目录结构与排查注意事项。
lang: zh
status: complete
updated: 2026-11-27
---

# CrashTrackerHandleLib 使用

## 快速开始

```cpp
#include <crash_reporter.h>

int main(int argc, char* argv[])
{
    // 1) 最先安装：任何后续初始化崩溃都能被兜住
    HiBerCTM::InstallCrashHandler();          // 空参数 = 当前 exe 目录
    HiBerCTM::InstallCrtReportHook();         // Debug 构建才真正生效
    HiBerCTM::SetCrashAppName("NSUM Editor");
    HiBerCTM::SetCrashHelpText(
        "  1. Check that workspace.json exists and is valid JSON.\n"
        "  2. Git operations require git.exe in PATH or install.conf.\n"
        "  3. Attach the whole crash-report/<timestamp>/ folder when reporting.");

    // 2) 按异常码补充解释（可选，多个可叠加）
    HiBerCTM::AddCrashTypeInfo("STACK_OVERFLOW",
        "Stack overflow: check for circular branch inheritance in the editor.");

    return runApplication(argc, argv);
}
```

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `InstallCrashHandler` | `void InstallCrashHandler(const std::string& dumpDir = "");` | 设置 dump 根目录（空 = exe 目录）、`SetThreadStackGuarantee` 提栈、安装未处理异常过滤器 |
| `InstallCrtReportHook` | `void InstallCrtReportHook();` | 安装 `_CrtSetReportHookW2` 钩子；仅 `_DEBUG` 编译生效 |
| `SetCrashAppName` | `void SetCrashAppName(const std::string& name);` | 应用名，写入 `.meta` 首行 |
| `SetCrashHelpText` | `void SetCrashHelpText(const std::string& text);` | 排查建议，写入 `.meta` |
| `AddCrashTypeInfo` | `void AddCrashTypeInfo(const std::string& name, const std::string& description);` | 追加异常类型说明块 |
| `SetCrashCliMode` | `void SetCrashCliMode(bool enabled);` | `true` = 崩溃后用 `CrashTracker --cli` 输出到当前控制台（不弹窗） |

全部声明在 `namespace HiBerCTM`（`include/crash_reporter.h`）。

## 典型用法

### 控制台 / CLI 程序

```cpp
HiBerCTM::SetCrashCliMode(true);      // 崩溃报告直接打在当前终端
HiBerCTM::InstallCrashHandler();
```

CLI 模式下库会用匿名管道跑 `CrashTracker.exe --cli <dmp>`，并把子进程输出转发到宿主 stdout/stderr——避免 GUI 子系统 EXE 在无控制台时不显示任何内容。

### 指定 dump 目录

```cpp
HiBerCTM::InstallCrashHandler("<安装目录>/crash");   // 报告落到 <安装目录>/crash/crash-report/<ts>/
```

不传参时用 `GetModuleFileNameW` 得到的 exe 目录。注意：**目录必须可写**，装进 `Program Files` 时建议显式指到用户数据目录。

### 排查堆破坏（Debug 专用）

```cpp
#ifdef _DEBUG
HiBerCTM::InstallCrtReportHook();     // 捕获 HEAP_CORRUPTION / _CrtIsValidHeapPointer
#endif
```

Debug 构建的 CRT 断言不经过 `SetUnhandledExceptionFilter`，必须靠这个钩子；它触发时会写同样结构的 `.dmp/.trace/.meta/crash_signal.txt`，然后以 `0xC0000374` 结束进程。

### 崩溃产物结构

```text
<exe 目录>/crash-report/<YYYYMMDD_HHMMSS>/
├── crash_<YYYYMMDD_HHMMSS>.dmp     # MiniDumpNormal
├── crash_<YYYYMMDD_HHMMSS>.trace   # 调用栈文本
├── crash_<YYYYMMDD_HHMMSS>.meta    # 应用名 + 帮助文本 + 异常类型说明
├── crash_signal.txt                # 指向 dmp 与 trace 的路径
└── crash_<YYYYMMDD_HHMMSS>_report.txt   # 由 CrashTracker 生成的可读报告（打开过才有）
```

## 注意事项

- **必须在其他初始化之前调用**：`InstallCrashHandler` 放在 `main` 最前面，否则早期崩溃（Qt 插件加载、配置解析）无法兜住。
- **`CrashTracker.exe` 必须与宿主 EXE 同目录**：库按「当前 exe 目录 + `CrashTracker.exe`」定位工具；找不到时报告仍会落盘，但不会自动打开分析界面。
- **报告写完立即终止进程**：异常路径用真实异常码 `TerminateProcess`，CRT 钩子路径固定 `0xC0000374`。这是设计行为，不要试图改成「继续运行」。
- **CRT 钩子只覆盖 Debug**：Release 下 `InstallCrtReportHook()` 是空实现，`HEAP CORRUPTION` 类问题必须用 Debug 构建复现。
- **钩子内禁止堆分配**：`crash_reporter.cpp` 的 CRT 路径只用栈缓冲与 Win32 API。别在它的回调里加 `std::string`/`new`/iostream——堆已损坏时二次崩溃或自锁。
- **`SetThreadStackGuarantee(64KB)`**：栈溢出场景下保证过滤器还留有栈可用；但极端递归仍可能写不出报告。
- **不要重复安装**：一个进程装一次即可；重复 `SetUnhandledExceptionFilter` 会让前一个过滤器（例如第三方库的）被顶掉。
- **`.meta` 的文案会被原文照抄进报告**：写英文/ASCII 更稳，避免终端代码页把中文显示成乱码。

## 相关文档

- [README.md](./README.md)
- [CrashTracker 说明](../CrashTracker/README.md)
- [CrashTracker 使用](../CrashTracker/usage.md)
- [测试与崩溃冒烟](../../testing.md)
