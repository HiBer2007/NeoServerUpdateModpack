# exec —— 执行操作命令详解（CLI-exec）

> exec 执行实际操作（构建 / 导出 / 同步 / 仓库校验 / 指针解析 / 崩溃测试 / Git 安装）。
> `--json` 支持面：**仅** `verify-repo`、`resolve-pointer`、`crash-test`、`git-update`、`repo-trust`、`repo-trust-check`；
> `build`、`export`、`sync-serverconfig` 为人类输出（`--json` 传入被忽略）。
> 统一 JSON 外壳：`{ "category": "exec", "command": "<verb>", "data": {...} }`。

---

## 1. `exec build`

构建整合包（可同时导出压缩包）。

```
NeoServerUpdateModpack.exe exec build --repo <url|path> --modpack <branch> [--git-branch <b>] [--format <f>] [--export <path>]
```

**必配参数：**

| 参数 | 必配 | 说明 |
|------|:---:|------|
| `--repo` | ✅ | 仓库；缺失 → `No repository URL specified. Use --repo <url>.` exit 2 |
| `--modpack` | ✅ | 整合包分支；缺失 → `No modpack branch specified. Use --modpack <branch>.` exit 2 |

**可选参数：**

| 参数 | 默认 | 说明 |
|------|------|------|
| `--git-branch` | — | clone/fetch 后 checkout 的分支 |
| `--format` | `mcbbs` | `mcbbs\|modrinth\|hmcl`；非法 → `Unsupported format: <f>. Use mcbbs, modrinth, or hmcl.` exit 2 |
| `--export <path>` | — | 导出路径（**仅 mcbbs/modrinth 生效**；hmcl 下作为同步目录） |

### 行为（两分支）

**格式 = mcbbs / modrinth**（压缩包模式）：

1. 克隆/更新仓库到缓存（见 CLI-usage 第 5 节）
2. 检查 workspace.json；加载对应格式的导出插件（找不到 → exit 1）
3. 虚拟工作目录 `.minecraft/versions/<branch>` 构建（合并继承链）
4. **若给了 `--export <path>`** → 再调 `export_modpack` 生成压缩包到该路径（成功后 `[+] Exported to: <path>`）
5. 无 `--export` → 仅构建到工作目录（不下发产物到别处）

**格式 = hmcl**（同步目录模式）：

1. `--export` 给了 → 直接同步到该目录；没给 → 同步到 `.minecraft/versions/<branch>`
2. 不产出压缩包

### 输出（人类模式，无 JSON）

```
================ Build Modpack ================
[*] Repository: https://example.com/repo.git
[*] Modpack branch: client-HBNS
[*] Cloning repository: ...
[+] Repository cloned.
[*] Starting build...
[+] Build completed successfully.
[*] Output directory: C:/.../.minecraft/versions/client-HBNS
[*] N files synced, M failed.
[+] Exported to: C:/out/pack.zip
```

### 退出码

| 场景 | exit |
|------|------|
| 成功 | 0 |
| Ctrl+C 取消（构建前后任一检查点） | 0（`[!] Build cancelled by user.`） |
| 缺 `--repo` / `--modpack` / 非法 `--format` | 2 |
| clone 失败 / workspace.json 缺失 / 导出插件缺失 / 构建失败 / 导出失败 | 1 |

> 提示：`exec build` 不输出 JSON；脚本若需要结构化结果，用 `exec verify-repo --json`（构建前校验）
> 或 `flow ... --to checklist`（收集参数）组合。

---

## 2. `exec export`

导出已构建产物（或先构建再导出）。

```
NeoServerUpdateModpack.exe exec export --export <path> --format <f> [--repo <url> --modpack <b>] [--git-branch <b>]
```

**必配参数：**

| 参数 | 必配 | 说明 |
|------|:---:|------|
| `--export <path>` | ✅ | 导出路径；缺失 → `No export path specified. Use --export <path>.` exit 2 |
| `--format <f>` | ✅ | 格式；缺失 → `No export format specified. Use --format <mcbbs\|modrinth\|hmcl>.` exit 2；非法值 exit 2 |

**可选参数：** `--repo`、`--modpack`、`--git-branch`。

### 双模式

**模式 A：构建后导出**（`--repo` 与 `--modpack` 都给了）

1. 克隆/更新缓存 → 构建 → 再导出到 `--export`
   （hmcl：构建即同步到 `--export` 或 `.minecraft/versions/<branch>`）

**模式 B：独立导出**（只给 `--export --format`）

1. 定位工作区：
   - 给了 `--repo` → 用缓存路径 `cache/repos/<slug>`
   - 否则当前目录有 `workspace.json` → 用当前目录
   - 否则当前目录父级有 `workspace.json` → 用父目录
   - 都没有 → `[-] No workspace found. Provide --repo <url> or run from a workspace directory.` exit 1
