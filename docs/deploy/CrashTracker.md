# CrashTracker 完整指南

崩溃追踪器 — 通用 Windows 崩溃捕获、分析、报告系统（静态库 + 独立工具）。

---

## 目录

1. [架构概览](#一架构概览)
2. [快速接入](#二快速接入)
3. [API 参考](#三api-参考)
4. [输出文件格式](#四输出文件格式)
5. [CLI 命令行](#五cli-命令行)
6. [GUI 使用](#六gui-使用)
7. [崩溃报告格式](#七崩溃报告格式)
8. [自定义排查指南](#八自定义排查指南)
9. [自定义崩溃类型说明](#九自定义崩溃类型说明)
10. [静态 Qt 编译](#十静态-qt-编译)
11. [分发部署](#十一分发部署)
12. [CrashTest 崩溃测试](#十二crashtest-崩溃测试)
13. [常见问题](#十三常见问题)

---

## 一、架构概览

```
┌─────────────────────────────────────────┐
│  你的应用程序                             │
│  #include <crash_reporter.h>             │
│  InstallCrashHandler();                  │
│  SetCrashAppName("MyApp");              │
│  SetCrashHelpText("...");               │
│  AddCrashTypeInfo("EXCEPTION", "...");  │
└──────────┬──────────────────────────────┘
           │ 链接
           ▼
┌─────────────────────────────────────────┐
│  CrashTrackerHandleLib (静态库)          │
│  └── 仅依赖 dbghelp.lib，不依赖 Qt       │
│  ├── SEH 钩子 (UnhandledExceptionFilter) │
│  ├── CRT 钩子 (CrtReportHook, 默认关闭)  │ ← 捕获 HEAP_CORRUPTION / 堆断言
│  ├── MiniDumpWriteDump (.dmp)            │
│  ├── StackWalk64 + SymFromAddr (.trace)  │
│  └── .meta 文件写入                      │
└──────────┬──────────────────────────────┘
           │ 崩溃时启动
           ▼
┌─────────────────────────────────────────┐
│  CrashTracker.exe (独立工具)                │
│  ├── GUI 模式: 可视化报告 + 导出 TXT     │
│  ├── CLI 模式: --cli/--list/--latest     │
│  ├── 任务栏闪烁提示                      │
│  ├── AI Work Prompt 嵌入                 │
│  └── 自定义排查指南 + 崩溃类型说明       │
└─────────────────────────────────────────┘
```

**依赖**: CrashTrackerHandleLib → `dbghelp.lib`（Windows SDK）。CrashTracker.exe → Qt6::Widgets（可静态链接）。

---

## 二、快速接入

### 2.1 CMake 集成

```cmake
add_subdirectory(CrashTrackerHandleLib)
target_link_libraries(MyApp PRIVATE CrashTrackerHandleLib)
```

### 2.2 最小代码

```cpp
#include <crash_reporter.h>

int main() {
    HiBerCTM::InstallCrashHandler();
    HiBerCTM::InstallCrtReportHook();   // 可选: 捕获 HEAP_CORRUPTION 等 CRT 断言
    HiBerCTM::SetCrashAppName("MyApp");
    // 你的代码
}
```

### 2.3 完整接入

```cpp
#include <crash_reporter.h>

int main() {
    HiBerCTM::InstallCrashHandler();
    HiBerCTM::SetCrashAppName("MyApp");

    // （可选）自定义排查指南
    HiBerCTM::SetCrashHelpText(
        "  1. Check config.json for syntax errors\n"
        "  2. Verify that data/ directory exists\n"
        "  3. Restart the application");

    // （可选）自定义崩溃类型说明
    HiBerCTM::AddCrashTypeInfo("ACCESS_VIOLATION",
        "A null or dangling pointer was dereferenced.\n"
        "Check the call stack for the specific module involved.");
    HiBerCTM::AddCrashTypeInfo("CPP_EXCEPTION",
        "An unhandled C++ exception occurred.\n"
        "Check error logs for details.");
    HiBerCTM::AddCrashTypeInfo("MY_CUSTOM_ERROR",
        "Description of my custom error type.");

    // 你的代码
}
```

---

## 三、API 参考

### `InstallCrashHandler`

```cpp
void InstallCrashHandler(const std::string& dumpDir = ".");
```

安装未处理异常过滤器。内部操作：
- `SetThreadStackGuarantee(65536)` — 保留 64KB 栈空间
- `SetUnhandledExceptionFilter` — 挂载 SEH 钩子
- 崩溃输出至 `crash-report/<时间戳>/`

### `InstallCrtReportHook`

```cpp
void InstallCrtReportHook();
```

安装 CRT 报告钩子，**默认不启用，需显式调用**。用于捕获**不会触发 `SetUnhandledExceptionFilter`** 的 CRT 调试堆故障：

- `HEAP CORRUPTION DETECTED: after Normal block`
- `_CrtIsValidHeapPointer(block)` 断言
- `__acrt_first_block == header` 断言
- 其他 `_CRT_ASSERT` / `_CRT_ERROR` 报告

这些故障是 CRT 通过 `_CrtDbgReport` 报告的（Debug 构建 + Debug CRT 堆检查），普通 SEH 过滤器收不到，**必须**走 `_CrtSetReportHookW2` 才能绝对抓取。

实现要点：
- 钩子触发时堆可能已损坏，**禁止任何堆分配**（`new`/STL 容器/`iostream` 全部不可用），一律使用栈缓冲区 + Win32 API（`CreateFileA`/`WriteFile`/`RtlCaptureContext`）写出 minidump
- 输出 `.dmp`（模拟 `STATUS_STACK_BUFFER_OVERRUN` 异常记录，可被 CrashTracker 正常解析）+ `.trace`（`CaptureStackBackTrace` 帧地址）+ `.meta`（含 `===CRT_MESSAGE===` 原始断言文本）+ `crash_signal.txt`
- 返回 `FALSE` 让 CRT 继续默认处理（对话框/abort），报告已在对话框弹出前落盘
- 仅在 `_DEBUG` 构建下生效（Release 无 CRT 堆检查，调用为空操作）

```cpp
int main() {
    HiBerCTM::InstallCrashHandler();
    HiBerCTM::InstallCrtReportHook();   // ← 显式启用 CRT 钩子
    ...
}
```

### `SetCrashAppName`

```cpp
void SetCrashAppName(const std::string& name);
```

设置报告中的应用名称。如未设置，回退为崩溃 EXE 文件名，再回退为 "Crash"。

### `SetCrashHelpText`

```cpp
void SetCrashHelpText(const std::string& text);
```

设置自定义排查指南，显示在报告的 "What to do" 段。支持多行（`\n` 分隔）。

### `AddCrashTypeInfo`

```cpp
void AddCrashTypeInfo(const std::string& name, const std::string& description);
```

为指定异常类型注册自定义 "What happened" 说明。支持任意数量。
- `name` — 异常名称（如 `ACCESS_VIOLATION`, `STACK_OVERFLOW`, 或自定义名称）
- `description` — 多行说明文本

---

## 四、输出文件格式

### 4.1 目录结构

```
crash-report/
  └── 20260728_133521/              ← 时间戳 (YYYYMMDD_HHMMSS)
        ├── crash_20260728_133521.dmp    # Windows minidump
        ├── crash_20260728_133521.trace  # 调用栈追踪
        └── crash_20260728_133521.meta   # 元数据
```

### 4.2 .meta 文件格式

```
MyApp                                    ← Line 1: AppName
===CRT_MESSAGE===                        ← CRT 断言原文 (仅 CRT 钩子路径, 可选)
HEAP CORRUPTION DETECTED: after Normal block (#7937) at 0x...
===CRT_HELP===                           ← HelpText (仅 CRT 钩子路径, 可选)
  1. Check config...
===CRASH_DESCRIPTIONS===                 ← 崩溃类型描述段 (可选)
CRT_ASSERT                                ← 异常名
CRT debug heap detected a problem...     ← 描述文本 (可多行)
===END===                                ← 类型结束标记
```

SEH 路径的 .meta 与旧格式兼容（首行 AppName，其后直接 HelpText）。CRT 钩子路径额外写入 `===CRT_MESSAGE===` 段，保存 CRT 断言对话框的原始文本。

### 4.3 .trace 文件格式

```
<hex_addr>|<module>|<hex_offset>|<symbol>+<hex_disp>
```

---

## 五、CLI 命令行

### 5.1 终端输出模式

```powershell
# 分析指定报告文件
CrashTracker.exe --cli crash_20260728_133521.dmp
CrashTracker.exe --cli trace_file.trace
CrashTracker.exe --cli report.txt

# 列出全部崩溃报告
CrashTracker.exe --cli --list

# 输出最新一份报告
CrashTracker.exe --cli --latest
```

直接将崩溃报告输出到 stdout，不启动 GUI。适用于服务器环境、自动化脚本、无图形界面环境。

- `--list`：扫描 `<exe目录>/crash-report/` 下全部报告目录（按时间倒序），打印时间戳与 .dmp 路径
- `--latest`：输出最新一份报告的完整内容（含 `.meta` 中的 CRT 断言原文）
- 两者也可组合：`--cli --list` 获取编号后，用 `--cli <路径>` 定向分析
- CLI 模式自动挂载父进程控制台（`AttachConsole`），无控制台时分配新控制台，管道/重定向场景下直接使用继承句柄

### 5.2 无 Qt DLL 环境

对于**无 Qt 运行时**的环境，使用**静态 Qt 编译**的 CrashTracker（参见第十节）。静态编译的 CrashTracker 可在 CLI 模式下工作，无需任何 Qt DLL。

### 5.3 返回码

| 码 | 含义 |
|----|------|
| 0 | 成功输出报告 |
| 1 | 无法加载 dump/trace 文件 |

---

## 六、GUI 使用

### 6.1 启动

```powershell
# 直接分析
CrashTracker.exe crash_20260728_133521.dmp

# 空白窗口
CrashTracker.exe
```

崩溃处理器通过 `ShellExecuteExW` 自动启动 GUI 模式。

### 6.2 界面

```
┌──────────────────────────────────────────┐
│  崩溃追踪器 — CrashTracker               │
├──────────────────────────────────────────┤
│  ┌────────────────────────────────────┐  │
│  │         崩溃报告文本                │  │
│  │   (Consolas 9pt, 暗色背景)         │  │
│  └────────────────────────────────────┘  │
│                                          │
│  [打开 Dump] [打开 Trace]                │
│  [复制报告] [导出 TXT]           [退出]  │
└──────────────────────────────────────────┘
```

加载 dump 后自动闪烁任务栏图标。

---

## 七、崩溃报告格式

```
---- MyApp Crash Report ----
// Oops. The bits got twisted.

Time: 2026-07-28 13:35:21
Description: ACCESS_VIOLATION at 0x7ff6ebff5e08

What happened:
  A null or dangling pointer was dereferenced.
  Check the call stack for the specific module involved.    ← 来自 AddCrashTypeInfo

System Details:
  CPU Architecture: x64 (AMD/Intel 64-bit)
  Processor Count: 16
  OS Version: 10.0 Build 26200
  CPU: Intel(R) Core(TM) i7-13700K
  RAM: 32678 MB total

What to do:
  1. Check config.json for syntax errors                      ← 来自 SetCrashHelpText
  2. Verify that data/ directory exists
  3. Restart the application

Call Stack:
  #0  MyApp!CrashLabel::tick+0x454 [0x7ff6ebff5e08]
  ...

-- End of Report --

==== AI Work Prompt ====
[Work Prompt]
...
[End of Work Prompt]
```

---

## 八、自定义排查指南

### 优先级

1. `SetCrashHelpText()` 设置的文本 — **优先使用**
2. 内置通用指南（仅当未设置时使用）

### 主程序示例

```cpp
HiBerCTM::SetCrashHelpText(
    "  1. Verify --repo and --modpack parameters are correct.\n"
    "  2. Check that workspace.json is valid JSON.\n"
    "  3. Ensure plugins are present in deploy/ directory.\n"
    "  4. Check network connectivity.\n"
    "  5. Report bugs with crash-report/ contents attached.");
```

报告头部名称通过 `SetCrashAppName` 设置。

---

## 九、自定义崩溃类型说明

### 概述

`AddCrashTypeInfo(name, description)` 为特定异常类型提供自定义的 "What happened" 说明。

### 已注册类型示例（NSUM 主程序）

| 异常 | 说明 |
|------|------|
| `ACCESS_VIOLATION` | 空指针或野指针 — 检查插件 DLL |
| `STACK_OVERFLOW` | 栈溢出 — 检查分支继承链循环 |
| `CPP_EXCEPTION` | 未捕获异常 — 检查 JSON 格式、网络超时 |

### 扩展类型

```cpp
// 可以注册应用程序特有的错误类型
HiBerCTM::AddCrashTypeInfo("DATABASE_ERROR",
    "Database connection failure.\n"
    "Check connection string and network.");
```

报告中的 "What happened" 段：自定义描述 > 内置 `explainException()`。

---

## 十、静态 Qt 编译

### 10.1 预设

```json
{
    "name": "CrashTracker-static",
    "generator": "Ninja",
    "binaryDir": "${sourceDir}/build_CrashTracker",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_TOOLCHAIN_FILE": "H:/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "CMAKE_PREFIX_PATH": "H:/Qt-static/6.11.1/msvc2022_64",
        "VCPKG_TARGET_TRIPLET": "x64-windows-static",
        "CrashTracker_ONLY_BUILD": "ON"
    }
}
```

### 10.2 构建

```powershell
cmake --preset CrashTracker-static
cmake --build build_CrashTracker --config Release
```

产物为单文件 `CrashTracker.exe`，无需 Qt DLL 即可运行。

---

## 十一、分发部署

### 结构

```
deploy/
  ├── MyApp.exe
  ├── CrashTracker.exe         ← 必须同目录
  ├── Qt6Core.dll           ← 动态链接时需要
  └── ...
```

### 安装器排除

```cmake
if(rel MATCHES "^crash-report/")
    continue()
endif()
```

---

## 十二、CrashTest 崩溃测试

内置隐藏崩溃测试，用于验证 CrashTracker。

### 触发

状态栏版本标签 Ctrl+字母 + 鼠标长按 10 秒。

| 组合键 | 类型 |
|--------|------|
| Ctrl+N + 点击 | nullptr (ACCESS_VIOLATION) |
| Ctrl+S + 点击 | stack (STACK_OVERFLOW) |
| Ctrl+D + 点击 | div0 (INT_DIVIDE_BY_ZERO) |
| Ctrl+T + 点击 | throw (CPP_EXCEPTION) |
| Ctrl+B + 点击 | bkpt (BREAKPOINT) |
| Ctrl+I + 点击 | illegal (ILLEGAL_INSTRUCTION) |
| Ctrl+F + 点击 | heap (HEAP_CORRUPTION) |
| Ctrl+U + 点击 | purecall |
| Ctrl+G + 点击 | guard (GUARD_PAGE) |

CLI: `MyApp.exe --crash-test`

---

## 十三、常见问题

**Q: 崩溃时没有生成 dump？**
检查 `InstallCrashHandler()` 是否在 `main()` 最前调用，以及 EXE 目录写入权限。

**Q: 弹出 HEAP CORRUPTION / _CrtIsValidHeapPointer 断言框但没有 dump？**
此类故障不走 `SetUnhandledExceptionFilter`，必须显式调用 `InstallCrtReportHook()`（`_DEBUG` 构建下生效）才能抓取。

**Q: CRT 钩子默认不启用，会不会漏抓？**
会——这就是默认关闭的原因：CRT 钩子改变了断言流程行为（对话框仍会弹出，但多了一份落盘报告）。确认需要"绝对抓取"的调试场景再启用。

**Q: 钩子触发时程序还能正常写 dump 吗？**
钩子内部严禁堆分配（堆已损坏），全部用栈缓冲 + Win32 API；`MiniDumpWriteDump` 与 `CaptureStackBackTrace` 均不依赖进程堆，可安全执行。

**Q: 调用栈显示 (unknown)？**
`SymInitialize` 搜索路径需包含 PDB 所在目录。Release 构建无调试符号。

**Q: 栈溢出没有 dump？**
已保留 64KB 栈空间处理。极端深度递归仍可能失败。

**Q: CrashTracker.exe 未找到？**
确保与应用程序在同一目录，或使用静态编译版本。

**Q: 如何自定义崩溃类型说明？**
调用 `AddCrashTypeInfo()`，参考第九节。

**Q: 如何在其他项目中使用？**
复制 `CrashTrackerHandleLib/` 目录，`add_subdirectory()` 并链接。

