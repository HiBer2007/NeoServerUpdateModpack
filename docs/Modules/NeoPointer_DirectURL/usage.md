# NeoPointer_DirectURL 使用文档

## 快速开始

### 构建

`NeoPointer_DirectURL` 是标准插件 DLL，随主工程 `neo_deploy` 目标自动构建并部署：

```powershell
$cmake = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
```

### 部署

产物 `NeoPointer_DirectURL.dll` 与 `NeoPointer_DirectURL.meta.json` 由根 `CMakeLists.txt`（`POINTER_TARGETS` → `neo_deploy` POST_BUILD）复制到：

```
<build>/deploy/pointers/NeoPointer_DirectURL.dll
<build>/deploy/pointers/NeoPointer_DirectURL.meta.json
```

`NeoBuild::PointerDownloader::scanResolvers(pointersDir)` 扫描该目录下 `*.dll`（Windows）/`*.so`/`*.dylib` 即完成加载——**指针插件不使用 meta.json 参与加载**，meta.json 仅作能力描述与文档。

## 插件契约

### 接口实现要点（`NeoCore/include/IPluginPointer.h`）

实现类 `DirectUrlPointer`（匿名命名空间内）逐字实现：

| 接口方法 | 实现 |
|----------|------|
| `std::string name() const override` | 返回 `"DirectURL"`（用于重复注册检测） |
| `bool can_handle(const NeoCore::PointerInfo& ptr) const override` | `return ptr.resolver == "direct_url";` |
| `std::string resolve_url(const NeoCore::PointerInfo& ptr) override` | `metadata.url` 非空字符串则**原样返回**；缺失/非字符串记 `metadata missing 'url' field`、空串记 `'url' field is empty`，均返回 `""` |
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

### meta.json 字段逐字段说明（`NeoPointer_DirectURL.meta.json`）

| 字段 | 值 | 说明 |
|------|-----|------|
| `name` | `"DirectURL Pointer Resolver"` | 显示名 |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoPointer_DirectURL.dll"` | 声明 DLL 文件名（加载器 `scanResolvers` 直接扫 DLL，不读此字段 [实现为纯描述]） |
| `resolver` | `"direct_url"` | **契约键**：与指针文件 `resolver` 字段及 `can_handle` 判定一致 |
| `description` | `"直链下载，直接使用指针 metadata 中的 url 字段"` | 人类可读描述 |
| `features` | `["resolve_url", "validate"]` | 能力声明，与实现一致（无 batch_search/reverse_lookup） |

## 功能细节

### DirectURL metadata.url 语义

`resolve_url` 的唯一逻辑：

```cpp
if (!ptr.metadata.contains("url") || !ptr.metadata["url"].is_string()) {
    // error: DirectURL pointer: metadata missing 'url' field
    return "";
}
std::string url = ptr.metadata["url"].get<std::string>();
if (url.empty()) {
    // error: DirectURL pointer: 'url' field is empty
    return "";
}
return url;
```

- **类型严格**：`url` 必须是 JSON 字符串（`is_string()`），数字/布尔/对象都会按缺失处理返回空串。
- **原文返回**：不做 URL 规范化、不做 `scheme` 校验。相对路径或非 HTTP(S) 值也会透传——最终由 `PointerDownloader::downloadToPath` 用 `QUrl` 请求，非法 URL 会以网络错误失败。
- **下载与校验仍由宿主负责**：返回的 URL 由 `PointerDownloader::download` 下载到缓存 `<cacheDir>/<sha256>`，随后 `validateFile`（宿主自身实现）按 `pointer.sha256` 校验；本插件的 `validate` 可在宿主侧单独调用（如编辑器「验证」场景）。

### 完整性校验（`validate`）

与 `NeoPointer_Modrinth` 相同实现：`QCryptographicHash::Sha256` 流式（`kBufferSize = 65536`），`toHex().toLower()` 与 `expected_sha256` 小写比对；文件不存在（`file not found`）/无法打开（`cannot open file`）记错误返回 `false`，比对不一致记 `SHA-256 mismatch` 返回 `false`。

## 典型用法

### 指针文件消费链路

直链指针文件示例（由分支配置/编辑器生成）：

```json
{
  "sha256": "<目标文件 SHA-256 十六进制小写>",
  "resolver": "direct_url",
  "metadata": {
    "url": "https://example.com/mods/example-mod-1.0.0.jar"
  }
}
```

宿主解析构造 `PointerInfo`（`build_engine.cpp` 实际代码）：

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
NeoCore::PointerInfo ptr{ /* resolver: "direct_url", ... */ };
NeoBuild::ResolveResult r = downloader.resolveUrl(ptr);   // can_handle 命中 → 返回 metadata.url
DownloadResult d = downloader.download(ptr, cacheDir);    // 下载 + SHA-256 校验 + 缓存
```

`findResolver` 按注册顺序遍历 `resolvers_`，第一个 `can_handle(ptr)==true` 的实例被选中；未命中报 `No resolver found for pointer type: <resolver>`。

## 注意事项

- **`__declspec(dllexport)` 必须保留**：`CreatePointer` 缺它则 DLL 零导出，`GetProcAddress` 报 `CreatePointer not found`（2026-08-06 全局修复教训）。修改后 `dumpbin /exports NeoPointer_DirectURL.dll` 验证。
- **URL 信任边界**：本插件**不校验 URL 目标内容**，下载后的正确性完全依赖 SHA-256。直链 URL 指向错误文件时，`PointerDownloader` 的哈希校验会拦截（`SHA-256 hash mismatch after download`）；但解析阶段无法预知。
- **`url` 字段为纯字符串**：不要在 metadata 里放 `{ "url": {"href": ...} }` 之类嵌套结构——`is_string()` 判定直接失败。
- **冗余依赖**：CMakeLists 链接了 `Qt6::Network` 但源码未使用；保持或移除不影响功能（如需精简依赖可去掉，改后需 clean-first 重建）。
- **日志通道**：源文件用 `CLogger::Error/Warn`；宿主注入 `SetPluginLogSink` 后以 `[NeoPointer_DirectURL]` 前缀输出；未注入时回退 spdlog default_logger 跨模块到达宿主。
- **与 Modrinth 插件的关系**：两者按 `resolver` 字段分流，互不干扰；同一指针文件可声明多个 `resolvers`（`PointerFileData.resolvers`），`PointerDownloader` 遍历匹配。

## 相关文档

- 接口契约：`docs/Modules/NeoCore/README.md`、`docs/Modules/NeoCore/usage.md`
- 消费端：`docs/Modules/NeoBuild/README.md`、`docs/Modules/NeoBuild/usage.md`（`PointerDownloader`/`BuildEngine::stepProcessFiles`）
- 同族插件：`docs/Modules/NeoPointer_Modrinth/README.md`、`docs/Modules/NeoPointer_Modrinth/usage.md`
- 指针文件设计/部署：`docs/Modules/README.md`（总索引）、根 `CMakeLists.txt` 的 `POINTER_TARGETS` 部署段落