2. 定位已构建产物：
   - 给了 `--modpack` → `.minecraft/versions/<modpack>`
   - 没给 → 扫描 `.minecraft/versions/*` 下**含 `mods` 子目录**的第一个版本目录
   - 找不到 → `[-] No previously-built output found. Build first with --repo --modpack.` exit 1
3. 用导出插件把产物打包到 `--export`

**hmcl**：无论哪种模式都不打包——`[+] Workspace sync completed: <dir>` exit 0（构建/同步语义）。

### 退出码

| 场景 | exit |
|------|------|
| 成功 / hmcl 同步完成 | 0 |
| Ctrl+C 取消 | 0 |
| 缺 `--export` / `--format` / 非法格式 | 2 |
| 仓库克隆失败 / workspace.json 缺失 / 无工作区 / 无已构建产物 / 构建失败 / 导出失败 | 1 |

---

## 3. `exec sync-serverconfig`

从本地存档把服务器配置同步回仓库。

```
NeoServerUpdateModpack.exe exec sync-serverconfig --save <world_name> [--repo <url>] [--git-branch <b>]
```

**必配参数：** `--save <world_name>`（缺失 → `No save world specified. Use --save <world_name>.` exit 2）。

**可选参数：** `--repo`（仓库路径）、`--git-branch`。

### 存档目录解析

| 条件 | saves 目录 |
|------|-----------|
| 给了 `--repo` | `缓存仓库/.minecraft/saves`（缓存目录必须已克隆，否则 `Repository not cloned. Clone first.` exit 1） |
| 未给 `--repo` | 依次探测：`cwd/.minecraft/saves` → `cwd/saves` → `父目录/.minecraft/saves` → 兜底 `cwd/saves` |

**行为**：

1. 世界目录 = `<saves>/<save_world>`；不存在 → `[-] Save world not found: <path>` exit 1
2. 分支：`--git-branch` 或仓库当前分支
3. 无对应分支的 serverconfig 规则 → `[!] No serverconfig rules found for branch: <b>` exit 0
4. 扫描世界内配置文件 → 逐条同步（进度条 + `[!] Failed to sync: <rel>`）

### 退出码

| 场景 | exit |
|------|------|
| 全部同步成功 / 无规则 / 无配置文件 | 0 |
| 有失败条目 | 1（`[!] Synced X / N files. M failed.`） |
| 缺 `--save` | 2 |
| 仓库未克隆 / 世界不存在 / sync 初始化失败 | 1 |

---

## 4. `exec verify-repo`

仓库完整性校验（clone/fetch + git 判定 + workspace.json + 分支数 + HEAD）。

```
NeoServerUpdateModpack.exe exec verify-repo --repo <url|path> [--git-branch <b>] [--json]
```

