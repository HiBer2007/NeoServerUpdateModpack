---
title: NeoWorkspace Usage
description: Workspace engine APIs: loading workspace files, branch queries, Git operations and sync.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoWorkspace/usage.md
---

# NeoWorkspace Usage

## Quick start

```cpp
#include <workspace_manager.h>

NeoWorkspace::WorkspaceManager manager;
if (!manager.loadFromFile("workspace.json")) { /* handle error */ }
for (const auto& branch : manager.branches()) {
    // branch.name / branch.parent / branch.type / branch.core
}
```

> On Windows with non-ASCII paths use `loadFromJson(nlohmann::json::parse(...))` to avoid ANSI path issues.

## Public API

| API | Signature | Description |
|---|---|---|
| `loadFromFile` | `bool loadFromFile(const std::string& configPath)` | Load workspace.json |
| `loadFromJson` | `bool loadFromJson(const nlohmann::json& config)` | Load from JSON |
| `branches` | `std::vector<BranchConfig> branches() const` | Branch list (by value) |
| `findBranch` | `BranchConfig findBranch(const std::string& name) const` | Find a branch (by value) |
| `defaultBranch` | `std::string defaultBranch() const` | Git default branch |
| `GitOperations::clone` | `GitResult clone(const std::string& url, const std::string& targetDir, int timeoutMs = 120000)` | Clone |
| `GitOperations::fetch` | `GitResult fetch(const std::string& repoDir, const std::string& remote = "origin")` | Fetch |
| `GitOperations::checkout` | `GitResult checkout(const std::string& repoDir, const std::string& branch)` | Checkout |
| `SyncEngine::init` | `bool init(const std::string& targetDir, const std::string& cacheDir)` | Init sync |
| `SyncEngine::syncFile` | `SyncResult syncFile(const std::string& sourcePath, const std::string& relativeTargetPath, const std::string& expectedSha256, bool hasCachedFile(const std::string& sha256) const)` | Sync a file |
| `SyncEngine::syncConfig` | `SyncResult syncConfig(const std::string& configPath, const std::string& remoteContent, const std::string& localContent, const std::vector<std::string>& trackedKeys, const std::vector<int>& trackedLines)` | Merge config |

## Typical usage

### Branches

```cpp
for (const auto& b : manager.branches()) { /* b.name / b.parent / b.type / b.core */ }
auto base = manager.findBranch("base");
```

### Git

```cpp
NeoWorkspace::GitOperations git;
auto head = git.revParse(repoDir);
auto branches = git.listRemoteBranches(repoDir);
```

### Sync

```cpp
NeoWorkspace::SyncEngine engine;
engine.init(targetDir, cacheDir);
auto r = engine.syncFile(source, "mods/a.jar", sha256,
    [&](const std::string& s){ return engine.hasCachedFile(s); });
```

## Notes

- Parent chains must be acyclic;
- `sync_policies.files.<path>.mode` is `full`/`partial`/`ignore`;
- `mirror` protects listed files before deletion;
- Paths must be UTF-8 (`fsu8`).

## Related

- [README.md](./README.md)
- [Sync rules](../../../pack-author/sync-rules.md)
