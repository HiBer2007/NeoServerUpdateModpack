# CrashTest

NSUM 内置崩溃测试系统，用于验证 CrashTracker 崩溃捕获能力。

## 触发方式

1. 按住 **Ctrl + 字母键** 选择崩溃类型
2. **鼠标长按** 窗口底部状态栏的版本标签（`NSUM v1.0.0` 或 `NSUM Editor v1.0.0`）
3. 保持按住 **10 秒** 不放
4. 倒计时结束后触发崩溃，CrashTracker 自动启动分析

> 任何时候释放鼠标或 Ctrl 键立即取消。

## 崩溃类型

| 快捷键 | ID | 异常码 | 说明 |
|--------|-----|--------|------|
| Ctrl+**N** | nullptr | `ACCESS_VIOLATION` | 空指针解引用 |
| Ctrl+**S** | stack | `STACK_OVERFLOW` | 100000 层深度递归 |
| Ctrl+**D** | div0 | `INT_DIVIDE_BY_ZERO` | 整数除零 |
| Ctrl+**T** | throw | `CPP_EXCEPTION` | 未捕获 `std::runtime_error` |
| Ctrl+**B** | bkpt | `BREAKPOINT` | `__debugbreak()` |
| Ctrl+**I** | illegal | `ILLEGAL_INSTRUCTION` | `__ud2()` |
| Ctrl+**F** | heap | `HEAP_CORRUPTION` | 重复释放同一内存 |
| Ctrl+**U** | purecall | `CPP_EXCEPTION` | 调用纯虚函数 |
| Ctrl+**G** | guard | `GUARD_PAGE` | 访问栈保护页 |

## CLI 崩溃测试

```powershell
NeoServerUpdateModpack.exe --crash-test
```

CLI 测试为固定 nullptr 崩溃，直接触发无需倒计时。

## 栈溢出特殊处理

`STACK_OVERFLOW` 发生时通常没有足够栈空间执行异常处理。NSUM 在 `InstallCrashHandler` 中调用 `SetThreadStackGuarantee(65536)`，保留 64KB 栈空间用于写入 minidump 和 trace 文件。

## 输出文件

崩溃后生成以下文件：

```
crash-report/
  └── YYYYMMDD_HHMMSS/
        ├── crash_YYYYMMDD_HHMMSS.dmp    # minidump
        └── crash_YYYYMMDD_HHMMSS.trace  # 调用栈追踪
```

CrashTracker 自动打开并展示崩溃报告。