**必配参数：** `--repo`（缺失 → `No repository URL specified. Use --repo <url>.` exit 2）。

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "exec",
  "command": "verify-repo",
  "data": {
    "repo": "https://example.com/repo.git",
    "workdir": "C:/.../cache/repos/https___example.com_repo.git",
    "is_git_repository": true,
    "has_workspace": true,
    "workspace_json": "C:/.../cache/repos/https___example.com_repo.git/workspace.json",
    "branch_count": 1,
    "head": "6adc093d6295d5760f510129b2ada7485e2f4c46",
    "valid": true
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `repo` | 传入的仓库参数 |
| `workdir` | 缓存目录（可能刚被 clone） |
| `is_git_repository` | 是否为合法 git 仓库 |
| `has_workspace` | 仓库根是否有 workspace.json |
| `workspace_json` | 有则为路径，否则空串 |
| `branch_count` | `git branch` 输出行数 |
| `head` | `git rev-parse HEAD` 哈希（失败为空串） |
| `valid` | `is_git_repository && has_workspace` |

**退出码：** `valid=true` → 0；`valid=false` → 1（人类模式 `[+] Repository is valid.` / `[-] Repository is invalid.`）；缺 `--repo` → 2。

**失败示例（exit 1，如非仓库/无 workspace 的 URL）：**

```
=====JSON-BEGIN=====
{
  "category": "exec",
  "command": "verify-repo",
  "data": {
    "repo": "...",
    "workdir": "...",
    "is_git_repository": true,
    "has_workspace": false,
    "workspace_json": "",
    "branch_count": 1,
    "head": "...",
    "valid": false
  }
}
=====JSON-END=====
```

---

## 5. `exec resolve-pointer`

把指针文件（`<sha256>.pointer`，缺失文件的哈希占位符）解析为实际下载 URL。

```
NeoServerUpdateModpack.exe exec resolve-pointer <file.pointer> [--json]
```

**必配参数：** 位置参数指针文件路径（缺失 → `No pointer file specified. Usage: exec resolve-pointer <file.pointer>` exit 2）。

**成功 JSON（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "exec",
  "command": "resolve-pointer",
  "data": {
    "file": "mods/abc123....jar.pointer",
    "sha256": "0123456789abcdef...",
    "resolver": "direct-url",
    "url": "https://example.com/example.jar",
    "success": true
  }
}
=====JSON-END=====
```

**失败 JSON（exit 1）：**

```
=====JSON-BEGIN=====
{
  "category": "exec",
  "command": "resolve-pointer",
  "data": {
    "file": "mods/abc123....jar.pointer",
    "sha256": "0123456789abcdef...",
    "resolver": "",
    "url": "",
    "success": false,
    "error": "No matching resolver found for 'modrinth'"
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `file` | 指针文件路径 |
| `sha256` | 指针内 sha256（缺失 → 前置报错 exit 1） |
| `resolver` | 选中的解析器 id（无匹配/异常时为空） |
| `url` | 成功时为 URL，否则空串 |
| `success` | 是否解析成功 |
| `error` | 仅失败时出现：无匹配 resolver / resolver 抛异常 / 返回空 URL 的区分信息 |

**解析器扫描**：`<exe>/pointers/*.meta.json` + DLL（direct-url / modrinth）。
**错误前置检查**：文件打不开 / JSON 非法 / 缺 `sha256` / 无 `resolvers` 数组 → 各自报错 exit 1。

---

## 6. `exec crash-test`

故意崩溃，验证崩溃处理（crash handler / CrashTracker 报告链路）。

```
NeoServerUpdateModpack.exe exec crash-test [--json]
```

**行为**：日志倒计时 3 秒（CRASH TEST: 3/2/1）→ 日志 BOOM → 空指针写入崩溃。
**进程退出码 = 0xC0000005**（ACCESS_VIOLATION），崩溃报告写入 `<exe>/crash-report/<date_time>/`。

**`--json` 时崩溃前先输出标记块（exit 0xC0000005，块在崩溃前出）：**

```
=====JSON-BEGIN=====
{
  "category": "exec",
  "command": "crash-test",
  "data": {
    "message": "Crashing in 3 seconds to generate a crash report",
    "note": "Crash report will be written under the crash-report directory"
  }
}
=====JSON-END=====
```

> 用于自动化验证：先确认拿到 JSON 块，再确认 crash-report 目录新增了报告。

---

## 7. `exec git-update`

安装/更新内置 Git（MinGit）并写入安装配置 `install.conf`。

```
NeoServerUpdateModpack.exe exec git-update [--json]
```

### 模式

**模式 1：系统 Git 模式**（配置 `use_system_git=true`）——不做任何安装：

```
=====JSON-BEGIN=====
{
  "category": "exec",
  "command": "git-update",
  "data": {
    "installed": false,
    "use_system_git": true,
    "message": "System git mode — nothing to update"
  }
}
=====JSON-END=====
```

**exit 1**（人类：`[!] System git mode — nothing to update.`）——这是"预期非零"，不是故障。

**模式 2：内置 Git 已存在**（`<root>/tools/git` 有可运行 git.exe）——复用并重写配置：

```
=====JSON-BEGIN=====
{
  "category": "exec",
  "command": "git-update",
  "data": {
    "installed": true,
    "already_installed": true,
    "use_system_git": false,
    "install_root": "C:/.../NeoServerUpdateModpack",
    "git_path": "C:/.../tools/git/mingw64/bin/git.exe",
    "git_version": "git version 2.55.0.windows.3",
    "install_conf": [
      "C:/.../NeoServerUpdateModpack/install.conf"
    ]
  }
}
=====JSON-END=====
```

**exit 0**。

**模式 3：全新安装**——PowerShell 查 GitHub 最新 MinGit 64 位 zip → 下载到临时目录 →
解压（tar，失败回退 Expand-Archive）→ 定位可运行 git.exe → 校验版本 → 写配置：

```
=====JSON-BEGIN=====
{
  "category": "exec",
  "command": "git-update",
  "data": {
    "installed": true,
    "already_installed": false,
    "use_system_git": false,
    "install_root": "...",
    "git_path": ".../tools/git/mingw64/bin/git.exe",
    "git_version": "git version 2.55.0.windows.3",
    "install_conf": [
      "C:/.../install.conf"
    ]
  }
}
=====JSON-END=====
```

**exit 0**。

| 字段 | 说明 |
|------|------|
| `installed` | 安装后是否可用 |
| `already_installed` | 是否原本就有 |
| `use_system_git` | false（本命令只管内置 Git） |
| `install_root` | 布局探测的安装根目录（已安装布局 = exe 目录；dev/deploy 布局 = exe 上一级） |
| `git_path` | 定位到的 git.exe（MinGit 布局为 `mingw64/bin/git.exe`，PortableGit 为 `bin/git.exe`，兜底 `cmd/git.exe`） |
| `git_version` | `git --version` 输出 |
| `install_conf` | 实际写入的 install.conf 路径数组（真根目录单文件） |

**错误（exit 1）：** 获取 GitHub URL 失败 `[-] Failed to get Git URL`；下载失败 `[-] Download failed`；
解压后找不到可运行 git.exe `[-] Git install failed: no runnable git.exe found under <dir>`。

> 布局说明：已安装布局（exe 同级有 `tools/git` 或 `install.conf`）→ 根 = exe 目录，只写
> `exe/install.conf`；dev/deploy 布局 → 根 = exe 上一级，只写根 `install.conf`。
> 内置 Git 布局：MinGit 无顶层 `bin/`，git.exe 在 `mingw64/bin/` 与 `cmd/`；系统 `C:\Program Files\Git\cmd\git.exe` 是 shim，不能单独拷贝。

---

## 8. `exec repo-trust`

信任一个 Git 仓库（写入 `git config --global safe.directory`），用于解决
**陌生仓库（dubious ownership）** 问题：仓库从其他设备/账户复制而来时，git 会拒绝所有
命令（`detected dubious ownership in repository at '...'`），此命令将仓库加入全局信任列表。

```
NeoServerUpdateModpack.exe exec repo-trust --repo <path> [--json]
```

| 参数 | 必配 | 说明 |
|------|:---:|------|
| `--repo` | ✅ | 本地仓库路径（也接受 URL，自动解析到缓存目录） |

**输出（人类模式）：**

```
[+] Repository trusted: K:/path/to/repo
```

**输出（`--json`）：**

```
=====JSON-BEGIN=====
{
  "category": "exec",
  "command": "repo-trust",
  "data": {
    "path": "K:/path/to/repo",
    "trusted": true,
    "already_trusted": false
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `path` | 实际操作的仓库路径（`resolveWorkDir` 解析结果） |
| `trusted` | 操作后是否受信任 |
| `already_trusted` | 是否原本就在信任列表中（无需改动） |

**退出码：** 成功（含已信任）exit 0；`--repo` 缺失 exit 2；`git config` 失败 exit 1。

> **陌生仓库行为（所有涉及仓库的命令）**：`ensureRepoCloned` 检测到 dubious ownership 时，
> 输出警告 `[-] Repository ownership is dubious (untrusted repository).` +
> `[-] Run 'exec repo-trust --repo <path>' to trust it, then retry.` 并退出（exit 1），
> 不再误报"克隆失败/仓库不存在"。信任后重试即可。

---

## 9. `exec repo-trust-check`

检查一个 Git 仓库是否已被信任（是否处于陌生仓库状态）。

```
NeoServerUpdateModpack.exe exec repo-trust-check --repo <path> [--json]
```

**输出（人类模式）：**

```
[*] Repository: K:/path/to/repo
[*] Trusted: no
[*] Dubious ownership: yes
[-] Repository is not trusted. Run: exec repo-trust --repo <path>
```

**输出（`--json`）：**

```
=====JSON-BEGIN=====
{
  "category": "exec",
  "command": "repo-trust-check",
  "data": {
    "path": "K:/path/to/repo",
    "trusted": false,
    "is_git_repository": false,
    "dubious_ownership": true
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `path` | 检查的仓库路径 |
| `trusted` | 是否在全局 `safe.directory` 信任列表中 |
| `is_git_repository` | `git rev-parse --git-dir` 是否成功（未信任的陌生仓库为 false） |
| `dubious_ownership` | 是否处于 dubious ownership（陌生仓库）状态 |

**退出码：** 仓库可用（未触发陌生仓库，或已在信任列表）exit 0；处于陌生仓库且未信任 exit 1
（提示运行 `exec repo-trust`）；`--repo` 缺失 exit 2。

---

## 附：exec 必配参数总表

| 命令 | 必配 | 可选 | `--json` |
|------|------|------|:---:|
| `build` | `--repo` `--modpack` | `--git-branch` `--format` `--export` | ❌ |
| `export` | `--export` `--format` | `--repo` `--modpack` `--git-branch` | ❌ |
| `sync-serverconfig` | `--save` | `--repo` `--git-branch` | ❌ |
| `verify-repo` | `--repo` | `--git-branch` | ✅ |
| `resolve-pointer` | `<file.pointer>` | — | ✅ |
| `crash-test` | — | — | ✅ |
| `git-update` | — | — | ✅ |
| `repo-trust` | `--repo` | — | ✅ |
| `repo-trust-check` | `--repo` | — | ✅ |
