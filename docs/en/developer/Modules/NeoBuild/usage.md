---
title: NeoBuild Usage
description: BuildEngine init/build/preview and pointer/exporter APIs.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoBuild/usage.md
---

# NeoBuild Usage

## Quick start

```cpp
#include <build_engine.h>

NeoBuild::BuildEngine engine;
engine.init(workspaceDir, cacheDir, outputBaseDir, "exporters");
auto result = engine.build("base", &progress, &cancel, "main");
```

## Public API

| API | Signature | Description |
|---|---|---|
| `BuildEngine::init` | `bool init(const std::string& workspacePath, const std::string& cacheDir = "", const std::string& outputBaseDir = "", const std::string& exportersDir = "")` | Load workspace/plugins |
| `BuildEngine::build` | `BuildResult build(const std::string& branchName, NeoCore::IBuildProgress* progress = nullptr, NeoCore::CancelToken* cancelToken = nullptr, const std::string& gitBranch = "")` | Build |
| `BuildEngine::previewStructure` | `nlohmann::json previewStructure(const std::string& format, ...)` | Preview structure |
| `PointerDownloader::resolve` | see `pointer_downloader.h` | Resolve pointers |
| `ModpackExporter::scanExporters` | see header | Scan exporters |
| `ServerConfigSync::sync` | see header | L3 sync |
| `SyncPolicyExecutor::apply` | see header | Apply policies |

## Typical usage

### Build and export

```cpp
NeoBuild::BuildEngine engine;
engine.init(repo, cache, buildBase, "exporters");
auto r = engine.build("base", &progress, &cancel, "main");
if (r.success) { exporter->export_modpack(buildDir, "out.nsumpack", meta); }
```

### Preview only

```cpp
engine.init(repo, cache, tempDir, "exporters");
engine.build("base", nullptr, &cancel, "main");
auto tree = engine.previewStructure("nsum");
```

## Notes

- The fourth argument of `build` is the **Git branch**, not the modpack branch;
- Fetch is skipped when the local repo has no remote;
- Pointer failures abort the build (resolver name in the log);
- Keep preview output in a temp directory.

## Related

- [README.md](./README.md)
- [Pointer system](../../architecture/pointer-system.md)
