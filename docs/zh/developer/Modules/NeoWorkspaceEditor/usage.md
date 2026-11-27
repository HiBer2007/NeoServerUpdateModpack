---
title: NeoWorkspaceEditor 使用
description: NeoWorkspaceEditor 的 GUI 启动方式、info/exec CLI 全部子命令、JSON 输出契约与注意事项。
lang: zh
status: complete
updated: 2026-11-27
---

# NeoWorkspaceEditor 使用

## 快速开始

```powershell
# GUI：打开/新建/克隆仓库
NeoWorkspaceEditor.exe                          # 空白窗口，菜单里选择仓库
NeoWorkspaceEditor.exe <仓库>\workspace.json    # 直接打开某个工作区

# CLI（无 GUI）：查询仓库信息，stdout 只有一条 JSON
NeoWorkspaceEditor.exe info workspace --repo <仓库>
NeoWorkspaceEditor.exe info branches  --repo <仓库>
NeoWorkspaceEditor.exe exec validate  --repo <仓库>
```

CLI 判定规则：首个非程序参数是 `info` / `exec` / `help` 时进入 CLI（`editor_cli::is_cli_invocation`），不会创建 `QApplication`。`--repo` 缺省为当前目录，且该目录必须含 `workspace.json`。

## 公共 API

本工具是 EXE，对外契约 = **CLI 子命令 + JSON 输出**。所有命令在 stdout 输出单个 JSON 对象：

```json
{ "ok": true, "command": "info.workspace", "repo": "<仓库>", "data": { } }
```

失败时为 `{ "ok": false, "command": ..., "error": "..." }`（`data` 在可用时附带，例如 git 的 `exit_code`/`stdout`/`stderr`）。`--text` 改为人类可读短摘要（`OK: <command>` / `ERROR: ...` 走 stderr）。退出码：`0` 成功、`1` 操作失败、`2` 用法/参数错误。

### info 子命令

| 命令 | 用法 | 说明 |
|---|---|---|
| `info workspace` | `info workspace [--repo <dir>] [--text]` | 工作区摘要：名称、MC 版本、加载器、分支列表、导出元数据、同步策略 |
| `info branches` | `info branches [--repo <dir>] [--text]` | 分支数组与数量 |
| `info branch` | `info branch <name> [--repo <dir>]` | 单分支详情 + 子分支 + `branch_config` 摘要 + 文件数 |
| `info tree` | `info tree [--path <rel>] [--depth <n>] [--repo <dir>]` | 目录树（默认深度 2） |
| `info pointers` | `info pointers [--repo <dir>]` | 指针文件与 resolver 概况 |
| `info sync` | `info sync [--repo <dir>]` | 有效同步策略 |
| `info file` | `info file <rel> [--base64] [--force]` | 读仓库内文件内容 |
| `info git` | `info git` | 分支/HEAD/上游/ahead-behind/暂存与未跟踪文件 |

### exec 子命令

| 命令 | 用法 | 说明 |
|---|---|---|
| `branch-add` | `exec branch-add --name <n> [--parent <p>] [--type universal\|client\|server] [--core <id>] [--description <s>] [--modloader-version <v>] [--game-version <v>]` | 新建分支并复制默认四段配置 |
| `branch-set` | `exec branch-set --name <n> [--parent <p>\|--parent ""] [--type ...] [--core <id>] [--description <s>] [--modloader-version <v>] [--hidden true\|false] [--game-version <v>]` | 改分支属性 |
| `branch-rename` | `exec branch-rename --name <n> --to <new>` | 重命名（同步目录与配置） |
| `branch-remove` | `exec branch-remove --name <n> [--force] [--purge]` | 删除分支（`--purge` 连数据目录） |
| `file-write` | `exec file-write --path <rel> (--content <text> \| --from-file <src>) [--append]` | 写仓库内文件 |
| `file-delete` | `exec file-delete --path <rel> [--recursive]` | 删除文件/目录 |
| `file-move` | `exec file-move --from <rel> --to <rel>` | 移动/改名 |
| `json-get` | `exec json-get --file <rel> [--pointer /a/b]` | 按 JSON Pointer 取值 |
| `json-set` | `exec json-set --file <rel> --pointer /a/b (--value <json>\|--value-string <s>\|--delete)` | 按 JSON Pointer 改值 |
| `pointer-add` | `exec pointer-add --path <rel> --sha256 <hex> --resolver <name> [--url <url>] [--branch <name>] [--metadata-json <json>]` | 添加指针并更新 manifest |
| `pointer-remove` | `exec pointer-remove --path <rel> [--branch <name>]` | 移除指针 |
| `validate` | `exec validate [--repo <dir>]` | 结构校验（重名/父缺失/父链环） |
| `git-status` | `exec git-status` | git 状态（同 `info git` 数据源） |
| `git-add` | `exec git-add [<path>...] [--all]` | 暂存 |
| `git-commit` | `exec git-commit --message <msg> [--add] [<path>...] [--amend] [--allow-empty] [--no-verify]` | 提交（`--message` 必填） |
| `git-push` | `exec git-push [--remote <r>] [--branch <b>] [--set-upstream] [--force]` | 推送；`--force` 实际用 `--force-with-lease` |
| `git-pull` | `exec git-pull [--remote <r>] [--branch <b>] [--rebase] [--ff-only]` | 拉取 |
| `git-fetch` | `exec git-fetch [--remote <r>] [--prune]` | 抓取 |
| `git-log` | `exec git-log [--limit <n>]` | 历史 |
| `git-diff` | `exec git-diff [--cached] [--stat] [<path>]` | 差异 |
| `git-checkout` | `exec git-checkout --branch <b> [--create] [--start <ref>] [--force]` | 切分支 |
| `git-reset` | `exec git-reset --mode soft\|mixed\|hard --ref <ref> [--confirm-hard]` | 回退；`hard` 必须 `--confirm-hard` |
| `git-config` | `exec git-config --key <k> [--value <v>] [--global]` | 读写 git 配置 |
| `help` | `NeoWorkspaceEditor.exe help` | 打印用法（同时是 CLI 入口之一） |

