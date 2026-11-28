---
title: NeoCore usage
description: NeoCore public APIs and typical usage: UTF-8 paths, parser loading, cancellation, build progress and the HTTP Range parallel downloader.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/Modules/NeoCore/usage.md
---

# NeoCore usage

## Quick start

```cpp
#include <cancel_token.h>
#include <path_utf8.h>
#include <PluginLoader.h>

#include <filesystem>

int main()
{
    NeoCore::CancelToken token;
    std::filesystem::path config = fsu8::path("config/example.json");   // UTF-8 -> std::filesystem::path
    if (!fsu8::exists(config)) {
        return 1;
    }

    NeoCore::PluginLoader loader;
    loader.ScanDirectory("parsers");                                    // parser DLLs plus their .meta.json
    if (token.is_cancelled()) {
        return 0;                                                       // cooperative cancellation
    }
    return 0;
}
```

`fsu8` lives in the **global namespace** (`namespace fsu8` in `path_utf8.h`), not inside `NeoCore`; `PluginLoader`, `CancelToken` and `ParallelDownloader` are in `NeoCore`.

## Public API

| API | Signature | Description |
|---|---|---|
| `fsu8::path` | `std::filesystem::path path(const std::string& value)` (also `const char*` and `const std::filesystem::path&` overloads) | UTF-8 string to `std::filesystem::path` via `std::filesystem::u8path` |
| `fsu8::exists` | `template <class T> bool exists(const T& value)`; `template <class T> bool exists(const T& value, std::error_code& ec)` | existence test with an `error_code` overload that never throws |
| `fsu8::is_directory` | `template <class T> bool is_directory(const T& value)`; `template <class T> bool is_directory(const T& value, std::error_code& ec)` | directory test, same overload pattern |
| `CancelToken::CancelToken` | `CancelToken()` | starts in the not-cancelled state |
| `CancelToken::request_cancel` | `void request_cancel()` | raises the flag |
| `CancelToken::is_cancelled` | `bool is_cancelled() const` | queried by engines between steps |
| `CancelToken::reset` | `void reset()` | clears the flag so the same token can be reused |
| `PluginLoader::ScanDirectory` | `void ScanDirectory(const std::string& parsersDir)` | scans a directory for `*.meta.json` plus its DLL and calls `CreateParser` |
| `PluginLoader::FindParser` | `IConfigParser* FindParser(const std::string& filepath) const` | picks a parser by file path/extension; `nullptr` when none matches |
| `PluginLoader::ListParsers` | `std::vector<ParserCapability> ListParsers() const` | registered capabilities (`name`, `extensions`, `supported_modes`, `supports_line_tracking`, `priority`) |
| `PluginLoader::ParserCount` | `size_t ParserCount() const` | number of successfully loaded parsers |
| `ParallelDownloader::download` | `static ParallelDownloadResult download(const QUrl& url, const QString& targetPath, const ParallelDownloadOptions& options, Progress progress = nullptr, CancelToken* cancelToken = nullptr)` | HTTP Range chunk download; `Progress` is `std::function<void(qint64 received, qint64 total)>` |
| `ParallelDownloadOptions::fromEnvironment` | `static ParallelDownloadOptions fromEnvironment()` | reads the `NSUM_DOWNLOAD_*` overrides |
| `IConfigParser::capability` | `virtual ParserCapability capability() const = 0` | declares what the plugin can do |
| `IConfigParser::can_handle` | `virtual bool can_handle(const std::string& filepath) const = 0` | path/extension test |
| `IConfigParser::parse_entries` | `virtual std::vector<ConfigEntry> parse_entries(const std::string& filepath) = 0` | key/value view of a config file |
| `IConfigParser::merge_entries` | `virtual std::string merge_entries(const std::string& filepath, const std::vector<std::string>& tracked_keys, const std::string& remote_content, const std::string& local_content) = 0` | key-level merge |
| `IConfigParser::list_keys` | `virtual std::vector<std::string> list_keys(const std::string& filepath) = 0` | all keys, used to fill tracked-key lists |
| `IBuildProgress::set_main_stage` / `set_main_progress` / `set_main_message` | `virtual void ... = 0` | main progress bar |
| `IBuildProgress::add_sub_bar` / `remove_sub_bar` / `set_sub_progress` / `set_sub_info` | `virtual int add_sub_bar(const std::string& label) = 0` and friends | handle-based sub bars; invalid handles must be ignored silently |
| `IBuildProgress::log` / `is_cancelled` | `virtual void log(const std::string& line) = 0`; `virtual bool is_cancelled() const = 0` | log line and cancel poll |

