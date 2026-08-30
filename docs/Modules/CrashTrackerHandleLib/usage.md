# CrashTrackerHandleLib 使用文档

## 快速开始

**步骤 1：CMake 集成**（HandleLib 位于 `modules/CrashTrackerHandleLib/`）

```cmake
add_subdirectory(modules/CrashTrackerHandleLib)
target_link_libraries(MyApp PRIVATE CrashTrackerHandleLib)
```

**步骤 2：main() 最早处安装崩溃处理**

```cpp
#include <crash_reporter.h>

int main() {
    HiBerCTM::InstallCrashHandler();          // 必须最早调用（main 起始处）
    HiBerCTM::InstallCrtReportHook();         // 可选：Debug 构建下抓 HEAP_CORRUPTION 等 CRT 断言
    HiBerCTM::SetCrashAppName("MyApp");
    // ...业务代码
}
```

崩溃发生后，报告写入宿主 EXE 目录 `crash-report/<YYYYMMDD_HHMMSS>/`，并自动拉起同目录 `CrashTracker.exe` 展示（GUI 弹窗）。

## 公共 API

以下签名**逐字摘自** `modules/CrashTrackerHandleLib/include/crash_reporter.h`（`namespace HiBerCTM`）：

| 函数签名 | 说明 |
|----------|------|
| `void InstallCrashHandler(const std::string& dumpDir = "");` | 安装 SEH 未处理异常过滤器。`dumpDir` 空串 → 宿主 EXE 目录；非空 → 以该目录为报告根。内部：`SetThreadStackGuarantee(65536)` 预留 64KB 栈空间 + `SetUnhandledExceptionFilter` |
| `void InstallCrtReportHook();` | 安装 CRT 报告钩子（`_CrtSetReportHookW2`），仅 `_DEBUG` 构建生效；捕获 `_CRT_ASSERT` / `_CRT_ERROR` 报告（HEAP_CORRUPTION、`_CrtIsValidHeapPointer`、`__acrt_first_block` 断言等）。钩子返回 `FALSE`，让 CRT 继续默认处理（对话框/abort） |
| `void SetCrashAppName(const std::string& name);` | 设置报告应用名；未设置时回退为崩溃 EXE 文件名，再回退为 `"Crash"` |
| `void SetCrashHelpText(const std::string& text);` | 设置 "What to do" 排查指南，支持多行（`\n` 分隔） |
| `void AddCrashTypeInfo(const std::string& name, const std::string& description);` | 为指定异常名注册 "What happened" 说明，可多次调用注册任意数量 |
| `void SetCrashCliMode(bool enabled);` | 置 CLI 模式：崩溃后以 `CrashTracker.exe --cli <dump>` 形式拉起并转发输出到当前控制台（最多等待 30s），不再弹 GUI |

### 崩溃报告目录结构

```
<exe目录>/crash-report/
  └── 20260728_133521/              ← 时间戳 (YYYYMMDD_HHMMSS)
        ├── crash_20260728_133521.dmp    # Windows minidump（MiniDumpNormal）
        ├── crash_20260728_133521.trace  # 调用栈 <hex_addr>|<module>|<hex_offset>|<symbol>+<hex_disp>
        ├── crash_20260728_133521.meta   # 首行 AppName，其后 HelpText / ===CRASH_DESCRIPTIONS===
        └── crash_signal.txt             # 两行：dump 路径、trace 路径
```

## 典型用法

### 完整接入（设应用名 + 排查指南 + 崩溃类型说明）

```cpp
#include <crash_reporter.h>

int main() {
    HiBerCTM::InstallCrashHandler();
    HiBerCTM::SetCrashAppName("MyApp");

    // 可选：自定义 "What to do" 排查指南
    HiBerCTM::SetCrashHelpText(
        "  1. Check config.json for syntax errors\n"
        "  2. Verify that data/ directory exists\n"
        "  3. Restart the application");

    // 可选：自定义崩溃类型说明（"What happened" 段，覆盖内置 explainException）
    HiBerCTM::AddCrashTypeInfo("ACCESS_VIOLATION",
        "A null or dangling pointer was dereferenced.\n"
        "Check the call stack for the specific module involved.");
    HiBerCTM::AddCrashTypeInfo("CPP_EXCEPTION",
        "An unhandled C++ exception occurred.\n"
        "Check error logs for details.");

    // ...业务代码
}
```

