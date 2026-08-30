# NeoPointer_Modrinth 说明文档

## 概述

`NeoPointer_Modrinth` 是 NeoServerUpdateModpack 的**指针解析器插件**（Pointer Resolver）：实现 `NeoCore::IPluginPointer` 接口，通过 **Modrinth API** 将哈希占位符（`.pointer` 文件）解析为实际下载 URL，并提供文件完整性校验。在项目中与 `NeoPointer_DirectURL` 并列，被 `NeoBuild::PointerDownloader` 从 `pointers/` 目录扫描加载，在构建阶段把指针解析为真实文件。

- 读取 Meta：`NeoPointer_Modrinth.meta.json` 声明 `resolver: "modrinth"`。
- 能力集（meta.json `features`）：`resolve_url`、`validate`、`reverse_lookup`（✅ 2026-08-30 起移除 `batch_search`，与实现一致）。
- 实现类：`ModrinthPointer`（匿名命名空间内），导出工厂函数 `CreatePointer`。

## 设计目标

- **双路径解析**：按 `PointerInfo.metadata` 提供 `project_id + version_id`（版本直查）或 `sha1`（反查）两种方式定位文件下载地址。
- **零信任完整性**：下载/缓存复用前一律用 SHA-256 重新校验，防止磁盘损坏或篡改造成静默数据错误（与 `PointerDownloader::validateFile` 同标准）。
- **HTTP 语义正确**：请求携带规范 `User-Agent`、30 秒超时、`NoLessSafeRedirectPolicy` 重定向策略；API 失败/空响应/JSON 解析失败全部显式打日志并返回空串。
- **插件契约最小化**：只实现接口规定的 `name`/`can_handle`/`resolve_url`/`validate`，不依赖任何宿主内部实现。

## 模块边界

**做什么**

- 解析 Modrinth 指针：`metadata` 含 `project_id`+`version_id` → `https://api.modrinth.com/v2/project/{project_id}/version/{version_id}`；含 `sha1` → `https://api.modrinth.com/v2/version_file/{sha1}?algorithm=sha1`。
- 从 API 返回的 `files` 数组中提取文件 `url`（sha1 模式优先按 `hashes.sha1` 匹配，未命中回退首个文件）。
- 对已下载文件做 SHA-256 流式校验（64 KiB 缓冲）。

**不做什么**

- **不下载文件**：解析出 URL 后由 `PointerDownloader::download` / `downloadToPath` 负责写盘与缓存。
- **不解析 `.pointer` 文件本身**：宿主 `BuildEngine::stepProcessFiles` 负责读指针文件 JSON 并构造 `NeoCore::PointerInfo`。
- **不处理平台无关逻辑**：不保存缓存、不管理指针文件路径，全部由 NeoBuild 侧消费。
- **能力与实现一致（✅ 2026-08-30）**：meta.json `features` 已移除 `batch_search`（现为 `resolve_url`/`validate`/`reverse_lookup`），`can_batch_search()`/`batch_search()` 未覆写即不在能力范围；`reverse_lookup` 由 `resolve_url` 内含 sha1 反查承担；`supported_download_methods()` 也未覆写（默认空）。
- CurseForge 因需要 API 密钥申请，不在本项目范围内。

## 依赖关系

依赖以 `modules/NeoPointer_Modrinth/CMakeLists.txt` 为准（均为 `PRIVATE`）：

| 依赖 | 类型 | 用途 |
|------|------|------|
| `NeoCore` | 静态库 | `IPluginPointer`/`PointerInfo` 接口（`NeoCore/include/IPluginPointer.h`）；经其 PUBLIC 链传递 `CommonLoggerCPP` 的 `CLogger`/`plugin_log_sink.h` |
| `spdlog::spdlog` | vcpkg | 日志（源文件实际经 `CLogger` 输出） |
| `Qt6::Core` | Qt6 | `QUrl`/`QUrlQuery`/`QByteArray`/`QCryptographicHash`/`QFile`/`QTimer` |
| `Qt6::Network` | Qt6 | `QNetworkAccessManager`/`QNetworkReply`/`QNetworkRequest` |

头文件依赖：`IPluginPointer.h`（NeoCore）、`logger.h`、`plugin_log_sink.h`（CommonLoggerCPP，经 NeoCore 传递）、`nlohmann/json.hpp`（经 NeoCore 传递）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库定义、include 目录、链接项 |
| `NeoPointer_Modrinth.meta.json` | 插件元数据：名称/版本/DLL 名/`resolver`/描述/`features` |
| `src/pointer_modrinth.cpp` | `ModrinthPointer` 实现 + `CreatePointer` 导出 + 日志注册宏 |

## 构建集成

- 目标类型：`add_library(NeoPointer_Modrinth SHARED ...)` → 产出 `NeoPointer_Modrinth.dll`。
- 导出符号（`extern "C" __declspec(dllexport)`，必须带 `__declspec`，否则 `GetProcAddress` 找不到）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer() {
      return new ModrinthPointer();
  }
  ```

- 日志注册（可选演进约定）：`NEO_DECLARE_PLUGIN_LOG_SINK("NeoPointer_Modrinth")` 展开出 `SetPluginLogSink(ILogSink*)` 导出，宿主 `PointerDownloader::loadResolverDLL` 用 `GetProcAddress` 注入；旧插件无此符号时自动跳过。
- `.meta.json` 与 DLL 一同由根 `CMakeLists.txt` 部署：`POINTER_TARGETS` 含 `NeoPointer_Modrinth`，`neo_deploy` POST_BUILD 将 `$<TARGET_FILE:...>` 与 `NeoPointer_Modrinth.meta.json` `copy_if_different` 到 `${DEPLOY_DIR}/pointers/`。
- VERSIONINFO：根 CMakeLists 通过 `nsum_add_version_info` 写入「NSUM 指针解析器插件 (NeoPointer_Modrinth)」。

## 公共符号

| 符号 | 签名 | 说明 |
|------|------|------|
| `CreatePointer` | `extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer()` | 工厂：`new ModrinthPointer()` |
| `SetPluginLogSink` | `extern "C" __declspec(dllexport) void SetPluginLogSink(ILogSink*)` | 宏展开，宿主日志注入点 |
| `ModrinthPointer` | `class ... : public NeoCore::IPluginPointer`（匿名命名空间） | 实现类：`name()` 返回 `"Modrinth"` |

**接口方法实现概览**（摘自 `NeoCore/include/IPluginPointer.h`）：

| 方法 | 实现要点 |
|------|----------|
| `std::string name() const` | 返回 `"Modrinth"` |
| `bool can_handle(const PointerInfo& ptr) const` | `ptr.resolver == "modrinth"` |
| `std::string resolve_url(const PointerInfo& ptr)` | 见上文双路径；失败返回空串 |
| `bool validate(const std::string& filepath, const std::string& expected_sha256)` | SHA-256 流式（64 KiB）十六进制小写比对 |
| `supported_download_methods()` / `can_batch_search()` / `batch_search(...)` | 未覆写，走接口默认（`{}` / `false` / `{}`） |