## 典型用法

### 查询工作区概况

```powershell
NeoWorkspaceEditor.exe info workspace --repo <仓库>
```

```json
{"ok":true,"command":"info.workspace","data":{
  "name":"E2E","minecraft":"1.21.1","modloader":"neoforge","modloader_version":"21.1.209",
  "branch_count":1,
  "branches":[{"name":"base","parent":"","type":"server","core":"paper",
               "description":"e2e","hidden":false,"has_config":true,"has_data_dir":true}],
  "git":{"remote":"","default_branch":"main"},
  "sync_policies":{"default_folder_policy":"mirror",
                   "folders":{"config":"mirror","logs":"skip"},
                   "files":{"config/partial.json":{"mode":"partial","tracked_keys":["keep","server"]}}},
  "workspace_file":"<仓库>/workspace.json"},"repo":"<仓库>"}
```

### 新增分支并校验

```powershell
NeoWorkspaceEditor.exe exec branch-add --name beta --parent base --type server --core paper
NeoWorkspaceEditor.exe exec validate
```

`validate` 的 `data` 形如：

```json
{"ok":true,"command":"exec.validate","data":{
  "loaded":true,"valid":true,"workspace_manager_valid":false,
  "branches":[{"name":"base","parent":"","type":"server","core":"paper",
               "has_config":true,"has_data_dir":true}],
  "errors":[],"warnings":[]}}
```

### 改配置里的单个键（不整体重写文件）

```powershell
NeoWorkspaceEditor.exe exec json-get --file workspace.json --pointer /workspace/name
NeoWorkspaceEditor.exe exec json-set --file workspace.json --pointer /workspace/name --value-string "我的整合包"
```

> JSON Pointer 必须以 `/` 开头，空指针 `/` 被拒绝。

### 指针化一个 jar

```powershell
NeoWorkspaceEditor.exe exec pointer-add --path mods/example.jar --sha256 <sha256> `
    --resolver modrinth --metadata-json '{"project_id":"...","version_id":"..."}'
```

`pointer-add` 会写 `branch_config/<branch>.pointer` 并同步 `file_manifest`/`pointer_files`，与 GUI 的 `PointerEditorPanel` 行为一致（不是你手写的 `branches/<name>/x.pointer`）。

### Git 提交流程

```powershell
NeoWorkspaceEditor.exe exec git-status
NeoWorkspaceEditor.exe exec git-add --all
NeoWorkspaceEditor.exe exec git-commit --message "docs: 更新模块文档"
NeoWorkspaceEditor.exe exec git-push --set-upstream
```

git 失败时 JSON 仍带 `data.exit_code` / `data.stdout` / `data.stderr` 且 `ok:false`，便于脚本直接读原因。

## 注意事项

- **路径限定在仓库内**：`--path`/`--from`/`--to`/`--file` 拒绝绝对路径与 `..`；`info file` 默认拒绝二进制内容；`--base64` 以 base64 返回，`--force` 才允许直接返回原始二进制。
- **CLI 的日志走 stderr**：`run()` 会把默认 logger 换成 stderr sink，stdout 仅含 JSON；解析失败也会保持这一约定。
- **CLI 也要 QCoreApplication**：`install.conf` / 内置 git 的定位依赖 `applicationDirPath()`，所以 CLI 仍构造 `QCoreApplication`（但不建 GUI）。
- **`--force` 不是 `--force` 推送**：`exec git-push --force` 实际走 `--force-with-lease`；`git-reset --mode hard` 必须显式 `--confirm-hard`。
- **`branch-add` 复制默认四段**：`file_manifest` / `pointer_files` / `serverconfig_sync` / `custom_mods`，缺目录/缺 `branch_config` 在 `validate` 里只算 warning。
- **GUI 启动会提升文档**：若 `docs/_editor/<lang>/pack-author` 存在（安装器选了编辑器），启动时被搬进正式文档树；这是「板块 2 仅编辑器释放」的实现，不要在别处手改该目录。
- **改动共享 GUI 类成员后必须全量重建**：编辑器是 `GUIWorker`/`HiBerGUILibrary` 的消费方，旧 `.obj` 会按旧布局分配对象导致关闭时 `HEAP CORRUPTION`（`--clean-first` 后冒烟：启动→等 3–4 秒→关窗→exit 0 且无新 `crash-report`）。
- **CLI 与 GUI 都受崩溃兜底保护**：CLI 模式下 `SetCrashCliMode` 让 `CrashTracker --cli` 输出到当前终端；报告目录为 exe 同级 `crash-report/`。
- **`--text` 只改呈现不改契约**：脚本请用默认 JSON 模式，`--text` 面向人工排查。

## 相关文档

- [README.md](./README.md)
- [仓库编辑器（GUI）](../../../pack-author/editor-gui.md)
- [编辑器 CLI](../../../pack-author/editor-cli.md)
- [ADR 0008 · 编辑器 CLI](../../adr/0008-editor-cli.md)
- [仓库结构](../../../pack-author/repo-structure.md)
