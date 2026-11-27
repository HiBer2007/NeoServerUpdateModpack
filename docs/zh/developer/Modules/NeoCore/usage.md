---
title: NeoCore 使用
description: NeoCore 的公共 API 与典型用法：路径、插件加载、取消、进度与并行下载。
lang: zh
status: complete
updated: 2026-11-27
---

# NeoCore 使用

## 快速开始

```cpp
#include <path_utf8.h>
#include <cancel_token.h>
#include <PluginLoader.h>

namespace fsu8 = NeoCore::fsu8;

NeoCore::CancelToken token;
auto path = fsu8::path(u8"config/example.json");
if (fsu8::exists(path)) {
    // ...
}
```

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `fsu8::path` | `std::filesystem::path path(const std::string&)` | UTF-8 → path |
| `fsu8::exists` | `bool exists(const std::filesystem::path&)` | 带 error_code 重载 |
| `CancelToken::cancel` | `void cancel()` | 请求取消 |
| `CancelToken::cancelled` | `bool cancelled() const` | 查询 |
| `PluginLoader::ScanDirectory` | `void ScanDirectory(const std::string& parsersDir)` | 扫描 parser |
| `PluginLoader::FindParser` | `IConfigParser* FindParser(const std::string& filepath) const` | 按文件选择 |
| `PluginLoader::ListParsers` | `std::vector<ParserCapability> ListParsers() const` | 列出能力 |
| `ParallelDownloader::download` | `static ParallelDownloadResult download(const QUrl&, const QString&, const ParallelDownloadOptions&, Progress = nullptr, CancelToken* = nullptr)` | Range 分片下载 |

## 典型用法

### 加载解析器

```cpp
NeoCore::PluginLoader loader;
loader.ScanDirectory("parsers");
if (auto* parser = loader.FindParser("config/a.json")) {
    for (auto& key : parser->list_keys("config/a.json")) { /* ... */ }
}
```

### 并行下载

```cpp
NeoCore::ParallelDownloadOptions opt;
opt.threads = 8;
opt.minSize = 8LL * 1024 * 1024;
auto result = NeoCore::ParallelDownloader::download(
    QUrl("https://example.com/a.zip"), "a.zip", opt,
    [](qint64 received, qint64 total) { /* progress */ }, &token);
if (!result.success) { /* 回退单流 */ }
```

## 注意事项

- 插件工厂必须 `__declspec(dllexport)`；
- 路径禁止 `fs::path(std::string)`；
- 取消是协作式，长循环需主动检查；
- 修改 `IConfigParser` 等接口后所有插件需重建。

## 相关

- [README.md](./README.md)
- [ABI](../../framework/abi.md)