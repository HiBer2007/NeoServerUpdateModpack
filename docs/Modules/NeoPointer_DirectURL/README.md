# NeoPointer_DirectURL 说明文档

## 概述

`NeoPointer_DirectURL` 是 NeoServerUpdateModpack 的**指针解析器插件**（Pointer Resolver）：实现 `NeoCore::IPluginPointer` 接口，**直接使用指针 metadata 中的 `url` 字段**作为下载地址，并提供 SHA-256 完整性校验。在项目中与 `NeoPointer_Modrinth` 并列，被 `NeoBuild::PointerDownloader` 从 `pointers/` 目录扫描加载，覆盖「已有直链、无需平台 API」的下载场景。

- 读取 Meta：`NeoPointer_DirectURL.meta.json` 声明 `resolver: "direct_url"`。
- 能力集（meta.json `features`）：`resolve_url`、`validate`。
- 实现类：`DirectUrlPointer`（匿名命名空间内），导出工厂函数 `CreatePointer`。

## 设计目标

- **零 API 依赖**：不调用任何平台接口，`resolve_url` 只做 metadata 字段存在性与非空校验后原样返回 `url`。
- **通用兜底**：作为 Modrinth 之外的通用通道，支持任意可直链下载的资源（CDN、对象存储、镜像站等）。
- **零信任完整性**：`validate` 提供与 `NeoPointer_Modrinth` 完全一致的 SHA-256 流式校验实现，配合 `PointerDownloader` 的缓存复用校验形成双重保障。
- **实现最简**：接口最小实现（`name`/`can_handle`/`resolve_url`/`validate`），无网络、无 JSON 解析分支，出错显式打日志返回空串。

## 模块边界

**做什么**

- 校验 `metadata.url` 存在且为字符串、非空。
- 返回 `metadata.url` 原文作为下载地址（**不校验 URL 格式**，不做重定向/可达性探测）。
- 对已下载文件做 SHA-256 流式校验。

**不做什么**

- **不发 HTTP 请求**：解析与下载分离，写盘/缓存/重定向处理全部由 `PointerDownloader::downloadToPath` 完成。
- **不解析 `.pointer` 文件**：由 `BuildEngine::stepProcessFiles` 读取占位符 JSON 构造 `PointerInfo`。
- **无平台语义**：不依赖任何第三方 API、无鉴权、无配额逻辑。
- **不实现扩展能力**：`supported_download_methods()`/`can_batch_search()`/`batch_search(...)` 均走接口默认（`{}` / `false` / `{}`），meta `features` 也未声明这些能力。

## 依赖关系

依赖以 `modules/NeoPointer_DirectURL/CMakeLists.txt` 为准（均为 `PRIVATE`）：

| 依赖 | 类型 | 用途 |
|------|------|------|
| `NeoCore` | 静态库 | `IPluginPointer`/`PointerInfo` 接口（`NeoCore/include/IPluginPointer.h`）；经其 PUBLIC 链传递 `CommonLoggerCPP` 的 `CLogger`/`plugin_log_sink.h` |
| `spdlog::spdlog` | vcpkg | 日志（源码实际经 `CLogger` 输出） |
| `Qt6::Core` | Qt6 | `QCryptographicHash`/`QFile`/`QFileInfo`/`QIODevice` |
| `Qt6::Network` | Qt6 | 已链接但**当前源码未使用**（源码无 `QNetwork*` 引用；如保持链接一致可保留，属冗余依赖） |

头文件依赖：`IPluginPointer.h`（NeoCore）、`logger.h`、`plugin_log_sink.h`（CommonLoggerCPP，经 NeoCore 传递）、`nlohmann/json.hpp`（经 NeoCore 传递）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库定义、include 目录、链接项 |
| `NeoPointer_DirectURL.meta.json` | 插件元数据：名称/版本/DLL 名/`resolver`/描述/`features` |
| `src/pointer_directurl.cpp` | `DirectUrlPointer` 实现 + `CreatePointer` 导出 + 日志注册宏 |

## 构建集成

- 目标类型：`add_library(NeoPointer_DirectURL SHARED ...)` → 产出 `NeoPointer_DirectURL.dll`。
- 导出符号（`extern "C" __declspec(dllexport)`，必须带 `__declspec`，否则 `GetProcAddress` 找不到）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer() {
      return new DirectUrlPointer();
  }
  ```

- 日志注册（可选演进约定）：`NEO_DECLARE_PLUGIN_LOG_SINK("NeoPointer_DirectURL")` 展开出 `SetPluginLogSink(ILogSink*)` 导出，宿主 `PointerDownloader::loadResolverDLL` 用 `GetProcAddress` 注入；旧插件无此符号时自动跳过。
- `.meta.json` 与 DLL 一同由根 `CMakeLists.txt` 部署：`POINTER_TARGETS` 含 `NeoPointer_DirectURL`，`neo_deploy` POST_BUILD 将 `$<TARGET_FILE:...>` 与 `NeoPointer_DirectURL.meta.json` `copy_if_different` 到 `${DEPLOY_DIR}/pointers/`。
- VERSIONINFO：根 CMakeLists 通过 `nsum_add_version_info` 写入「NSUM 指针解析器插件 (NeoPointer_DirectURL)」。

## 公共符号

| 符号 | 签名 | 说明 |
|------|------|------|
| `CreatePointer` | `extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer()` | 工厂：`new DirectUrlPointer()` |
| `SetPluginLogSink` | `extern "C" __declspec(dllexport) void SetPluginLogSink(ILogSink*)` | 宏展开，宿主日志注入点 |
| `DirectUrlPointer` | `class ... : public NeoCore::IPluginPointer`（匿名命名空间） | 实现类：`name()` 返回 `"DirectURL"` |

**接口方法实现概览**（摘自 `NeoCore/include/IPluginPointer.h`）：

| 方法 | 实现要点 |
|------|----------|
| `std::string name() const` | 返回 `"DirectURL"` |
| `bool can_handle(const PointerInfo& ptr) const` | `ptr.resolver == "direct_url"` |
| `std::string resolve_url(const PointerInfo& ptr)` | `metadata.url` 非空字符串原样返回；否则返回空串 |
| `bool validate(const std::string& filepath, const std::string& expected_sha256)` | SHA-256 流式（64 KiB）十六进制小写比对 |
| `supported_download_methods()` / `can_batch_search()` / `batch_search(...)` | 未覆写，走接口默认（`{}` / `false` / `{}`） |