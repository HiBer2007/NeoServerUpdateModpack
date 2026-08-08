# flow —— 流程控制命令详解（CLI-flow）

> flow 系列把"仓库 → 分支 → 整合包 → 导出格式 → 导出目录 → 额外字段"的收集过程做成向导，
> 结束时输出统一 JSON 块（**无需 `--json`，恒输出**），供脚本消费。
> 两种实现：`flow gui`（复用 GUI WizardWindow）与 `flow console`（纯文本引导，可管道化）。

```
NeoServerUpdateModpack.exe flow gui [--from <page> --to <page> --collect-only --prefill k=v ...]
NeoServerUpdateModpack.exe flow console [--from <page> --to <page> --prefill k=v ...]
```

## 1. 页面模型（两个命令共用）

页面名 → 索引（`WizardWindow::pageNameToIndex`）：

| 页名 | 索引 | 收集内容 | prefill 键 |
|------|:---:|---------|-----------|
| `repo` | 0 | 仓库 URL / 本地路径 | `repo` |
| `branch` | 1 | Git 分支 | `branch` |
| `modpack` | 2 | 整合包（workspace 非 hidden 分支） | `modpack` |
| `export-type` | 3 | 导出格式（mcbbs/modrinth/hmcl） | `format` |
| `export-dir` | 4 | 导出目录 | `exportdir` |
| `extra-info` | 5 | 导出格式定义的额外字段 | 字段键（`name`/`version`/`summary`/`author`/`description`…） |
| `checklist` | 6 | 清单汇总页（**绝不触发构建**） | — |
| `build` | 7 | （flow 不构建） | — |
| `done` | 8 | 完成页 | — |

- `--prefill k=v` 可重复；键如上表（`exportdir` 是**键名**，对应页是 export-dir）。
- 额外字段键以所选格式的 exporter meta `fields[].key` 为准
  （`info exporters --json` 可查；mcbbs/modrinth 必填 `name`、`version`）。
- **预填页自动跳过**：`--from` 未指定时，起始页 = 第一个未被预填满足的页；
  到达终点页后收集数据 → JSON → 退出。

### `--from` / `--to` 规则

| 参数 | gui | console |
|------|-----|---------|
| `--from <page>` | 起始页（合法页名；非法 → `Invalid --from page: '<x>'. Use --help for usage.` exit 2） | 同左，但**上限 `checklist`**（`--from build` → exit 2） |
| `--to <page>` | 终点页；**默认 `done`**（非法 exit 2） | 终点页；**默认 `checklist`**；上限 `checklist`（`--to build/done` → 明确报错 exit 2） |
| `--collect-only` | ✅ 终点页被钳制到 `checklist`（即使 `--to build/done`），绝不构建 | 不支持（console 本就不构建） |

> `--from` 晚于 `--to` 时以 `--to` 为准（from 被钳到 to）。
> `flow console` 起始页 > repo 时必须 `--prefill repo=<...>`，否则
> `[-] repo is required before the branch page. Pass --prefill repo=<url|path>.` exit 2。

### `--prefill` 合法性

每个 `k=v` 必须**同时含键和值**（`--prefill repo` 或 `--prefill "=x"` 或 `--prefill "repo="` 均报错）：

```
Invalid --prefill '<kv>'. Expected key=value. Use --help for usage.   ← exit 2
```

## 2. `flow gui`

复用 GUI WizardWindow（需要 GUI 会话；进程会 `AttachConsole` 输出日志到终端）。

### 行为

- 启动后进入 flow 模式；预填满足的页自动选择并前进（repo prefill → 同步加载分支 →
  选分支 → 选整合包 → 选格式 → 填目录 → 填额外字段）。
- **全预填（含终点页所需全部数据）时 headless**：init 阶段直接完成，不显示窗口、不闪现，
  直接输出 JSON 块 exit 0。
