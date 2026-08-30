# NeoWorkspace 使用文档

## 快速开始

根 `CMakeLists.txt` 已 `add_subdirectory(modules/NeoWorkspace)`（引擎核心层静态库，与 NeoCore/NeoBuild 同类）。宿主 target 链接：

```cmake
# NeoWorkspace PUBLIC 链 NeoCore 与 Qt6::Core —— 链接本模块即传递获得两者，无需重复写
target_link_libraries(MyTarget PRIVATE NeoWorkspace)

# 若仅引用 NeoWorkspace 头文件但未链接它（如 GUIWorker 的 PRIVATE include 模式），
# 最终 EXE 必须链接 NeoWorkspace（STATIC 库，符号在最终链接阶段解析）
```

源码侧包含（`include/` 为 PUBLIC 目录，按文件名直接包含）：

```cpp
#include "workspace_manager.h"   // WorkspaceManager / BranchConfig / BranchManifest / FileMarker
#include "git_operations.h"      // GitOperations / GitResult
#include "sync_engine.h"         // SyncEngine / SyncResult
#include "sync_policy.h"         // SyncPolicy / SyncPolicyFile / SyncPolicyFolder
#include "file_scanner.h"        // FileScanner / FileEntry
#include "history_store.h"       // HistoryStore / RecentRepo / RepoType
#include <cancel_token.h>        // NeoCore::CancelToken（可选，取消检查）
```

**前置条件**：`HistoryStore` 依赖 `QCoreApplication::applicationDirPath()`，调用前须已构造 `QCoreApplication`/`QApplication`（CLI 用 `QCoreApplication`，GUI 用 `QApplication`）。

## 公共 API

以下签名均逐字摘自 `modules/NeoWorkspace/include/` 各头文件。

### 1. `workspace_manager.h` — WorkspaceManager

**枚举 `FileMarker`：**

| 枚举值 | 值 | 说明 |
|--------|-----|------|
| `FileMarker::None` | 0 | 无标记（移除既有标记） |
| `FileMarker::Delete` | 1 | 标记删除（不参与继承） |
| `FileMarker::Override` | 2 | 标记覆盖（本分支自持版本） |

**结构体 `BranchManifest`：**

| 成员 | 类型 | 说明 |
|------|------|------|
| `branchName` | `std::string` | 分支名 |
| `markers` | `std::unordered_map<std::string, FileMarker>` | 相对路径 → 文件标记 |
| `toJson` | `nlohmann::json toJson() const` | 序列化：`{"branch": "...", "markers": {"<path>": "delete"\|"override", ...}}` |
| `fromJson` | `static BranchManifest fromJson(const nlohmann::json& j)` | 反序列化（"delete"/"override" 字符串 ↔ 枚举） |

**嵌套结构体 `WorkspaceManager::BranchConfig`（字段）：**

| 字段 | 类型/默认 | 说明 |
|------|-----------|------|
| `name` | `std::string` | 分支名 |
| `parent` | `std::string` | 父分支名（空 = 无父） |
| `gameVersion` | `std::string` | 游戏版本（JSON 键 `game_version`） |
| `modloader` | `std::string` | 加载器 |
| `modloaderVersion` | `std::string` | 加载器版本（JSON 键 `modloader_version`） |
| `description` | `std::string` | 分支描述（workspace.json 顶层，供 GUI 显示） |
| `hidden` | `bool = false` | 隐藏分支（主程序不可见） |

**类 `WorkspaceManager`：**

