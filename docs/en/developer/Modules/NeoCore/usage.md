---
title: NeoCore Usage
description: NeoCore public APIs and typical usage: paths, plugin loading, cancellation, progress and parallel downloads.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoCore/usage.md
---

# NeoCore Usage

## Quick start

```cpp
#include <path_utf8.h>
#include <cancel_token.h>
#include <PluginLoader.h>

namespace fsu8 = NeoCore::fsu8;

NeoCore::CancelToken token;
auto path = fsu8::path(u8"config/example.json");
if (fsu8::exists(path)) { /* ... */ }
```

## Public API

| API | Signature | Description |
|---|---|---|
| `fsu8::path` | `std::filesystem::path path(const std::string&)` | UTF-8 → path |
| `fsu8::exists` | `bool exists(const std::filesystem::path&)` | Exists (error_code overload) |
| `CancelToken::cancel` | `void cancel()` | Request cancellation |
| `CancelToken::cancelled` | `bool cancelled() const` | Query cancellation |
| `PluginLoader::ScanDirectory` | `void ScanDirectory(const std::string& parsersDir)` | Scan parsers |
| `PluginLoader::FindParser` | `IConfigParser* FindParser(const std::string& filepath) const` | Select parser |
| `PluginLoader::ListParsers` | `std::vector<ParserCapability> ListParsers() const` | List capabilities |
| `ParallelDownloader::download` | `static ParallelDownloadResult download(const QUrl&, const QString&, const ParallelDownloadOptions&, Progress = nullptr, CancelToken* = nullptr)` | Range downloads |

## Typical usage

### Load parsers

```cpp
NeoCore::PluginLoader loader;
loader.ScanDirectory("parsers");
if (auto* parser = loader.FindParser("config/a.json")) {
    for (auto& key : parser->list_keys("config/a.json")) { /* ... */ }
}
```

### Parallel download

```cpp
NeoCore::ParallelDownloadOptions opt;
opt.threads = 8;
opt.minSize = 8LL * 1024 * 1024;
auto result = NeoCore::ParallelDownloader::download(
    QUrl("https://example.com/a.zip"), "a.zip", opt,
    [](qint64 received, qint64 total) { /* progress */ }, &token);
if (!result.success) { /* fall back to single stream */ }
```

## Notes

- Plugin factories must be exported with `__declspec(dllexport)`;
- Never use `fs::path(std::string)` on Windows;
- Cancellation is cooperative; long loops must check it;
- Rebuild plugins after interface changes.

## Related

- [README.md](./README.md)
- [ABI](../../framework/abi.md)
