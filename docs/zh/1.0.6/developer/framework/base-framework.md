---
title: 基础框架
description: NeoCore 提供的公共基础设施：路径、日志、取消、错误码、进度与插件加载。
lang: zh
status: complete
updated: 2026-11-27
---

# 基础框架

`NeoCore` 是所有模块/插件的公共基础，只提供接口与轻量工具，不包含业务逻辑。

## 组件

| 组件 | 头文件 | 作用 |
|---|---|---|
| 配置解析接口 | `IConfigParser.h` | 解析/合并配置 |
| 指针接口 | `IPluginPointer.h` | 解析下载地址 |
| 导出器接口 | `IModpackExporter.h` | 打包格式 |
| 编辑器扩展接口 | `IConfigEditorExtension.h` / `IPointerEditorExtension.h` | 编辑器扩展 |
| 插件加载 | `PluginLoader.h` | 扫描/加载 parser DLL |
| 路径 | `path_utf8.h` | UTF-8 路径封装（`fsu8`） |
| 取消 | `cancel_token.h` | 构建/同步取消 |
| 错误码 | `error_codes.h` | 统一错误枚举 |
| 进度 | `IBuildProgress.h` | 构建进度回调 |
| 并行下载 | `parallel_downloader.h` | Range 分片下载器 |

## 日志

- 日志实现位于 `CommonLoggerCPP`（`CLogger`，全局命名空间）；
- 宿主 `CLogger::Init` 后插件共享（`CLogger::Resolve` 回退 spdlog 默认 logger）；
- 插件日志通过导出符号 `SetPluginLogSink` 接入宿主 sink，前缀 `[插件名]`。

## 路径

```cpp
#include <path_utf8.h>
namespace fsu8 = NeoCore::fsu8;

auto p = fsu8::path(u8"配置/文件.json");
if (fsu8::exists(p)) { /* ... */ }
```

- Windows 上禁止 `fs::path(std::string)`（按 ANSI 解释）；
- 显示路径用 `generic_u8string()`（正斜杠）。

## 取消

```cpp
NeoCore::CancelToken token;
token.cancel();
if (token.cancelled()) return;
```

- 构建/同步/下载在循环中检查；
- 取消是协作式的，不强制杀线程。

## 错误码

`error_codes.h` 定义统一错误枚举；CLI 将其映射为退出码与 JSON `error` 字符串。

## 加载

```cpp
NeoCore::PluginLoader loader;
loader.ScanDirectory("parsers");
auto* parser = loader.FindParser("config/a.json");
```

- `ScanDirectory` 配对 `*.dll` 与 `*.meta.json`；
- `FindParser` 按扩展名与优先级选择。

## 相关

- [模块布局](./module-layout.md)
- [ABI](./abi.md)