- 未全预填时显示向导窗口；用户在终点页点"完成"（flow 模式下 Next 文案变为"完成"）→ 收集 JSON → 关闭 → exit 0。
- 用户中途关闭窗口 → stderr `Flow cancelled.` → **exit 1**。
- 自动化测试钩子：环境变量 `NSUM_FLOW_AUTOFINISH=1` → 启动 1.5s 后自动触发"下一步"。

### 参数速查

| 参数 | 必配 | 说明 |
|------|:---:|------|
| `--from <page>` | 否 | 起始页（非法 exit 2） |
| `--to <page>` | 否 | 终点页，默认 `done`（非法 exit 2） |
| `--collect-only` | 否 | 钳制终点到 checklist，绝不构建 |
| `--prefill k=v` | 否（可重复） | 预填值；非法格式 exit 2 |

### 输出 JSON（`{category:flow, command:gui, ...}`）

```
=====JSON-BEGIN=====
{
  "category": "flow",
  "command": "gui",
  "data": {
    "repo": "https://example.com/repo.git",
    "repo_local_path": "C:/Users/bob_2/AppData/Local/NeoServerUpdateModpack/cache/repos/https___example.com_repo.git",
    "branch": "master",
    "modpack": "client-HBNS",
    "format": "mcbbs",
    "export_dir": "C:/out/client-HBNS_modpack.zip",
    "extra": {
      "name": "MyPack",
      "version": "1.0.0"
    }
  }
}
=====JSON-END=====
```

| 字段 | 出现条件 | 说明 |
|------|---------|------|
| `repo` | 恒有 | 用户输入的仓库（URL 或路径） |
| `repo_local_path` | 恒有 | 解析后的本地路径（远程仓库 = 缓存克隆路径；本地路径 = 原路径） |
| `branch` | 恒有 | Git 分支 |
| `modpack` | 恒有 | 整合包分支名 |
| `format` | 恒有 | `mcbbs` / `modrinth` / `hmcl` |
| `export_dir` | 恒有 | 最终导出路径（见下方组成规则） |
| `extra` | 所选格式定义了 fields 且收集到值时 | 额外字段 `{键: 值}` 对象 |

### export_dir 组成规则

| format | 结果 |
|--------|------|
| `mcbbs` | `<导出目录>/<modpack>_modpack.zip` |
| `modrinth` | `<导出目录>/<modpack>_modpack.mrpack` |
| `hmcl` | `<导出目录>`（原样，目录同步模式） |

### 退出码

| 场景 | exit |
|------|------|
| 数据收集完成（headless 或窗口完成） | 0 |
| 中途关闭窗口 | 1（`Flow cancelled.`） |
| 非法 `--from/--to` 页、非法 `--prefill` | 2（在 QApplication 创建前拦截，输出到 stderr） |

### 示例

```powershell
# 全预填 headless（mcbbs + 必填额外字段），直接出 JSON
NeoServerUpdateModpack.exe flow gui --to checklist --collect-only --prefill repo=C:/repo --prefill branch=master --prefill modpack=client-HBNS --prefill format=mcbbs --prefill exportdir=C:/out --prefill name=MyPack --prefill version=1.0.0

# 只让用户选仓库和整合包分支
NeoServerUpdateModpack.exe flow gui --from repo --to modpack

# 只显示清单页（点完成收集 JSON，不构建）
NeoServerUpdateModpack.exe flow gui --to checklist
```

## 3. `flow console`

无 GUI 纯文本引导（QCoreApplication，无 QApplication）。逐页提示输入；**可管道化 / headless**。

### 参数速查

| 参数 | 必配 | 说明 |
|------|:---:|------|
| `--from <page>` | 否 | 起始页；上限 `checklist`（`--from build` → exit 2） |
| `--to <page>` | 否 | 终点页，**默认 `checklist`**；上限 `checklist`（`--to build/done` → `--to '<x>' is not supported by console (max is 'checklist').` exit 2） |
| `--prefill k=v` | 否（可重复） | 预填值；非法格式 exit 2 |

> console 无 `--collect-only`（本就无构建能力）。

### 交互输入规则

