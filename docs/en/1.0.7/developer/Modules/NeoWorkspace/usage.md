---
title: NeoWorkspace usage
description: NeoWorkspace APIs and typical usage: loading the workspace model, querying branches, driving Git and syncing files through the hash cache.
lang: en
status: complete
updated: 2026-11-28
translation_of: /zh/1.0.7/developer/Modules/NeoWorkspace/usage.md
---

# NeoWorkspace usage

## Quick start

```cpp
#include <workspace_manager.h>

NeoWorkspace::WorkspaceManager manager;
if (!manager.loadFromFile("workspace.json")) {
    return 1;                                   // missing or unparsable workspace.json
}

for (const auto& branch : manager.branches()) {
    // branch.name / branch.parent / branch.type / branch.core
}

auto base = manager.findBranch("base");
const std::string gitDefault = manager.defaultBranch();
```

> `loadFromFile` opens the path with `std::ifstream`, i.e. a narrow (ANSI) path on Windows. For a repository root that contains non-ASCII characters, read the file yourself - for example with `QFile` - and call `loadFromJson(nlohmann::json::parse(bytes))` instead.

## Public API

| API | Signature | Description |
|---|---|---|
| `loadFromFile` | `bool loadFromFile(const std::string& configPath)` | loads and parses `workspace.json` |
| `loadFromJson` | `bool loadFromJson(const nlohmann::json& config)` | loads from an already parsed document |
| `validate` | `bool validate()` | checks the required sections: `workspace.name`, `git.remote` and a non-empty `branches` array |
| `branches` | `std::vector<BranchConfig> branches() const` | all branches (returned by value) |
| `findBranch` | `BranchConfig findBranch(const std::string& name) const` | one branch by name (by value); returns a default-constructed `BranchConfig` when the name is unknown |
| `branchInheritanceChain` | `std::vector<std::string> branchInheritanceChain(const std::string& branchName) const` | branch first, root last; empty when a cycle is detected |
| `defaultBranch` | `std::string defaultBranch() const` | Git default branch recorded in the workspace |
| `syncPolicy` | `SyncPolicy syncPolicy(const std::string& branch = "") const` | effective policy: top-level `sync_policies` merged with the branch-level override |
| `fileManifest` | `std::unordered_map<std::string, std::string> fileManifest() const` | relative path to SHA-256 |
| `pointerFiles` | `std::unordered_map<std::string, NeoCore::PointerInfo> pointerFiles() const` | pointer entries declared by the workspace |
| `loadBranchManifest` | `BranchManifest loadBranchManifest(const std::string& branchName) const` | reads the branch's marker file |
| `GitOperations::clone` | `GitResult clone(const std::string& url, const std::string& targetDir, int timeoutMs = 120000)` | clone |
| `GitOperations::lsRemote` | `GitResult lsRemote(const std::string& url, int timeoutMs = 8000)` | Probe a repository address (`git ls-remote`) |
| `GitOperations::fetch` | `GitResult fetch(const std::string& repoDir, const std::string& remote = "origin")` | fetch remote-tracking refs only |
| `GitOperations::checkout` | `GitResult checkout(const std::string& repoDir, const std::string& branch)` | checkout a branch |
| `GitOperations::revParse` | `GitResult revParse(const std::string& repoDir, const std::string& ref = "HEAD")` | resolve a ref to a commit |
| `SyncEngine::init` | `bool init(const std::string& targetDir, const std::string& cacheDir)` | target directory plus hash cache directory |
| `SyncEngine::syncFile` | `SyncResult syncFile(const std::string& sourcePath, const std::string& relativeTargetPath, const std::string& expectedSha256, NeoCore::CancelToken* cancelToken = nullptr)` | copies one file, reusing and verifying the cache |
| `SyncEngine::hasCachedFile` | `bool hasCachedFile(const std::string& sha256) const` | cache lookup by SHA-256 |
| `SyncEngine::syncConfig` | `SyncResult syncConfig(const std::string& configPath, NeoCore::IConfigParser* parser, NeoCore::TrackingMode mode, const std::string& remoteContent, const std::string& localContent, const std::vector<std::string>& trackedKeys, const std::vector<int>& trackedLines, NeoCore::CancelToken* cancelToken = nullptr)` | config merge delegated to the parser plugin |

## Typical usage

### Listing branches

```cpp
for (const auto& b : manager.branches()) {
    // b.name / b.parent / b.type ("universal" | "client" | "server") / b.core
    // b.hidden branches are not shown by the main application
}

auto chain = manager.branchInheritanceChain("server-prod");   // leaf first, root last
auto base  = manager.findBranch("base");
```

### Git operations

```cpp
#include <git_operations.h>

NeoWorkspace::GitOperations::SetDefaultGitPath("C:/tools/git/mingw64/bin/git.exe");  // optional, default "git"
NeoWorkspace::GitOperations git;

auto head     = git.revParse(repoDir);
auto branches = git.listRemoteBranches(repoDir);
auto status   = git.status(repoDir);

if (!git.hasRemote(repoDir)) {
    // local-only repository: skip fetch instead of failing
}
```

