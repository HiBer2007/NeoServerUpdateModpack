---
title: Editor CLI
description: NeoWorkspaceEditor 的无界面 CLI：分支/文件/JSON/指针操作与内置 Git 命令，供 AI 与脚本使用。
lang: zh
status: complete
updated: 2026-11-27
---

# Editor CLI

`NeoWorkspaceEditor.exe` 首个参数为 `info` / `exec` / `help` 时进入无界面 CLI：不创建窗口，stdout 只输出单个 JSON，日志走 stderr。

## 通用规则

```powershell
NeoWorkspaceEditor.exe <info|exec|help> <subcommand> [--repo <目录>] [options]
```

| 规则 | 说明 |
|---|---|
| `--repo` | 仓库根目录，缺省当前目录；必须含 `workspace.json` |
| 输出 | 成功 `{"ok":true,"command":"...","repo":"...","data":{...}}`；失败 `{"ok":false,"command":"...","error":"..."}` |
| 退出码 | 0 成功 / 1 失败 / 2 参数错误 |
| `--text` | 人类可读输出 |
| 路径 | 只允许仓库内相对路径；拒绝绝对路径与 `..` |
| Git | 内置常用 git 操作，失败仍返回 `data.exit_code/stdout/stderr` |

## info（只读）

```powershell
NeoWorkspaceEditor.exe info workspace --repo <仓库>
NeoWorkspaceEditor.exe info branches --repo <仓库>
NeoWorkspaceEditor.exe info branch <名称> --repo <仓库>
NeoWorkspaceEditor.exe info tree --path branches/base --depth 2 --repo <仓库>
NeoWorkspaceEditor.exe info pointers --repo <仓库>
NeoWorkspaceEditor.exe info sync --repo <仓库>
NeoWorkspaceEditor.exe info file branches/base/config/x.json --repo <仓库>
```

## exec（写入）

### 分支

```powershell
# 新建：parent 可选；server 分支必须给 --core
NeoWorkspaceEditor.exe exec branch-add --name client --parent base --type client --repo <仓库>
NeoWorkspaceEditor.exe exec branch-add --name server --type server --core paper --repo <仓库>

# 修改：--parent "" 改为根分支；--hidden true|false
NeoWorkspaceEditor.exe exec branch-set --name client --parent base --hidden false --repo <仓库>

# 重命名（同步移动 branches/ 与 branch_config/，更新子分支 parent）
NeoWorkspaceEditor.exe exec branch-rename --name client --to client-lite --repo <仓库>

# 删除（有子分支需 --force 自动改挂父分支；--purge 删除内容目录）
NeoWorkspaceEditor.exe exec branch-remove --name client-lite --purge --repo <仓库>
```

### 文件与 JSON

```powershell
NeoWorkspaceEditor.exe exec file-write --path branches/base/config/x.json --content "{""a"":1}" --repo <仓库>
NeoWorkspaceEditor.exe exec file-write --path mods/a.jar --from-file D:\a.jar --repo <仓库>
NeoWorkspaceEditor.exe exec file-move --from branches/base/a.json --to branches/base/b.json --repo <仓库>
NeoWorkspaceEditor.exe exec file-delete --path branches/base/old.json --repo <仓库>

NeoWorkspaceEditor.exe exec json-get --file workspace.json --pointer /workspace/minecraft --repo <仓库>
NeoWorkspaceEditor.exe exec json-set --file workspace.json --pointer /sync_policies/folders/mods --value "\"mirror\"" --repo <仓库>
NeoWorkspaceEditor.exe exec json-set --file workspace.json --pointer /obsolete --delete --repo <仓库>
```

`--value` 先按 JSON 解析，失败按字符串；`--value-string` 强制字符串；指针支持 `/数组下标` 与 `~0`/`~1` 转义。

### 指针

```powershell
NeoWorkspaceEditor.exe exec pointer-add --path mods/example.jar --sha256 <64位hex> `
    --resolver direct_url --url https://example.com/example.jar --branch base --repo <仓库>

NeoWorkspaceEditor.exe exec pointer-remove --path mods/example.jar --branch base --repo <仓库>
```

写入 `branch_config/<sha>.pointer`，并更新 `branch_config/<branch>.json` 的 `file_manifest` 与 `pointer_files`。

### Git

```powershell
NeoWorkspaceEditor.exe exec git-status --repo <仓库>
NeoWorkspaceEditor.exe exec git-add --all --repo <仓库>
NeoWorkspaceEditor.exe exec git-commit --message "feat: ..." --repo <仓库>
NeoWorkspaceEditor.exe exec git-push --set-upstream --repo <仓库>
NeoWorkspaceEditor.exe exec git-log --limit 10 --repo <仓库>
NeoWorkspaceEditor.exe exec git-diff --stat --repo <仓库>
NeoWorkspaceEditor.exe exec git-checkout --branch client --create --repo <仓库>
NeoWorkspaceEditor.exe exec git-reset --mode mixed --ref HEAD --repo <仓库>
NeoWorkspaceEditor.exe exec git-config --key user.name --repo <仓库>
```

安全边界：

- 推送只提供 `--force-with-lease`，不提供强推；
- `git-reset --mode hard` 必须显式 `--confirm-hard`；
- `git-commit` 必须提供 `--message`；
- 需要凭据时不会交互挂起（`GIT_TERMINAL_PROMPT=0`）。

## AI 工作流示例

```powershell
# 1. 读取
NeoWorkspaceEditor.exe info branches --repo K:\repo

# 2. 修改
NeoWorkspaceEditor.exe exec branch-add --name client --parent base --type client --repo K:\repo
NeoWorkspaceEditor.exe exec file-write --path branches/client/config/client.json --content "{}" --repo K:\repo

# 3. 校验
NeoWorkspaceEditor.exe exec validate --repo K:\repo

# 4. 提交与推送
NeoWorkspaceEditor.exe exec git-add --all --repo K:\repo
NeoWorkspaceEditor.exe exec git-commit --message "feat: add client branch" --repo K:\repo
NeoWorkspaceEditor.exe exec git-push --repo K:\repo
```

## validate

```powershell
NeoWorkspaceEditor.exe exec validate --repo <仓库>
```

- 错误：分支重名、父分支缺失、父链成环；
- 警告：缺少 `branches/<name>/` 或 `branch_config/<name>.json`；
- 返回 `packFormat` 无关；仅校验仓库结构。

## 帮助

```powershell
NeoWorkspaceEditor.exe help
```