### CLI 模式（无 GUI 环境 / 自动化脚本）

在 `InstallCrashHandler()` 之前调用 `SetCrashCliMode(true)`，崩溃后报告内容经管道转发到**发起进程的同一终端**（参考主程序 `src/main.cpp`：先 `SetCrashCliMode(true)` 再 `InstallCrashHandler()`）：

```cpp
int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--cli") {
        HiBerCTM::SetCrashCliMode(true);   // CLI 模式：拉起 CrashTracker --cli 而非 GUI
    }
    HiBerCTM::InstallCrashHandler();
    HiBerCTM::SetCrashAppName("MyApp");
    // ...
}
```

### 外部 API 语义补充

- `AddCrashTypeInfo` 的 `name` 使用异常名（如 `ACCESS_VIOLATION`、`STACK_OVERFLOW`、`CPP_EXCEPTION`，或自定义名称）；`description` 为多行说明文本。注册结果写入 .meta 的 `===CRASH_DESCRIPTIONS===` 段（每个类型以 `===END===` 收尾）。
- 崩溃退出码：SEH 路径 `TerminateProcess` 使用**真实异常码**（例如 ACCESS_VIOLATION → `0xC0000005`）；CRT 钩子路径固定 `0xC0000374`（HEAP_CORRUPTION）。

## 注意事项

以下陷阱摘自 AGENTS.md 会话记录与实现代码：

- **CRT 钩子内禁止一切堆分配**：HEAP_CORRUPTION 断言触发时 `_CrtDbgReport` 可能持有堆锁，任何 `new` / STL 容器 / `iostream` / `snprintf`（locale）都会二次崩溃或自锁死锁。钩子内只能使用**栈缓冲 + Win32 API**（`CreateFileA` / `WriteFile` / `RtlCaptureContext` / `CaptureStackBackTrace` / `MiniDumpWriteDump` / `CreateProcessA`）。
- **堆损坏进程中 `ShellExecuteExW` 会死锁**（内部 COM/堆分配），拉起 CrashTracker 必须用 `CreateProcessA`（实现中 `launchCrashTracker` 即如此）。
- **报告写完必须 `TerminateProcess` 强制退出**：SEH 用真实异常码，CRT 钩子用 `0xC0000374`，否则 CRT 弹框/abort 会挂住进程。
- **CRT 钩子路径不能依赖 std::string 全局量**：安装时路径已预拷贝到静态 `g_dumpDirFixed` 缓冲，钩子内只读该缓冲（`g_dumpDir` 的堆缓冲在堆损坏时可能已被破坏）。
- **报告路径语义**：`InstallCrashHandler(dumpDir="")` 空参数 → 报告位于宿主 **exe 目录** `crash-report/`；`dumpDir` 非空 → 以传入目录为报告根。
- **`InstallCrtReportHook` 仅 `_DEBUG` 构建生效**：Release 无 CRT 堆检查，调用为空操作；且它改变了断言流程行为（对话框仍弹出，但多一份落盘报告），按需显式启用。
- **`InstallCrashHandler` 必须在 `main()` 最前调用**：崩溃若发生在过滤器安装之前将无报告；同时确认 EXE 目录可写。
- **调用栈显示 `(unknown)`**：`SymInitialize` 搜索路径需包含 PDB 所在目录；Release 构建无调试符号。
- **栈溢出兜底有限**：已预留 64KB 栈空间，极端深度递归仍可能无法出报告。
- **源码纯 ASCII 约束**：本模块源码不写中文字面量（统一 `\uXXXX` 转义），中文直书在 936 代码页下触发 C4819/C2447（2026-08-04 crash_reporter.cpp 实测）。
- **冒烟测试**：每次类布局/成员变更后，启动 → 等窗口建好 → 关闭，检查 `HEAP CORRUPTION` 标记与 `crash-report` 目录**数量是否新增**（关闭时崩溃属布局失配的典型信号，须对比前后目录数）。

## 相关文档

- [docs/deploy/CrashTracker.md](../../deploy/CrashTracker.md) — 崩溃追踪系统完整指南（快速接入、输出格式、CLI、分发部署、常见问题），本文与其保持一致。
- CrashTracker 模块文档：[CrashTracker README](../CrashTracker/README.md)、[CrashTracker usage](../CrashTracker/usage.md)。
- 模块边界与依赖说明见 [README.md](./README.md)。