`GitOperations` reads the executable from the static default path; the main application sets it once from `install.conf` so that the bundled MinGit is used everywhere. When git reports a repository as having `dubious ownership`, `isDubiousOwnership` / `trustRepository` provide the explicit opt-in.

### Syncing a file

```cpp
#include <sync_engine.h>
#include <cancel_token.h>

NeoWorkspace::SyncEngine engine;
engine.init(targetDir, cacheDir);

NeoCore::CancelToken token;
auto r = engine.syncFile(sourcePath, "mods/a.jar", sha256, &token);
if (!r.success) {
    // r.failedFiles / r.messages carry the reason
}

if (engine.hasCachedFile(sha256)) {
    const std::string cached = engine.cacheFilePath(sha256);   // hash re-check via validateCacheFile()
}
```

### Reading the effective policy

```cpp
const NeoWorkspace::SyncPolicy policy = manager.syncPolicy("server-prod");
// policy.defaultFolderPolicy: skip | mirror | incremental_add | incremental_overwrite
// policy.folders: { path, policy }   policy.files: { path, mode, trackedKeys, trackedLines }
```

`NeoBuild::SyncPolicyExecutor` consumes exactly this structure, so a GUI preview and a real build see the same merged policy.

## Repository URLs and SSH keys

```cpp
#include <repo_urls.h>
#include <ssh_key.h>

auto urls = NeoWorkspace::inferRepoUrls("git@github.com:owner/repo.git");
nlohmann::json workspace = NeoWorkspace::repoUrlsJson(urls);
NeoWorkspace::ensureWorkspaceRepoUrls(workspace);

const std::string key = NeoWorkspace::defaultSshKeyPath();
std::string error;
NeoWorkspace::generateSshKey(key, "nsum", error);
NeoWorkspace::configureSshHost("github.com", key, error);
NeoWorkspace::applySshEnvironment(); // process-wide GIT_SSH_COMMAND
```

| API | Signature | Description |
|---|---|---|
| `inferRepoUrls` | `std::vector<RepoUrl> inferRepoUrls(const std::string& primary)` | Derives ssh/https/http from a primary URL |
| `workspaceRepoUrls` | `std::vector<RepoUrl> workspaceRepoUrls(const nlohmann::json&)` | Reads `git.remotes`, falls back to `git.remote` |
| `ensureWorkspaceRepoUrls` | `void ensureWorkspaceRepoUrls(nlohmann::json&)` | Writes `git.remotes` |
| `repoUrlsJson` / `repoUrlsFromJson` | JSON <-> URL list | Shared by the exporter and the bridge |
| `SshConfig::load/save` | Reads/writes `~/.nsum/git_ssh.json` | `ssh_path` / `key_path` / `host` |
| `generateSshKey` | `bool generateSshKey(path, comment, error)` | Runs `ssh-keygen -t ed25519` |
| `configureSshHost` | `bool configureSshHost(host, key, error)` | Writes the marked block in `~/.ssh/config` |
| `testSshConnection` | `bool testSshConnection(host, ssh, key, output, error)` | `ssh -T` with a 30s timeout |
| `applySshEnvironment` | `void applySshEnvironment()` | Sets `GIT_SSH_COMMAND` when unset |

## Notes

- **Parent chains must be acyclic**: `branchInheritanceChain` stops at depth 100 and returns an empty chain when it detects a cycle; duplicate names and missing parents are caught by `exec validate`, not by `WorkspaceManager::validate()`, which only checks the required sections.
- **`sync_policies.files.<path>.mode` is `full` / `partial` / `ignore`**: `full` writes the file as-is, `partial` merges through `IConfigParser` with `tracked_keys` / `tracked_lines`, `ignore` leaves the target untouched.
- **`mirror` protects before deleting**: a mirrored folder is cleared and rewritten, and the protected list (root-level `log4j2.xml`, `options.txt`, `natives*`, instance metadata, `saves/`) is preserved.
- **Paths must be UTF-8**: construct them through `fsu8::path` and hand relative paths to JSON as `generic_u8string()`; `u8string()` returns backslashes on Windows.
- **`loadFromFile` is the narrow-path exception**: it uses `std::ifstream`, so on Windows a non-ASCII repository root needs `loadFromJson` with a `QFile`/`fsu8`-based read.
- **`branches()` and `findBranch()` return by value**: do not cache a reference into the returned vector; keep a copy or call the accessor again.
- **`BranchConfig::type` defaults to `universal`** and `core` is only meaningful for `type == "server"`; the editor deletes `core` for the other types.

## Related

- [README.md](./README.md)
- [Sync rules](../../../pack-author/sync-rules.md)
