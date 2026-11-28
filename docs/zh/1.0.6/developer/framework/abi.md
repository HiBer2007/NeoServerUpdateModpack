---
title: ABI
description: 跨 DLL 二进制接口：导出符号、调用约定、STL/Qt 边界、日志共享、结构体布局与版本兼容。
lang: zh
status: complete
updated: 2026-11-27
---

# ABI

插件与宿主运行在同一进程，通过 DLL 边界交互。**跨边界的一切都必须是 ABI 稳定的**。

## 导出符号

```cpp
extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser();
extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer();
extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter();
extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor();
extern "C" __declspec(dllexport) NeoCore::IPointerEditorExtension* CreateEditorExtension();
```

- `extern "C"` 防止名称修饰差异；
- **必须 `__declspec(dllexport)`**，否则 DLL 零导出、`GetProcAddress` 失败（`CreateExporter not found`）；
- 工厂返回接口指针，生命周期归宿主。

## 调用约定

- 默认 `__cdecl`（x64 下统一）；不要在导出函数上使用不一致的调用约定；
- 只传 C 类型/接口指针/`const char*`；跨边界传 `std::string`/`std::vector`/`QString` 需要双方使用**同一编译器/标准库/运行库**，属于高风险。

## 接口内的 C++ 类型

现有接口（`IConfigParser`/`IPluginPointer`/`IModpackExporter`）确实使用 `std::string`/`std::vector`/`nlohmann::json`：

- 前提：宿主与插件由**同一套 MSVC 工具链 + 同一运行库**构建；
- 破坏条件：不同 VS 版本、不同 `/MD`/`/MT`、不同 STL 版本、不同 nlohmann 版本；
- 约定：发布包内的插件与宿主必须同批构建；第三方插件应使用相同工具链。

## 日志跨 DLL

- `CLogger` 在 `CommonLoggerCPP`（STATIC）中，每个 DLL 各有一份静态实例；
- 宿主 Init 后调用 `SetPluginLogSink` 注入 sink；
- 插件通过 `NEO_DECLARE_PLUGIN_LOG_SINK("name")` 导出注册函数；
- 未注入时 `CLogger::Resolve()` 回退 `spdlog::default_logger()`，不会空指针。

## 结构体布局

- 跨 DLL 传递的结构体（`ParserCapability`、`ConfigEntry`、`PointerInfo`、`ExportMetadata`、`BuildTarget`）布局必须一致；
- **修改共享头文件的成员/顺序后，所有 target 必须重建**（含 `new` 该类型的 TU）；
- 布局失配症状：关闭时 `HEAP CORRUPTION 0xC0000374`、随机 `0xC0000005`。

## 版本兼容

| 变化 | 兼容性 |
|---|---|
| 新增虚函数到接口末尾 | 旧插件 vtable 不匹配 → 崩溃/未定义，必须同步升级 |
| 修改枚举值 | 语义不兼容 |
| 修改 meta 字段 | 加载器可容忍未知字段，但新字段旧宿主不识别 |
| 仅插件内部实现变化 | 兼容 |

发布约定：Runtime/插件/编辑器扩展同版本发布；bridge 与 Runtime 通过 CLI 契约（JSON/退出码）交互，不共享 ABI。

## 正反例

```cpp
// 正确：工厂导出 + 接口指针
extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer();

// 错误：缺 dllexport（GetProcAddress 失败）
extern "C" NeoCore::IPluginPointer* CreatePointer();

// 错误：跨边界返回 std::string 的裸指针/引用（生命周期不可控）
```

## 相关

- [插件契约](./plugin-contracts.md)
- [扩展注册表](./extension-registry.md)