| 方法 | 签名 | 说明 |
|------|------|------|
| 构造/析构 | `WorkspaceManager(); ~WorkspaceManager();` | 默认构造；未加载/未校验状态 |
| `loadFromFile` | `bool loadFromFile(const std::string& configPath);` | 从 `workspace.json` 加载并解析 branches/file_manifest/pointer_files/sync_policies；JSON 解析失败返回 false |
| `loadFromJson` | `bool loadFromJson(const nlohmann::json& config);` | 从内存 JSON 加载（同上解析） |
| `validate` | `bool validate();` | 校验必含 `workspace.name`、`git.remote`、非空 `branches` 数组；须先加载成功 |
| `workspaceName` | `std::string workspaceName() const;` | `<workspace.name>` |
| `minecraftVersion` | `std::string minecraftVersion() const;` | `<workspace.minecraft_version>` |
| `modloader` | `std::string modloader() const;` | `<workspace.modloader>` |
| `gitRemote` | `std::string gitRemote() const;` | `<git.remote>` |
| `defaultBranch` | `std::string defaultBranch() const;` | `<git.default_branch>`，缺省 `"main"` |
| `config` | `const nlohmann::json& config() const;` | 原始配置 JSON 引用 |
| `branches` | `std::vector<BranchConfig> branches() const;` | 全部分支元数据 |
| `findBranch` | `BranchConfig findBranch(const std::string& name) const;` | 按名查找；未找到打日志并返回空 `BranchConfig{}` |
| `branchInheritanceChain` | `std::vector<std::string> branchInheritanceChain(const std::string& branchName) const;` | 继承链（自根父到自身，逆序）；环/断裂/超深（100）返回空 |
| `resolveDirectory` | `std::string resolveDirectory(const std::string& key, const std::string& branch = "") const;` | 查 `<directories.<key>>`，替换 `{branch}` 占位符，`\`→`/`；未定义返回空串 |
| `fileManifest` | `std::unordered_map<std::string, std::string> fileManifest() const;` | `<file_manifest>`（相对路径 → 目标盘路径等） |
| `syncPolicy` | `SyncPolicy syncPolicy(const std::string& branch = "") const;` | 有效同步策略：`branch` 为空或未定义分支级覆盖时返回顶层策略；否则顶层与分支级合并（folder/file 按 path 覆盖去重、configFiles 追加去重） |
| `serverConfigSyncEnabled` | `bool serverConfigSyncEnabled() const;` | `<serverconfig_sync.enabled>`，缺省 false |
| `serverConfigScanPaths` | `std::vector<std::string> serverConfigScanPaths() const;` | `<serverconfig_sync.scan_paths>`（`\`→`/` 规范化） |
| `pointerFiles` | `std::unordered_map<std::string, NeoCore::PointerInfo> pointerFiles() const;` | `<pointer_files>`（key = sha256） |
| `customModsEnabled` | `bool customModsEnabled() const;` | `<custom_mods.enabled>`，缺省 false |
| `customModsPath` | `std::string customModsPath(const std::string& branch) const;` | `<custom_mods.path>` + `{branch}` 替换 + `\`→`/` |
| `loadBranchManifest` | `BranchManifest loadBranchManifest(const std::string& branchName) const;` | 读 `<branchStorageDir>/branch_manifest.json`；不存在/解析失败返回 `{branchName, {}}` |
| `saveBranchManifest` | `void saveBranchManifest(const std::string& branchName, const BranchManifest& manifest) const;` | 写 `<branchStorageDir>/branch_manifest.json`（自动 mkdir） |
| `setFileMarker` | `void setFileMarker(const std::string& branchName, const std::string& relPath, FileMarker marker);` | 增/改/删（`None` 为删）标记并落盘；`Override` 同时确保 `.overrides/` 目录存在 |
| `branchStorageDir` | `std::string branchStorageDir(const std::string& branchName) const;` | 分支存储目录（由 `resolveDirectory("mods", branch)` 取父目录推导） |
| `branchOverridesDir` | `std::string branchOverridesDir(const std::string& branchName) const;` | `<branchStorageDir>/.overrides` |
| `listInheritedFiles` | `std::vector<std::string> listInheritedFiles(const std::string& branchName) const;` | 父分支继承文件（完整相对路径；排除本分支已覆盖/已标记、父链 delete 标记；含父 `.overrides`） |
| `fileExistsInParent` | `bool fileExistsInParent(const std::string& branchName, const std::string& relPath) const;` | 沿继承链查父分支存储目录/`.overrides`；父链有 delete 标记即视为不存在 |
| `branchFiles` | `std::vector<std::string> branchFiles(const std::string& branchName) const;` | 分支存储目录内全部文件（排除 `.overrides/` 与 `branch_manifest.json`，相对路径正斜杠） |

### 2. `sync_policy.h` — 同步策略数据模型（纯头文件，无 .cpp）

**结构体 `SyncPolicyFile`（L2 配置文件特化，`sync_policies.files`）：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `path` | `std::string` | 相对路径 |
| `mode` | `std::string` | `"full"` = 仅覆盖（直接写入目标）/ `"partial"` = 半同步 merge（经 IConfigParser，tracked_keys/tracked_lines 参与）/ `"ignore"` = 忽略（不写目标） |
| `trackedKeys` | `std::vector<std::string>` | `partial` 时跟踪的键 |
| `trackedLines` | `std::vector<int>` | `partial` 时跟踪的行号 |

**结构体 `SyncPolicyFolder`（L1 文件夹策略，`sync_policies.folders`，递归含子文件夹）：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `path` | `std::string` | 相对路径 |
| `policy` | `std::string` | `"skip"` = 跳过 / `"mirror"` = 覆盖(严格镜像：清空重写+删多余) / `"incremental_add"` = 增量补充(只补缺失) / `"incremental_overwrite"` = 增量覆盖(保留多余项,被改也写入) / `"default"` = 兜底(用分支/顶层 `default_folder_policy`) |

**结构体 `SyncPolicy`（有效同步策略 = 顶层 sync_policies 与分支级合并后的结果）：**

| 字段 | 类型/默认 | 说明 |
|------|-----------|------|
| `defaultFolderPolicy` | `std::string = "incremental_add"` | 兜底文件夹策略 |
| `folders` | `std::vector<SyncPolicyFolder>` | 文件夹策略列表 |
| `files` | `std::vector<SyncPolicyFile>` | 配置文件策略列表 |
| `configFiles` | `std::vector<std::string>` | 用户标记为配置文件的额外相对路径（无扩展名解析器时按 `full` 回落） |

> 本头文件是**数据模型**；策略的**执行**（镜像/增量/规则匹配）由 `NeoBuild` 的 `sync_policy_executor` 承担，不属于本模块。

### 3. `git_operations.h` — GitOperations

**结构体 `GitResult`：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `exitCode` | `int` | git 进程退出码（无法启动/超时杀进程为 -1） |
| `stdoutOutput` | `std::string` | 标准输出（UTF-8） |
| `stderrOutput` | `std::string` | 标准错误（UTF-8） |
| `errorCode` | `NeoCore::ErrorCode` | 语义错误码：`Success`/`GitTimeout`(1001)/`GitNotFound`(1002)/`GitCrash`(1003)/`Unknown`(9999) 等；非 0 exit 且未崩溃时为 `Unknown`（可再经 `NeoCore::AnalyzeGitError` 分析 stderr） |

**类 `GitOperations`（QProcess 逐条执行 git 命令）：**

| 方法 | 签名 | 说明 |
|------|------|------|
| 构造 | `GitOperations(const std::string& gitPath = GetDefaultGitPath());` | 可指定 git 可执行文件；默认走全局默认（见下） |
| `SetDefaultGitPath` | `static void SetDefaultGitPath(const std::string& path);` | 设置全局默认 git 路径（编辑器/主程序注入内置 Git 用） |
| `GetDefaultGitPath` | `static std::string GetDefaultGitPath();` | 全局默认；未设置时 `"git"`（PATH 查找） |
| `clone` | `GitResult clone(const std::string& url, const std::string& targetDir, int timeoutMs = 120000);` | `git clone <url> <targetDir>`；失败经 `AnalyzeGitError` 写入 `lastError` |
| `pull` | `GitResult pull(const std::string& repoDir, int timeoutMs = 60000);` | `git pull` |
| `fetch` | `GitResult fetch(const std::string& repoDir, const std::string& remote = "origin");` | `git fetch <remote>` |
| `checkout` | `GitResult checkout(const std::string& repoDir, const std::string& branch);` | `git checkout <branch>` |
| `createBranch` | `GitResult createBranch(const std::string& repoDir, const std::string& branch, const std::string& baseBranch = "");` | `git branch <branch> [<baseBranch>]` |
| `currentBranch` | `GitResult currentBranch(const std::string& repoDir);` | `git rev-parse --abbrev-ref HEAD`（stdout = 当前分支名） |
| `listBranches` | `GitResult listBranches(const std::string& repoDir);` | `git branch`（本地分支，stdout 每行一个） |
| `listRemoteBranches` | `GitResult listRemoteBranches(const std::string& repoDir);` | `git branch -r` |
| `status` | `GitResult status(const std::string& repoDir);` | ✅ 已修复（2026-08-30）：执行 `git status --porcelain -z`（NUL 分隔、路径原样 UTF-8）；CLI `info status` 消费点已把 NUL 转 `\n` 再入 JSON；编辑器 Git 信息对话框按 NUL 计数 |
| `revParse` | `GitResult revParse(const std::string& repoDir, const std::string& ref = "HEAD");` | `git rev-parse <ref>` |
| `log` | `GitResult log(const std::string& repoDir, const std::string& format = "%H %s", int maxCount = 10);` | `git log --format=<format> -n <maxCount>` |
| `lsFiles` | `GitResult lsFiles(const std::string& repoDir);` | `git ls-files` |
| `init` | `GitResult init(const std::string& dir);` | `git init` |
| `addRemote` | `GitResult addRemote(const std::string& dir, const std::string& name, const std::string& url);` | `git remote add <name> <url>` |
| `addAll` | `GitResult addAll(const std::string& dir);` | `git add -A`（**必须 -A**：`git add` 不暂存已删除文件） |
| `commit` | `GitResult commit(const std::string& dir, const std::string& message);` | `git commit -m <message>` |
| `push` | `GitResult push(const std::string& dir, const std::string& remote = "origin", const std::string& branch = "");` | `git push -u <remote> [<branch>]`（固定带 `-u` 设 upstream） |
| `generateSshKey` | `GitResult generateSshKey(const std::string& keyPath, const std::string& comment = "", const std::string& type = "ed25519");` | ✅ 已修复（2026-08-30）：生成 SSH 密钥（参数语义按 `ssh-keygen -t/-f/-N/-C` 设计）；经新增 `executeProgram()` 以 `siblingToolPath("ssh-keygen")` 执行（优先 git 同目录的 MinGit 伴随工具，否则 PATH），参数不再经 git 二进制派发 |
| `testSshConnection` | `GitResult testSshConnection(const std::string& host = "github.com");` | ✅ 已修复（2026-08-30）：测试 SSH 连接（`-T git@<host> -o StrictHostKeyChecking=accept-new -o BatchMode=yes`）；经新增 `executeProgram()` 以 `siblingToolPath("ssh")` 执行（优先 git 同目录的 MinGit 伴随工具，否则 PATH），参数不再经 git 二进制派发 |
| `defaultSshKeyPath` | `static std::string defaultSshKeyPath();` | 默认密钥路径 `~/.ssh/id_ed25519` |
| `readPublicKey` | `static std::string readPublicKey(const std::string& keyPath);` | 读 `<keyPath>.pub` 内容（trim 后）；不存在返回空串 |
| `isGitRepository` | `bool isGitRepository(const std::string& dir);` | `git rev-parse --git-dir` 成功即真 |
| `hasRemote` | `bool hasRemote(const std::string& dir);` | `git remote` 输出非空白即真；**本地仓库无 remote 时应跳过 fetch**（NeoBuild `stepCloneOrFetch` 约定） |
| `isDubiousOwnership` | `bool isDubiousOwnership(const std::string& dir);` | stderr 含 `detected dubious ownership` 即真（git 判定目录属主不可信，所有 git 命令失败） |
| `isTrustedRepository` | `bool isTrustedRepository(const std::string& dir);` | 读 `git config --get-all safe.directory`，条目双向 `\`→`/` 规范化后**大小写不敏感**比较；条目 `*` 视为全信任；exit 1（无条目）不视为错误 |
| `trustRepository` | `GitResult trustRepository(const std::string& dir);` | `git config --global --add safe.directory <dir>`（dir 自动 `\`→`/`；**反斜杠条目不生效**，见注意事项） |
| `lastError` | `std::string lastError() const;` | 最近一次失败的错误描述（经 `NeoCore::AnalyzeGitError`） |

> 每次执行由独立 QProcess 完成，`SeparateChannels`；启动失败 → `GitNotFound`，超时 kill → `GitTimeout`（timeout 后 stdout 仍回读），崩溃 → `GitCrash`。stdout/stderr 均以 `QString::fromUtf8` 解码。

### 4. `sync_engine.h` — SyncEngine

**结构体 `SyncResult`：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | `bool` | 是否整体成功 |
| `syncedFiles` | `int` | 成功同步文件数 |
| `conflictedFiles` | `int` | 冲突文件数 |
| `failedFiles` | `int` | 失败文件数 |
| `messages` | `std::vector<std::string>` | 逐条消息（英文） |

**类 `SyncEngine`：**

| 方法 | 签名 | 说明 |
|------|------|------|
| 构造 | `SyncEngine();` | 默认构造（未 init） |
| `init` | `bool init(const std::string& targetDir, const std::string& cacheDir);` | 设置目标/缓存目录（`\`→`/` 规范化）并 `fs::create_directories` 两者；失败返回 false |
| `syncFile` | `SyncResult syncFile(const std::string& sourcePath, const std::string& relativeTargetPath, const std::string& expectedSha256, NeoCore::CancelToken* cancelToken = nullptr);` | 校验源文件 SHA-256 == expected → 存入缓存 → 复制到 `<targetDir>/<relativeTargetPath>`（相对路径 `\`→`/`） |
| `hasCachedFile` | `bool hasCachedFile(const std::string& sha256) const;` | 缓存目录中是否存在该 sha256 的常规文件 |
| `cacheFilePath` | `std::string cacheFilePath(const std::string& sha256) const;` | `<cacheDir>/<sha256>`（正斜杠） |
| `validateCacheFile` | `bool validateCacheFile(const std::string& sha256) const;` | **零信任**：重算缓存文件 SHA-256，不匹配即删除该条目并返回 false |
| `storeInCache` | `bool storeInCache(const std::string& sha256, const std::string& content);` | 内容校验哈希一致后写入缓存（已存在且有效则直接返回 true） |
| `storeInCacheFile` | `bool storeInCacheFile(const std::string& sha256, const std::string& sourcePath);` | 将已有文件拷入缓存（先验源哈希） |
| `syncConfig` | `SyncResult syncConfig(const std::string& configPath, NeoCore::IConfigParser* parser, NeoCore::TrackingMode mode, const std::string& remoteContent, const std::string& localContent, const std::vector<std::string>& trackedKeys, const std::vector<int>& trackedLines, NeoCore::CancelToken* cancelToken = nullptr);` | 经解析器合并配置：`FullSync` → 直接 remote；`ConfigMerge` → `parser->merge_entries`；`LineByLine` → `parser->merge_lines`；`NoSync` → 跳过（success=true）；合并结果空时回落 remote；写入 `<targetDir>/<configPath>` |
| `cleanTargetDir` | `void cleanTargetDir();` | 删除并重建目标目录（`fs::remove_all` + `create_directories`） |
| `cleanCache` | `void cleanCache();` | 删除并重建缓存目录 |

> `NeoCore::TrackingMode` 枚举（IConfigParser.h）：`FullSync` / `ConfigMerge` / `LineByLine` / `NoSync`。配置同步须宿主提供 `NeoCore::IConfigParser`（配置解析器插件）。

### 5. `file_scanner.h` — FileScanner

**结构体 `FileEntry`：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `relativePath` | `std::string` | 相对根目录路径（`generic_string()` 正斜杠） |
| `absolutePath` | `std::string` | 绝对路径（`generic_string()`） |
| `sha256` | `std::string` | SHA-256（`computeHashes=false` 或指针文件时为文件名 stem） |
| `fileSize` | `uint64_t` | 文件大小 |
| `isPointer` | `bool` | 是否 `.pointer` 文件 |
| `lastModified` | `std::time_t` | 最后修改时间（Unix 秒） |

**类 `FileScanner`：**

| 方法 | 签名 | 说明 |
|------|------|------|
| 构造 | `FileScanner();` | — |
| `scanDirectory` | `std::vector<FileEntry> scanDirectory(const std::string& rootDir, const std::vector<std::string>& excludePatterns = {}, bool computeHashes = false, NeoCore::CancelToken* cancelToken = nullptr);` | 递归扫描（`skip_permission_denied`）；`excludePatterns` 为 **ECMAScript 正则**（`std::regex_match` 整串匹配相对路径，非 glob）；每 100 个文件做一次取消检查 |
| `scanPointers` | `std::vector<FileEntry> scanPointers(const std::string& dir);` | 只收集 `.pointer` 文件；`sha256` = 文件名 stem（去 `.pointer` 后缀） |
| `computeSha256` | `std::string computeSha256(const std::string& filepath);` | 分块（64 KiB）SHA-256，hex 小写 |
| `parsePointerFile` | `static NeoCore::PointerInfo parsePointerFile(const std::string& filepath);` | 读指针文件 JSON：`resolver` + `metadata`；`sha256` = 文件名 stem |
| `diff` | `DiffResult diff(const std::vector<FileEntry>& oldFiles, const std::vector<FileEntry>& newFiles) const;` | 按 `relativePath` 比对：两方均有哈希时按哈希判 modified/unchanged，否则按 size+lastModified 兜底；结果四组均排序 |

**嵌套结构体 `FileScanner::DiffResult`：** `added` / `removed` / `modified` / `unchanged`（均为 `std::vector<std::string>` 相对路径）。

### 6. `history_store.h` — HistoryStore

**枚举 `RepoType`：**

| 枚举值 | 值 | 说明 |
|--------|-----|------|
| `RepoType::Remote` | 0 | 远程仓库 |
| `RepoType::Local` | 1 | 本地仓库 |
| `RepoType::Cache` | 2 | 缓存仓库 |

**结构体 `RecentRepo`：**

| 字段 | 类型/默认 | 说明 |
|------|-----------|------|
| `type` | `RepoType = RepoType::Remote` | 仓库类型 |
| `location` | `std::string` | 仓库地址/路径 |
| `cachePath` | `std::string` | 缓存路径（远程仓库克隆位置） |

**类 `HistoryStore`（纯静态）：**

| 成员 | 签名 | 说明 |
|------|------|------|
| `MaxRecentRepos` | `static constexpr int MaxRecentRepos = 10;` | 历史条数上限 |
| `historyDir` | `static std::string historyDir();` | `<exe>/config/history`（基于 `QCoreApplication::applicationDirPath()`） |
| `historyPath` | `static std::string historyPath();` | `<exe>/config/history/main.json` |
| `recentCacheDir` | `static std::string recentCacheDir();` | `<exe>/config/history/cache`（远程缓存根目录；退出不清理，打开先 `git fetch --all --prune` 同步） |
| `readRecentRepos` | `static std::vector<RecentRepo> readRecentRepos();` | 读 `main.json`（数组）；缺失/解析失败返回空 |
| `saveRecentRepo` | `static void saveRecentRepo(const std::string& location, RepoType type = RepoType::Remote, const std::string& cachePath = "");` | 去重插入头部、截断到 `MaxRecentRepos`、写 `main.json`（缩进 2）；location trim 后为空则忽略 |

> 历史文件 `main.json` 条目字段：`type`（int）/ `location` / `cache_path`（可选）。GUIWorker 的 repo_page 按 `RepoType` 过滤重载最近仓库。

## 典型用法

### 1. 打开工作区并读取分支

```cpp
#include "workspace_manager.h"

