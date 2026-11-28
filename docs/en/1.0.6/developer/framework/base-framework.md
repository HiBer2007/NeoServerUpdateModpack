---
title: Base Framework
description: NeoCore public infrastructure: paths, logging, cancellation, error codes, progress and plugin loading.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/framework/base-framework.md
---

# Base Framework

`NeoCore` is the shared foundation for all modules/plugins: interfaces and lightweight utilities only, no business logic.

## Components

| Component | Header | Purpose |
|---|---|---|
| Config parser interface | `IConfigParser.h` | Parse/merge configs |
| Pointer interface | `IPluginPointer.h` | Resolve download URLs |
| Exporter interface | `IModpackExporter.h` | Package formats |
| Editor extension interfaces | `IConfigEditorExtension.h` / `IPointerEditorExtension.h` | Editor extensions |
| Plugin loading | `PluginLoader.h` | Scan/load parser DLLs |
| Paths | `path_utf8.h` | UTF-8 path helpers (`fsu8`) |
| Cancellation | `cancel_token.h` | Build/sync cancellation |
| Error codes | `error_codes.h` | Unified errors |
| Progress | `IBuildProgress.h` | Build progress callbacks |
| Parallel downloader | `parallel_downloader.h` | Range downloads |

## Logging

- Implemented in `CommonLoggerCPP` (`CLogger`, global namespace);
- The host calls `CLogger::Init`; plugins share it (`CLogger::Resolve` falls back to the spdlog default logger);
- Plugin logs use the exported `SetPluginLogSink` symbol with a `[plugin]` prefix.

## Paths

```cpp
#include <path_utf8.h>
namespace fsu8 = NeoCore::fsu8;

auto p = fsu8::path(u8"config/file.json");
if (fsu8::exists(p)) { /* ... */ }
```

- Never use `fs::path(std::string)` on Windows (interpreted as ANSI);
- Use `generic_u8string()` for display paths (forward slashes).

## Cancellation

```cpp
NeoCore::CancelToken token;
token.cancel();
if (token.cancelled()) return;
```

- Checked in build/sync/download loops;
- Cooperative, does not kill threads.

## Error codes

`error_codes.h` defines the unified enum; the CLI maps it to exit codes and JSON `error` strings.

## Loading

```cpp
NeoCore::PluginLoader loader;
loader.ScanDirectory("parsers");
auto* parser = loader.FindParser("config/a.json");
```

- `ScanDirectory` pairs `*.dll` with `*.meta.json`;
- `FindParser` selects by extension and priority.

## Related

- [Module layout](./module-layout.md)
- [ABI](./abi.md)
