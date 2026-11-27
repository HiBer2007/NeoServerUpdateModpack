---
title: NeoBuild usage
description: NeoBuild APIs and typical usage: engine initialization, the build pipeline, exporting through a plugin, pointer downloads and target-directory sync.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoBuild/usage.md
---

# NeoBuild usage

## Quick start

```cpp
#include <build_engine.h>
#include <cancel_token.h>

NeoBuild::BuildEngine engine;
if (!engine.init(workspaceDir, cacheDir, outputBaseDir, exportersDir)) {
    return 1;                                   // workspace.json missing or exporters dir unusable
}

NeoCore::CancelToken cancel;
NeoBuild::BuildResult result = engine.build("base", nullptr, &cancel, "main");
if (!result.success) {
    // result.errorMessage carries the cause; result.warnings lists the soft failures
    return 1;
}
const std::string output = result.outputDir;    // target directory when one is set, else the build dir
```

`init` accepts either the repository directory or the `workspace.json` file path (a file path is reduced to its parent directory). Empty arguments default to `<workspace>/.cache` for the cache and `<workspace>/output` for the build directory.

## Public API

| API | Signature | Description |
|---|---|---|
| `BuildEngine::init` | `bool init(const std::string& workspacePath, const std::string& cacheDir = "", const std::string& outputBaseDir = "", const std::string& exportersDir = "")` | loads `workspace.json`, creates cache/output, scans exporters and `pointers/` |
| `BuildEngine::build` | `BuildResult build(const std::string& branchName, NeoCore::IBuildProgress* progress = nullptr, NeoCore::CancelToken* cancelToken = nullptr, const std::string& gitBranch = "")` | full pipeline; the last argument is the **Git** branch |
| `BuildEngine::exportModpack` | `bool exportModpack(const std::string& format, const std::string& outputPath, const NeoCore::ExportMetadata& metadata)` | exports the last build output through the exporter plugin |
| `BuildEngine::previewStructure` | `nlohmann::json previewStructure(const std::string& format, const NeoCore::ExportMetadata& metadata, const std::string& targetDir = "")` | archive tree preview; `targetDir` enables the `U`/`M`/`D` comparison |
| `BuildEngine::setGitPath` | `void setGitPath(const std::string& path)` | replaces the bundled-git lookup for this engine |
| `BuildEngine::setTargetDir` | `void setTargetDir(const std::string& dir)` | target working directory; empty means "packaging format, do not sync" |
| `BuildEngine::cacheDir` / `outputDir` | `std::string cacheDir() const`; `std::string outputDir() const` | resolved directories |
| `BuildEngine::mergedFileManifest` / `mergedPointerFiles` | `const std::unordered_map<std::string, std::string>& mergedFileManifest() const` and the `PointerInfo` counterpart | branch-aware manifest after `branch_config/<branch>.json` merge |
| `PointerDownloader::resolveUrl` | `ResolveResult resolveUrl(const NeoCore::PointerInfo& pointer) const` | resolver lookup plus URL resolution, no download |
| `PointerDownloader::download` | `DownloadResult download(const NeoCore::PointerInfo& pointer, const std::string& cacheDir, std::function<void(const DownloadProgress&)> progressCallback = nullptr, NeoCore::CancelToken* cancelToken = nullptr)` | download into the hash cache with SHA-256 validation |
| `PointerDownloader::isCached` / `cachePath` | `bool isCached(const std::string& cacheDir, const std::string& sha256) const`; `std::string cachePath(const std::string& cacheDir, const std::string& sha256) const` | cache inspection |
| `ModpackExporter::scanExporters` | `void scanExporters(const std::string& exportersDir)` | loads every `*.meta.json` plus the matching DLL |
| `ModpackExporter::exporterForFormat` | `NeoCore::IModpackExporter* exporterForFormat(const std::string& format)` | raw plugin pointer, stable after `scanExporters` |
| `ServerConfigSync::syncAll` | `bool syncAll(NeoCore::CancelToken* cancelToken = nullptr)` | applies the L3 rules to every save |
| `ServerConfigSync::syncConfig` | `bool syncConfig(const ServerConfigEntry& entry, NeoCore::CancelToken* cancelToken = nullptr)` | applies one entry |
| `SyncPolicyExecutor::execute` | `Result execute(const std::string& sourceDir, const std::string& targetDir, const NeoWorkspace::SyncPolicy& policy, NeoCore::IBuildProgress* progress = nullptr, NeoCore::CancelToken* cancel = nullptr)` | L1/L2 application from build dir to target |
| `BranchMerger::merge` | `MergeResult merge(const std::vector<BranchLayer>& layers, const std::string& outputDir, bool overwriteChild = true, NeoCore::CancelToken* cancelToken = nullptr)` | merges parent chain layers |