NeoWorkspace::WorkspaceManager wm;
if (!wm.loadFromFile(repoDir + "/workspace.json")) {
    // JSON 解析失败 / 文件不存在
    return;
}
if (!wm.validate()) {
    // 缺少 workspace.name / git.remote / branches
    return;
}
for (const auto& b : wm.branches()) {
    // b.name / b.parent / b.description / b.hidden ...
}
auto chain = wm.branchInheritanceChain("branch_feature"); // {根父, ..., branch_feature}
auto policy = wm.syncPolicy("branch_feature");            // 顶层 + 分支级合并后的策略
std::string modsDir = wm.resolveDirectory("mods", "branch_feature"); // {branch} 替换 + 正斜杠
```

### 2. 列出分支与切换

```cpp
#include "git_operations.h"

NeoWorkspace::GitOperations git;
auto br = git.listBranches(repoDir);          // stdout：本地分支列表
auto cur = git.currentBranch(repoDir);        // stdout：当前分支（rev-parse --abbrev-ref HEAD）

if (git.hasRemote(repoDir)) {
    git.fetch(repoDir);                       // git fetch origin（本地仓库无 remote 跳过）
}
auto co = git.checkout(repoDir, "feature/x"); // git checkout feature/x
auto log = git.log(repoDir);                  // git log --format=%H %s -n 10
```

### 3. 信任陌生仓库（dubious ownership 三件套）

```cpp
#include "git_operations.h"

