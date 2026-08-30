# NeoPointer_Modrinth 使用文档

## 快速开始

### 构建

`NeoPointer_Modrinth` 是标准插件 DLL，随主工程 `neo_deploy` 目标自动构建并部署：

```powershell
$cmake = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
```

### 部署

产物 `NeoPointer_Modrinth.dll` 与 `NeoPointer_Modrinth.meta.json` 由根 `CMakeLists.txt`（`POINTER_TARGETS` → `neo_deploy` POST_BUILD）复制到：

```
<build>/deploy/pointers/NeoPointer_Modrinth.dll
<build>/deploy/pointers/NeoPointer_Modrinth.meta.json
```

`NeoBuild::PointerDownloader::scanResolvers(pointersDir)` 扫描该目录下 `*.dll`（Windows）/`*.so`/`*.dylib` 即完成加载——**指针插件不使用 meta.json 参与加载**，meta.json 仅作能力描述与文档。

## 插件契约

### 接口实现要点（`NeoCore/include/IPluginPointer.h`）

实现类 `ModrinthPointer`（匿名命名空间内）逐字实现：

| 接口方法 | 实现 |
|----------|------|
| `std::string name() const override` | 返回 `"Modrinth"`（用于重复注册检测） |
| `bool can_handle(const NeoCore::PointerInfo& ptr) const override` | `return ptr.resolver == "modrinth";` |
| `std::string resolve_url(const NeoCore::PointerInfo& ptr) override` | 双路径解析（见「功能细节」）；任何失败返回空串 `""` |
| `bool validate(const std::string& filepath, const std::string& expected_sha256) override` | 文件存在 → 流式 SHA-256（64 KiB 缓冲）→ 十六进制小写比对，一致返回 `true` |

`PointerInfo` 结构（接口原文）：

```cpp
struct PointerInfo {
    std::string resolver;
    std::string sha256;
    nlohmann::json metadata;
};
```

未覆写的接口成员（走默认实现）：`supported_download_methods()`（默认 `{}`）、`can_batch_search()`（默认 `false`）、`batch_search(...)`（默认 `{}`）。

### meta.json 字段逐字段说明（`NeoPointer_Modrinth.meta.json`）

| 字段 | 值 | 说明 |
|------|-----|------|
| `name` | `"Modrinth Pointer Resolver"` | 显示名 |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoPointer_Modrinth.dll"` | 声明 DLL 文件名（加载器 `scanResolvers` 直接扫 DLL，不读此字段 [实现为纯描述]） |
| `resolver` | `"modrinth"` | **契约键**：与指针文件 `resolver` 字段及 `can_handle` 判定一致 |
| `description` | `"通过 Modrinth API 解析文件下载地址"` | 人类可读描述 |
| `features` | `["resolve_url", "validate", "reverse_lookup"]` | 能力声明，与实现一致（✅ 2026-08-30 起 `batch_search` 已从 meta 移除，见注意事项） |

## 功能细节

### Modrinth API 语义

`resolve_url` 按 `ptr.metadata` 内容走两条路径（互斥分支，代码顺序）：

**路径 A：`project_id` + `version_id`（版本直查）**

要求 `metadata["project_id"]`、`metadata["version_id"]` 均为**非空字符串**，否则记错误日志（`project_id or version_id is empty`）返回空串。请求：

```
GET https://api.modrinth.com/v2/project/{project_id}/version/{version_id}
```

**路径 B：`sha1`（文件反查）**

要求 `metadata["sha1"]` 为非空字符串，否则返回空串。请求：

```
GET https://api.modrinth.com/v2/version_file/{sha1}?algorithm=sha1
```

**两条路径都没有 → 错误日志 `metadata requires project_id+version_id or sha1`，返回空串。**

### HTTP 请求参数（`makeModrinthRequest`）

| 参数 | 值 |
|------|-----|
| 超时 | `constexpr int kTimeoutMs = 30000;`（`QNetworkRequest::setTransferTimeout` + `QTimer` abort 双保险） |
| `User-Agent` | `"NeoServerUpdateModpack/1.0 (NeoServer)"` |
| `ContentTypeHeader` | `"application/json"` |
| 重定向策略 | `QNetworkRequest::NoLessSafeRedirectPolicy` |
| 错误处理 | `reply->error() != QNetworkReply::NoError` → 打 `Modrinth API request failed: HTTP {} error: {}`，返回空；空响应记 `Modrinth API returned empty response` |

### 响应解析（`extractFileUrl`）

- 要求响应 JSON 含 `files` 数组且非空，否则记错误返回空串。
- `matchSha1` 非空时（路径 B）：遍历 `files`，取 `f["hashes"]["sha1"] == matchSha1` 的文件返回 `f["url"]`；未命中打 Warn（`Modrinth reverse lookup: no file matched SHA-1 ... falling back to first file`）并回退 `files[0]["url"]`。
- 路径 A（`matchSha1` 为空）：直接取 `files[0]["url"]`。
- JSON 解析失败（`json::parse_error`）记 `Modrinth API JSON parse error` 返回空串。

