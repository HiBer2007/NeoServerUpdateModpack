# NeoWorkspaceEditor CLI（面向 AI / 自动化）

`NeoWorkspaceEditor.exe` 在首个参数为 `info`、`exec` 或 `help` 时进入无界面 CLI 模式：
不创建 `QApplication`、不加载 Qt 平台插件、不显示窗口，只读写仓库文件并输出 JSON。

> 与 GUI 共用同一份 `workspace.json` / `branch_config/<branch>.json` 格式；
> CLI 本身**不执行 git 操作**（不 add/commit/push），需要提交时由调用方自行运行 git。

## 调用方式

```powershell
NeoWorkspaceEditor.exe <info|exec|help> <subcommand> [--repo <dir>] [options]
```

- `--repo <dir>`：仓库根目录，缺省为当前工作目录；必须包含 `workspace.json`。
- 默认输出**单个 JSON 对象**到 stdout；日志与告警全部走 stderr。
- `--text`：改为人类可读输出（仍含 JSON 数据块）。
- 退出码：`0` 成功；`1` 操作失败；`2` 参数/用法错误。

成功：

```json
{"ok":true,"command":"info.workspace","repo":"K:\\repo","data":{...}}
```

失败：

```json
{"ok":false,"command":"exec.branch-add","error":"branch already exists: x"}
```

## info 子命令（只读）

| 命令 | 说明 |
|------|------|
| `info workspace` | 工作区概览：名称、Minecraft、加载器、git 配置、分支列表、sync_policies、export |
| `info branches` | 分支列表（含 `has_data_dir` / `has_config`） |
| `info branch <name>` | 单个分支 + 其 `branch_config` 统计、子分支、文件数 |
| `info tree [--path <rel>] [--depth <n>]` | 目录树（默认深度 2，跳过 `.git`/`.NSUM`，上限 5000 项） |
| `info pointers` | 指针清单：`sha256`、`resolver`、`metadata`、`targets[{branch,path}]` |
| `info sync` | 顶层 `sync_policies` + 各分支 `serverconfig_sync` / `custom_mods` |
| `info file <rel> [--base64] [--force]` | 读取文件；二进制默认不返回内容，`--base64` 返回 base64 |

## exec 子命令（写入）

### 分支

```powershell
# 新建（parent 可选；server 分支必须给 --core；自动创建 branches/<name>/ 与 branch_config/<name>.json）
NeoWorkspaceEditor.exe exec branch-add --name feature --parent base --description "新分支" --type universal

# 修改（--parent "" 表示改为根分支；--hidden true|false）
NeoWorkspaceEditor.exe exec branch-set --name feature --type server --core paper --hidden false

# 重命名（同步移动 branches/<old> 与 branch_config/<old>.json，并更新子分支 parent）
NeoWorkspaceEditor.exe exec branch-rename --name feature --to feature2

# 删除（有子分支需 --force 自动改挂父分支；--purge 连同 branches/<name>/ 一起删）
NeoWorkspaceEditor.exe exec branch-remove --name feature2 --purge
```

### 文件

```powershell
NeoWorkspaceEditor.exe exec file-write --path branches/base/config/x.json --content "{""a"":1}"
NeoWorkspaceEditor.exe exec file-write --path mods/a.jar --from-file D:\a.jar --append
NeoWorkspaceEditor.exe exec file-move --from branches/base/a.json --to branches/base/b.json
NeoWorkspaceEditor.exe exec file-delete --path branches/base/old.json [--recursive]
```

路径必须是仓库内相对路径；绝对路径与 `..` 会被拒绝。

### JSON 配置

```powershell
NeoWorkspaceEditor.exe exec json-get --file workspace.json --pointer /workspace/minecraft
NeoWorkspaceEditor.exe exec json-set --file workspace.json --pointer /sync_policies/folders/mods --value "\"mirror\""
NeoWorkspaceEditor.exe exec json-set --file branch_config/base.json --pointer /serverconfig_sync/enabled --value true
NeoWorkspaceEditor.exe exec json-set --file workspace.json --pointer /workspace/name --value-string "New Name"
NeoWorkspaceEditor.exe exec json-set --file workspace.json --pointer /obsolete --delete
```

`--value` 先按 JSON 解析（数字/布尔/对象/数组），失败则按字符串；`--value-string` 强制字符串。
指针支持 `~0`/`~1` 转义与数组下标（如 `/branches/0/hidden`）。

### 指针

```powershell
# 写入 branch_config/<sha>.pointer，并登记到 branch_config/<branch>.json 的
# file_manifest[path]=sha 与 pointer_files[sha]={resolver,metadata}
NeoWorkspaceEditor.exe exec pointer-add --path mods/example.jar --sha256 <64位hex> --resolver direct_url --url https://example.com/example.jar --branch base

NeoWorkspaceEditor.exe exec pointer-remove --path mods/example.jar --branch base
```

`--metadata-json '{"url":"..."}'` 可一次给完整 metadata；同 sha 的指针文件存在时会合并。

### 校验

```powershell
NeoWorkspaceEditor.exe exec validate
```

返回 `loaded`、`workspace_manager_valid`、`valid`、`branches`、`errors`、`warnings`：

- `errors`：分支重名、父分支缺失、父链成环 → `valid=false`，退出码 1；
- `warnings`：缺少 `branches/<name>/` 或 `branch_config/<name>.json`（本地仓库常见，不算失败）。

## AI 典型流程

```powershell
# 1. 读取现状
NeoWorkspaceEditor.exe info workspace --repo K:\repo
NeoWorkspaceEditor.exe info branches  --repo K:\repo

# 2. 修改（示例：新增客户端分支并写配置）
NeoWorkspaceEditor.exe exec branch-add --name client --parent base --type client --repo K:\repo
NeoWorkspaceEditor.exe exec file-write --path branches/client/config/client.json --content "{}" --repo K:\repo
NeoWorkspaceEditor.exe exec json-set --file workspace.json --pointer /branches/1/hidden --value false --repo K:\repo

# 3. 校验后再自行提交
NeoWorkspaceEditor.exe exec validate --repo K:\repo
cd K:\repo; git add -A; git commit -m "feat: ..."
```

## 获取帮助

```powershell
NeoWorkspaceEditor.exe help
```