NeoWorkspace::GitOperations git;
if (git.isDubiousOwnership(dir)) {
    // 询问用户后执行信任：git config --global --add safe.directory K:/path（正斜杠已自动规范化）
    auto t = git.trustRepository(dir);
    if (t.exitCode == 0 && git.isTrustedRepository(dir)) {
        // 可信，继续加载
    }
}
// 信任前后都可用 isTrustedRepository(dir) 复查（双向规范化 + 大小写不敏感比较）
```

### 4. 扫描文件与差异

```cpp
#include "file_scanner.h"

NeoWorkspace::FileScanner scanner;
NeoCore::CancelToken token;   // 可选取消
auto files = scanner.scanDirectory(root,
    {"\\.git($|/)", "cache"}, // ECMAScript 正则（整串匹配相对路径）
    /*computeHashes=*/true, &token);

auto pointers = scanner.scanPointers(root);   // 仅 .pointer，sha256 = 文件名 stem
auto d = scanner.diff(oldFiles, files);       // added/removed/modified/unchanged
```

### 5. 执行同步（缓存优先 + 哈希校验）

```cpp
#include "sync_engine.h"

NeoWorkspace::SyncEngine engine;
if (!engine.init(targetDir, cacheDir)) return;

// 缓存命中且校验通过时可直接使用（零信任）
if (engine.hasCachedFile(sha256) && engine.validateCacheFile(sha256)) {
    std::string cached = engine.cacheFilePath(sha256); // 直接用
}
auto r = engine.syncFile(srcPath, relPath, sha256, &token);
// r.success / r.syncedFiles / r.failedFiles / r.messages
```

### 6. 记录与读取最近仓库历史

```cpp
#include "history_store.h"