- **选择菜单**（分支/整合包/导出格式）：数字菜单 `[1] <选项> ...` + `[q] cancel`。
  输入编号（`1`）或精确选项值均可；非法输入 → `[!] Invalid selection: <t>` 重提示。
- **文本输入**（repo / export-dir / 额外字段）：`<提示> > ` 后输入；空白行不允许（必填）或跳过（可选）。
- **取消**：任意菜单输入 `q` / `quit` / `cancel` → `Flow cancelled.` **exit 1**。
- **EOF（stdin 结束）语义**：
  - 可选字段（`required:false`，如 author/description/summary）→ 视为跳过（空值）
  - 必填字段（`required:true`，如 mcbbs/modrinth 的 name/version）→ 视为取消 → `Flow cancelled.` exit 1
- `--json` 模式下提示文本输出到 **stderr**，stdout 只留 JSON 块。

### 额外字段提示来源

与 `info exporters` 的 `fields` 一致：mcbbs/modrinth 会提示 `name`（必填）、`version`（必填），
以及可选字段；hmcl 无 fields → 不提示额外字段。

### 输出 JSON（`{category:flow, command:console, ...}`）

结构与 gui 完全相同，仅 `command` 不同：

```
=====JSON-BEGIN=====
{
  "category": "flow",
  "command": "console",
  "data": {
    "repo": "C:/repo",
    "repo_local_path": "C:/repo",
    "branch": "master",
    "modpack": "client-HBNS",
    "format": "modrinth",
    "export_dir": "C:/out/client-HBNS_modpack.mrpack",
    "extra": {
      "name": "MP",
      "summary": "desc",
      "version": "1.1"
    }
  }
}
=====JSON-END=====
```

字段说明与 export_dir 组成规则同 gui（见上表）。

### 本地仓库的例外行为

`flow console` 是唯一直接使用本地仓库原目录的路径：`--prefill repo=C:\x` 且该目录含 `.git`
→ `repo_local_path = C:\x`（**不克隆到缓存**，未提交改动可见）。
其余仓库形态（http/git@ssh 开头 → 克隆到缓存；`file://` → 转本地路径；普通存在的路径 → 直接使用）。

### 退出码

| 场景 | exit |
|------|------|
| 收集完成 | 0 |
| 任意处取消（q/quit/cancel / 必填字段 EOF） | 1（`Flow cancelled.`） |
| 非法页 / 非法 prefill / 起始页非 repo 但未给 repo prefill | 2 |
| 仓库不存在 / 无分支 / 无整合包 / 无导出插件 / workspace.json 缺失或解析失败 | 1 |

### 示例

```powershell
# 全预填 headless：直接输出 JSON（无任何交互）
NeoServerUpdateModpack.exe flow console --prefill repo=C:/repo --prefill branch=master --prefill modpack=client-HBNS --prefill format=mcbbs --prefill exportdir=C:/out --prefill name=Pack --prefill version=1.0.0

# 部分预填 + 交互：只交互选导出目录及以后
NeoServerUpdateModpack.exe flow console --to export-dir --prefill repo=C:/repo --prefill branch=master --prefill modpack=client-HBNS

# 管道化交互输入（分支=1，整合包=3，格式=2，目录=C:\out，name/version=..., 可选字段留空）
# 注意：换行分隔的 stdin 行；可选字段空行=跳过，必填字段空行=重提示
```

## 4. 共同注意点

- `repo` 为本地路径但**不带 `.git`**（如 `C:\x` 无 `.git` 子目录）且路径存在 → 视为工作区目录直接使用
  （无法在其中列 git 分支时，branch 页会报错退出 1）。
- `repo` 既不是路径也不是 URL → `[-] Repository not found: <repo>` exit 1。
- 整合包列表来自 `workspace.json` 的**非 hidden 分支**（`hidden:true` 的分支不出现）。
- flow 模式**绝不构建**（终点页永远只是收集；`--collect-only`/console 上限双保险）。
- GUI 与 console 输出字段顺序一致（repo / repo_local_path / branch / modpack / format / export_dir / extra?）。