### 完整性校验（`validate`）

与 `DirectUrlPointer` 相同实现：`QCryptographicHash::Sha256` 流式（`kBufferSize = 65536`），`toHex().toLower()` 与 `expected_sha256` 小写比对；文件不存在/无法打开记错误返回 `false`。宿主 `PointerDownloader::download` 对**缓存命中与下载后**都会再走自己的 `validateFile`（按 `pointer.sha256` 全等比较），插件 `validate` 是对同一标准的第二套实现——若 `validate` 返回 `false`，`download` 仍会尝试重新下载（见 `pointer_downloader.cpp`：`validateFile(cached, pointer.sha256)` 失败 → 删缓存重下）。

## 典型用法

### 指针文件消费链路

指针文件是缺失文件的 JSON 占位符，由 `BuildEngine::stepProcessFiles` 读取并构造 `PointerInfo` 后交给 `PointerDownloader`：

```json
{
  "sha256": "<目标文件 SHA-256 十六进制小写>",
  "resolver": "modrinth",
  "metadata": {
    "project_id": "abcdefgh",
    "version_id": "0123456789abcdef0123456789abcdef01234567"
  }
}
```

解析代码（`build_engine.cpp`，实际签名）：

```cpp
auto j = nlohmann::json::parse(raw.toStdString());
NeoCore::PointerInfo ptr;
ptr.sha256 = j.value("sha256", "");
ptr.resolver = j.value("resolver", "");
ptr.metadata = j.value("metadata", nlohmann::json::object());
```

### 宿主加载（`NeoBuild::PointerDownloader`）

```cpp
PointerDownloader downloader;
downloader.scanResolvers(pointersDir);          // 扫 *.dll，GetProcAddress("CreatePointer")
NeoCore::PointerInfo ptr{ /* resolver: "modrinth", ... */ };
NeoBuild::ResolveResult r = downloader.resolveUrl(ptr);   // 调 can_handle + resolve_url
DownloadResult d = downloader.download(ptr, cacheDir);    // resolve_url → 下载 → SHA-256 校验 → 缓存
```

`loadResolverDLL` 失败模式：`LoadLibrary` 失败（记 `error <DWORD>`）、`CreatePointer not found`、`CreatePointer returned null`、重名（`resolver already loaded`，`delete rawInstance` 并 `FreeLibrary`）；可选 `SetPluginLogSink` 注入宿主日志 sink。

## 注意事项

- **`__declspec(dllexport)` 必须保留**：`CreatePointer` 缺它则 DLL 零导出（无 `.lib`），`GetProcAddress` 报 `CreatePointer not found`（2026-08-06 全局修复教训）。修改后可用 `dumpbin /exports NeoPointer_Modrinth.dll` 验证 `CreatePointer`/`SetPluginLogSink` 在列。
- **✅ 已修复（2026-08-30）：meta `features` 与实现一致**：`batch_search` 已从 `NeoPointer_Modrinth.meta.json` 移除，`features` 现为 `resolve_url`/`validate`/`reverse_lookup`，与实现一致——`reverse_lookup` 由 `resolve_url` 的 sha1 路径承担，批量搜索不在能力范围内。
- **metadata 校验严格**：`project_id`/`version_id`/`sha1` 必须为**字符串且非空**（非字符串或空都失败返回空串），指针文件生成侧需保证类型正确。
- **响应契约脆弱点**：API 返回 `files` 缺失/为空直接失败；sha1 反查未命中仅 Warn 并取 `files[0]`——存在**解析到错误文件**的可能（URL 后续仍由 sha256 校验兜底，下载阶段会拦截哈希不符）。
- **网络要求**：Modrinth API 需要外网；`kTimeoutMs = 30000` 固定，慢网络下解析可能超时返回空串。
- **日志通道**：源文件用 `CLogger::Error/Warn`（NeoCore PUBLIC 链传递 CommonLoggerCPP）；宿主注入 `SetPluginLogSink` 后以 `[NeoPointer_Modrinth]` 前缀输出。
- **跨 DLL 日志**：插件各自持有静态 `g_pluginSink`；未注入时回退 `CLogger` → spdlog default_logger 跨模块到达宿主，无需插件自行 `Logger::Init`。

## 相关文档

- 接口契约：`docs/Modules/NeoCore/README.md`、`docs/Modules/NeoCore/usage.md`
- 消费端：`docs/Modules/NeoBuild/README.md`、`docs/Modules/NeoBuild/usage.md`（`PointerDownloader`/`BuildEngine::stepProcessFiles`）
- 同族插件：`docs/Modules/NeoPointer_DirectURL/README.md`、`docs/Modules/NeoPointer_DirectURL/usage.md`
- 指针文件设计/部署：`docs/Modules/README.md`（总索引）、根 `CMakeLists.txt` 的 `POINTER_TARGETS` 部署段落
- 外部：Modrinth API 文档 https://docs.modrinth.com/api/