NeoWorkspace::HistoryStore::saveRecentRepo(
    repoUrl, NeoWorkspace::RepoType::Remote, cacheClonePath);

auto recent = NeoWorkspace::HistoryStore::readRecentRepos();
for (const auto& e : recent) {
    // e.location / e.type / e.cachePath；最多 HistoryStore::MaxRecentRepos (10) 条
}
```

## 注意事项

- **`git status --porcelain` 必须 `-z` 解析**（AGENTS.md 铁律）：非 `-z` 模式下中文路径输出**八进制转义**（`"\344\270\255..."`）、含空格路径**带双引号**，拿该输出再传给 `git add` 必然匹配失败且静默。✅ 已修复（2026-08-30）：`GitOperations::status()` 现已执行 `git status --porcelain -z`（NUL 分隔、路径原样 UTF-8）；CLI `info status` 消费点把 NUL 转 `\n` 再入 JSON，编辑器 Git 信息对话框按 NUL 计数。需要按路径解析的界面侧（如 HiBerGUI 的 `GitPanel`）仍用 `git status -b -z`。
- **`git add` 对已删除文件不生效**（不暂存删除）→ 一律用 `git add -A`（`GitOperations::addAll` 已内置 `-A`）。
- **safe.directory 条目必须正斜杠**（2026-08-20 实测）：`git config --global --add safe.directory` 的反斜杠条目不生效；`trustRepository` 已自动 `\`→`/` 规范化，`isTrustedRepository` 比较时双向规范化且大小写不敏感，`*`（全部信任）视为可信。
- **std::filesystem 路径编码铁律**：MSVC 下 `fs::path::string()/generic_string()` 返回 ANSI/GBK（按进程 locale），中文路径经 Qt `fromStdString`（假定 UTF-8）必乱码。Windows 上 `fs::path ↔ std::string` 一律用 `u8string()`/`fs::u8path()`，Qt 侧 `fromUtf8`。**`u8string()`（native 反斜杠）vs `generic_u8string()`（正斜杠）**：供 JSON/UI 显示的相对路径必须 `generic_u8string()`（Qt `split('/')` 建树），纯磁盘访问才可用 `u8string()`。`file_scanner` 已用 `fs::relative(...).generic_string()` 产出正斜杠相对路径；`resolveDirectory`/`customModsPath`/`serverConfigScanPaths` 亦已 `\`→`/` 规范化。
- **HistoryStore 依赖 QApplication/QCoreApplication**：`historyDir()` 等基于 `QCoreApplication::applicationDirPath()`，必须在构造 app 实例后调用，否则返回空路径。
- **GitOperations 的 git 路径**：默认 `"git"`（PATH）；带内置 Git 的宿主须在启动时 `GitOperations::SetDefaultGitPath(内置 git 路径)`（编辑器 main.cpp 已按 install.conf→system git→tools/git 链解析注入）。✅ 已修复（2026-08-30）：`generateSshKey`/`testSshConnection` 经新增 `executeProgram()` 以 `siblingToolPath("ssh-keygen"/"ssh")` 执行——优先取 git 同目录的 MinGit 伴随工具，否则回退 PATH，参数不再经 git 二进制派发，常规 git 二进制场景下两接口均可用。
- **本地仓库无 remote 跳过 fetch**：`GitOperations::hasRemote(dir) == false` 时同步操作应跳过 fetch（NeoBuild `stepCloneOrFetch` 约定：`No git remote configured, skipping fetch`）。
- **`SyncEngine::syncFile` 的哈希语义**：源文件实测 SHA-256 必须等于 `expectedSha256`，否则按失败处理（防静默错误）；缓存使用前须 `validateCacheFile` 重算（零信任原则）。
- **`excludePatterns` 是 ECMAScript 正则不是通配符**：`std::regex_match` 整串匹配相对路径；非法正则仅告警并忽略。
- **`FileScanner::scanPointers` 的 sha256 取自文件名 stem**（`<sha256>.pointer`），与指针文件内容 JSON 的解析（`parsePointerFile`）一致。
- **`push` 固定带 `-u`**（设置 upstream）；`fetch` 内固定 60s 超时、`clone` 默认 120s。
- **`std::move` 后不得读取被移对象**（项目铁律）；本模块内部已遵循。
- **改共享头文件后须 clean-first 全量重建**：本模块头文件被 NeoBuild/NeoCLI/GUIWorker/编辑器/主程序多处引用，改类成员布局后务必全量重编再冒烟。
- **日志全英文、源码纯 ASCII**：本模块内 `CLogger::*` 消息一律英文（ANSI），新增日志不要写中文字面量（936 代码页下触发 C4819/C2447）。

## 相关文档

- [README.md](README.md) — 模块说明、设计目标、依赖关系、构建集成
- `docs/Modules/README.md` — NSUM 模块文档总索引
- `docs/Modules/NeoBuild/usage.md` — `SyncPolicy` 的执行方（`sync_policy_executor`）、`stepCloneOrFetch` 的 hasRemote 约定
- `docs/CLI/CLI-info.md` / `CLI-exec.md` — CLI 经 NeoCLI 复用本模块（workspace 查询、repo-trust 等）
- `AGENTS.md` — `git status -z`、safe.directory 正斜杠、路径编码铁律、dubious ownership 三端处理
- `PLAN.md` — 功能更新记录表与已知问题表