## Typical usage

### Loading parsers

```cpp
#include <PluginLoader.h>

NeoCore::PluginLoader loader;
loader.ScanDirectory("parsers");                       // parsers/NeoParser_JSON.dll + NeoParser_JSON.meta.json

if (auto* parser = loader.FindParser("config/a.json")) {
    for (const auto& key : parser->list_keys("config/a.json")) {
        // offer the key to the tracked-keys UI, or merge a single key
    }
    auto entries = parser->parse_entries("config/a.json");
    (void)entries;
}
```

`FindParser` returns a non-owning pointer; the instance is owned by `PluginLoader` and released together with it. `ScanDirectory` pairs each `*.meta.json` with the DLL of the same base name - `NeoParser_JSON.meta.json` maps to `NeoParser_JSON.dll`, never to `NeoParser_JSON.meta.dll`.

### Parallel download with a cancel token

```cpp
#include <cancel_token.h>
#include <parallel_downloader.h>

NeoCore::CancelToken token;
NeoCore::ParallelDownloadOptions opt = NeoCore::ParallelDownloadOptions::fromEnvironment();
opt.threads = 8;                                 // chunk count; capped at 32 per server
opt.minSize = 8LL * 1024 * 1024;                 // smaller files stay single-stream

auto result = NeoCore::ParallelDownloader::download(
    QUrl("https://example.com/a.zip"), "a.zip", opt,
    [](qint64 received, qint64 total) { /* drive the progress bar */ },
    &token);

if (!result.success) {
    // result.cancelled == true -> user abort
    // otherwise fall back to a single-stream QNetworkAccessManager download
}
```

### Reporting progress from an engine

```cpp
#include <IBuildProgress.h>
#include <logger.h>

class ConsoleProgress : public NeoCore::IBuildProgress {
public:
    void set_main_stage(const std::string& stage) override { CLogger::Info("[{}]", stage); }
    void set_main_progress(int percent) override { (void)percent; }
    void set_main_message(const std::string& message) override { CLogger::Info("{}", message); }
    int add_sub_bar(const std::string&) override { return 0; }   // headless build: no sub bars at all
    void remove_sub_bar(int) override {}
    void set_sub_progress(int, int) override {}
    void set_sub_info(int, const std::string&) override {}
    void log(const std::string& line) override { CLogger::Info("{}", line); }
    bool is_cancelled() const override { return false; }
};
```

The interface is tolerant by contract: a caller may pass `nullptr`, an implementation must silently degrade instead of throwing, and handles `<= 0` must be ignored - which is why a headless host can implement every method above as a no-op and still run a full build.

## Notes

- **Plugin factories must be exported with `__declspec(dllexport)`**: `extern "C" __declspec(dllexport) IConfigParser* CreateParser()`; without it the DLL has no export, `GetProcAddress` fails and every parser silently disappears.
- **Never write `fs::path(std::string)` on Windows**: the narrow-string constructor converts through the ANSI code page and throws on non-ASCII paths. Use `fsu8::path`, and prefer `generic_u8string()` for relative paths that go into JSON or the UI.
- **Cancellation is cooperative**: nothing interrupts a running loop by itself. Long loops must call `CancelToken::is_cancelled()` (or `IBuildProgress::is_cancelled()`) and return early; also check it between download chunks.
- **`ParallelDownloader` needs a `QCoreApplication`**: it runs inside the Qt event loop and does not spawn threads. A server that does not report `Accept-Ranges` yields `ranged == false`, and the caller decides whether to fall back.
- **Environment overrides**: `NSUM_DOWNLOAD_THREADS` (default 16), `NSUM_DOWNLOAD_MIN_SIZE_MB` (default 8), `NSUM_DOWNLOAD_CHUNK_MB` (0 = auto), `NSUM_DOWNLOAD_RETRIES`, and `NSUM_DOWNLOAD_PARALLEL=0` disables parallel downloads entirely.
- **Interface changes force a rebuild of every plugin**: `IConfigParser`, `IPluginPointer`, `IModpackExporter` and both editor extension interfaces are consumed by separate DLLs; changing a virtual signature requires rebuilding `parsers/`, `pointers/` and `exporters/` together.
- **`PluginLoader` injects the host log sink while loading**: it resolves the optional `SetPluginLogSink` export and passes the host `LoggerLogSink`, so plugin log lines arrive prefixed with the plugin name. A plugin that does not export the symbol still works and falls back to its own `CLogger` channel.

## Related

- [README.md](./README.md)
- [ABI](../../framework/abi.md)