## Typical usage

### Build and export

```cpp
NeoBuild::BuildEngine engine;
engine.init(repoDir, cacheDir, buildBase, exportersDir);
engine.setTargetDir("");                          // packaging format: no working-directory sync

NeoCore::CancelToken cancel;
auto r = engine.build("base", &progress, &cancel, "main");
if (!r.success) {
    return 1;
}

NeoCore::ExportMetadata meta;
meta.name = "Example pack";
meta.version = "1.0.0";
meta.extra["branch"] = "base";
if (!engine.exportModpack("nsum", "out.nsumpack", meta)) {
    return 1;                                     // the exporter plugin reported a failure
}
```

`exportModpack` reuses the output of the preceding `build`; it does not rebuild. Formats are whatever the exporter plugins in `exportersDir` declare (`mcbbs`, `modrinth`, `hmcl`, `nsum` in this repository).

### Preview only

```cpp
NeoBuild::BuildEngine engine;
engine.init(repoDir, tempCacheDir, tempBuildDir, exportersDir);   // keep both in %TEMP%
NeoCore::CancelToken cancel;
engine.build("base", nullptr, &cancel, "main");

NeoCore::ExportMetadata meta;
auto tree = engine.previewStructure("nsum", meta, "");            // no target dir: every entry unchanged
```

Virtual previews need a real build: exporting the structure of an unbuilt directory yields an empty tree. Keep the cache and build directories under the system temp directory so no artifact leaks into a release package.

### Resolving a pointer before scanning

```cpp
NeoBuild::PointerDownloader downloader;
downloader.scanResolvers("pointers");             // NeoPointer_* DLLs plus their .meta.json

NeoCore::PointerInfo pointer;
pointer.sha256 = sha;
pointer.resolver = "modrinth";
pointer.metadata = nlohmann::json::parse(metadata);

auto resolved = downloader.resolveUrl(pointer);
if (!resolved.success) {
    // resolved.resolver / resolved.errorMessage name the failing plugin
}

auto dl = downloader.download(pointer, cacheDir, [](const NeoBuild::DownloadProgress& p) {
    // p.bytesDownloaded / p.totalBytes / p.error
}, nullptr);
```

### Syncing into a working directory

```cpp
NeoBuild::BuildEngine engine;
engine.init(repoDir, cacheDir, tempBuildDir, exportersDir);
engine.setTargetDir(instanceDir);                 // HMCL game directory

NeoCore::CancelToken cancel;
auto r = engine.build("server-prod", &progress, &cancel, "main");
// r.outputDir == instanceDir; existing files follow the L1/L2 policy
```

With a target directory the pipeline adds `stepSyncTarget` (92 percent) and then `stepSyncServerConfigs` (97 percent), because the L3 rules must be applied after the target files exist.

## Notes

- **The fourth argument of `build` is the Git branch, not the modpack branch**: checkout uses `gitBranch.empty() ? branchName : gitBranch`. In this repository a modpack branch lives in `branches/<name>/`, while the Git branch may only be `main`; passing the modpack branch to checkout fails with a branch-switch error.
- **Fetch is skipped for local repositories**: `stepCloneOrFetch` logs that no remote is configured and keeps the local working tree; a failing fetch is only a warning and the local workspace is used.
- **Pointer failures abort the build**: `stepProcessFiles` returns false, the result carries an error and a warning, and the log names the resolver that failed.
- **Cancellation returns a failed result, not an exception**: the token and `IBuildProgress::is_cancelled()` are both checked between steps, and the partial output stays on disk for inspection.
- **`init` scans `pointers/` relative to the executable**: even a caller that only builds must ship the pointer DLLs next to the exe. The exporters directory is optional but without it `exportModpack` and `previewStructure` cannot find a format.
- **`init` loads `workspace.json` through a narrow path**: `WorkspaceManager::loadFromFile` opens the file with `std::ifstream`, so a workspace root containing non-ASCII characters can make `init` fail on Windows. The GUI avoids this by calling the exporter plugin's `build_modpack` directly with an explicit `BuildTarget`; callers that must support such roots should do the same or keep the workspace root ASCII.
- **Preview output belongs in a temp directory**: `%TEMP%/NSUM-virtual-build/<branch>/` is the convention used by the GUI, so nothing is left behind in the release tree.
- **Progress percentages are part of the contract**: `clone` 5, `checkout` 15, `merge` 25, `download` 40, `sync_config` 60, `custom_mods` 70, `finalize` 85, `sync_target` 92, `server_config` 97, `done` 100 - a UI that mirrors them keeps the CLI log and the GUI bar aligned.

## Related

- [README.md](./README.md)
- [Pointer system](../../architecture/pointer-system.md)
