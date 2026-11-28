---
title: NeoWorkspace 使用
description: 工作区引擎 API：加载 workspace、分支查询、Git 操作与同步。
lang: zh
status: complete
updated: 2026-11-28
---

# NeoWorkspace 使用

## 快速开始

```cpp
#include <workspace_manager.h>

NeoWorkspace::WorkspaceManager manager;
if (!manager.loadFromFile("workspace.json")) {
    // 处理错误
}
for (const auto& branch : manager.branches()) {
    // branch.name / branch.parent / branch.type / branch.core
}
```

> Windows 中文路径请用 `loadFromJson(nlohmann::json::parse(...))`，避免 ANSI 路径问题。

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `loadFromFile` | `bool loadFromFile(const std::string& configPath)` | 加载 workspace.json |
| `loadFromJson` | `bool loadFromJson(const nlohmann::json& config)` | 从 JSON 加载 |
| `branches` | `std::vector<BranchConfig> branches() const` | 分支列表（按值返回） |
| `findBranch` | `BranchConfig findBranch(const std::string& name) const` | 查找分支（按值返回） |
| `defaultBranch` | `std::string defaultBranch() const` | Git 默认分支 |
| `GitOperations::clone` | `GitResult clone(const std::string& url, const std::string& targetDir, int timeoutMs = 120000)` | 克隆 |
| `GitOperations::lsRemote` | `GitResult lsRemote(const std::string& url, int timeoutMs = 8000)` | 探测仓库地址（`git ls-remote`） |
| `GitOperations::fetch` | `GitResult fetch(const std::string& repoDir, const std::string& remote = "origin")` | 抓取 |
| `GitOperations::checkout` | `GitResult checkout(const std::string& repoDir, const std::string& branch)` | 切换分支 |
| `SyncEngine::init` | `bool init(const std::string& targetDir, const std::string& cacheDir)` | 初始化同步 |
| `SyncEngine::syncFile` | `SyncResult syncFile(const std::string& sourcePath, const std::string& relativeTargetPath, const std::string& expectedSha256, NeoCore::CancelToken* cancelToken = nullptr)` | 同步单文件 |
| `SyncEngine::syncConfig` | `SyncResult syncConfig(const std::string& configPath, NeoCore::IConfigParser* parser, NeoCore::TrackingMode mode, const std::string& remoteContent, const std::string& localContent, const std::vector<std::string>& trackedKeys, const std::vector<int>& trackedLines, NeoCore::CancelToken* cancelToken = nullptr)` | 配置合并 |
| `SyncEngine::hasCachedFile` | `bool hasCachedFile(const std::string& sha256) const` | 缓存查询 |

## 典型用法

### 列出分支

```cpp
for (const auto& b : manager.branches()) {
    // b.name / b.parent / b.type / b.core
}
auto base = manager.findBranch("base");
```

### Git 操作

```cpp
NeoWorkspace::GitOperations git;
auto head = git.revParse(repoDir);
auto branches = git.listRemoteBranches(repoDir);
```

### 同步

```cpp
NeoWorkspace::SyncEngine engine;
engine.init(targetDir, cacheDir);
auto r = engine.syncFile(source, "mods/a.jar", sha256, &cancel);
```

## 仓库多地址与 SSH 密钥

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
NeoWorkspace::applySshEnvironment(); // 进程级 GIT_SSH_COMMAND
```

| API | 签名 | 说明 |
|---|---|---|
| `inferRepoUrls` | `std::vector<RepoUrl> inferRepoUrls(const std::string& primary)` | 由主地址推导 ssh/https/http |
| `workspaceRepoUrls` | `std::vector<RepoUrl> workspaceRepoUrls(const nlohmann::json&)` | 读 `git.remotes`，缺失时由 `git.remote` 推导 |
| `ensureWorkspaceRepoUrls` | `void ensureWorkspaceRepoUrls(nlohmann::json&)` | 写回 `git.remotes` |
| `repoUrlsJson` / `repoUrlsFromJson` | JSON ↔ 地址列表 | 导出与 bridge 共用格式 |
| `SshConfig::load/save` | 读写 `~/.nsum/git_ssh.json` | `ssh_path`/`key_path`/`host` |
| `generateSshKey` | `bool generateSshKey(path, comment, error)` | 调 `ssh-keygen -t ed25519` |
| `configureSshHost` | `bool configureSshHost(host, key, error)` | 写 `~/.ssh/config` 标记块 |
| `testSshConnection` | `bool testSshConnection(host, ssh, key, output, error)` | `ssh -T`，30s 超时 |
| `applySshEnvironment` | `void applySshEnvironment()` | 未设置时注入 `GIT_SSH_COMMAND` |

## 注意事项

- 分支父链不允许成环；
- `sync_policies.files.<path>.mode` 取 `full`/`partial`/`ignore`；
- `mirror` 删除前有保护清单；
- 中文路径必须 UTF-8（`fsu8`）。

## 相关

- [README.md](./README.md)
- [同步规则](../../../pack-author/sync-rules.md)
