# NeoServerUpdateModpack CLI 文档组

> 本文档组覆盖 CLI 的全部工作模式、工具与信息获取命令。
> 阅读顺序：**总览（本文）→ 基础（CLI-usage）→ 按类别深读（CLI-info / CLI-flow / CLI-exec）→ 出错排查（CLI-errors）**。

| 文档 | 内容 |
|------|------|
| [CLI.md](CLI.md)（本文） | 命令总览表、类别索引、快速上手 |
| [CLI-usage.md](CLI-usage.md) | 参数解析规则、帮助/版本写法、退出码、JSON 协议、stdout/stderr 路由、仓库缓存与克隆语义、Ctrl+C 取消 |
| [CLI-info.md](CLI-info.md) | `info` 13 个信息获取命令：参数、必配项、完整 JSON schema、成功/失败示例 |
| [CLI-flow.md](CLI-flow.md) | `flow gui` / `flow console` 向导流程：页面语义、`--prefill`、交互规则、JSON、取消 |
| [CLI-exec.md](CLI-exec.md) | `exec` 9 个执行命令：参数、必配项、双模式、JSON schema、错误 |
| [CLI-errors.md](CLI-errors.md) | 退出码速查、常见错误信息、参数搭配问题、故障排查 |

---

## 1. 是什么

`NeoServerUpdateModpack.exe` 是 HMCL 新服务器自动更新整合包的后端控制器。
CLI 采用**三类互斥子命令**架构：

```
NeoServerUpdateModpack.exe <category> <verb> [options]
```

- `info` —— 信息获取（只读，结构化 JSON 输出）
- `flow` —— 流程控制（复用 GUI 向导，或纯文本引导，收集数据并输出 JSON）
- `exec` —— 执行操作（构建 / 导出 / 同步 / 工具）

旧扁平参数系统（`--cli <command>`、`--list-branches` 等）已**彻底废弃**，不再保留兼容别名。

不传任何参数 → 启动 GUI 模式（WizardWindow 向导）。

## 2. 命令总览

### 全局选项（所有命令可用）

| 选项 | 说明 |
|------|------|
| `--json` | 结果以 JSON 标记块输出（stdout），人类日志改走 stderr |
| `--verbose` | 详细输出（当前命令集内暂无额外分支输出，保留接口） |
| `--silent` / `--quiet` | 静默模式（仅错误） |
| `--prefill k=v` | flow 专用，可重复，预填向导页值 |

### 帮助与版本（多写法）

| 类型 | 写法 | 行为 |
|------|------|------|
| help | `-h` `--help` `-help` `help` `/h` `/?` `-?` | 全量帮助；`info --help` / `flow --help` / `exec --help` 输出分类帮助 |
| version | `-v` `--version` `/v` | 单行 `NeoServerUpdateModpack CLI v1.0.0`，exit 0 |

> 注意：version 标记优先级极高——只要参数里出现 `-v/--version//v` 且无 help 标记，无论类别/动词是什么都直接输出版本。

### info —— 信息获取（13 个命令）

| 命令 | 必配参数 | 可选参数 | 说明 | 详细 |
|------|---------|---------|------|------|
| `info version` | — | — | 软件版本 + 构建类型 | [CLI-info](CLI-info.md#1-info-version) |
| `info system` | — | — | 平台 / 目录 / 磁盘 / Git | [CLI-info](CLI-info.md#2-info-system) |
| `info git` | — | — | Git 可执行路径与版本 | [CLI-info](CLI-info.md#3-info-git) |
| `info git-branches` | `--repo` | `--git-branch` | Git 分支列表 | [CLI-info](CLI-info.md#4-info-git-branches) |
| `info modpacks` | `--repo` | `--git-branch` | workspace.json 整合包分支 | [CLI-info](CLI-info.md#5-info-modpacks) |
| `info status` | `--repo` | `--modpack` | 工作区状态（不克隆） | [CLI-info](CLI-info.md#6-info-status) |
| `info workspace` | `--repo` | `--git-branch` | 工作区元信息 + 分支继承链 | [CLI-info](CLI-info.md#7-info-workspace) |
| `info preview` | `--repo` `--modpack` | `--format` `--git-branch` | 虚拟构建文件树预览 | [CLI-info](CLI-info.md#8-info-preview) |
| `info plugins` | — | — | parser/pointer/exporter 插件清单 | [CLI-info](CLI-info.md#9-info-plugins) |
| `info exporters` | — | — | 导出格式与额外字段 | [CLI-info](CLI-info.md#10-info-exporters) |
| `info pointers` | `--repo` | `--git-branch` | 指针文件清单 | [CLI-info](CLI-info.md#11-info-pointers) |
| `info history` | — | `--type` | 最近仓库历史 | [CLI-info](CLI-info.md#12-info-history) |
| `info debug` | — | — | 汇总诊断 | [CLI-info](CLI-info.md#13-info-debug) |

### flow —— 流程控制（2 个命令）

| 命令 | 参数 | 说明 | 详细 |
|------|------|------|------|
| `flow gui` | `[--from <page> --to <page> --collect-only --prefill k=v ...]` | 复用 GUI 向导；预填页自动跳过；到达终点页收集数据输出 JSON | [CLI-flow](CLI-flow.md#1-flow-gui) |
| `flow console` | `[--from <page> --to <page> --prefill k=v ...]` | 纯文本引导；逐页提问收集数据输出 JSON | [CLI-flow](CLI-flow.md#2-flow-console) |

页面：`repo|branch|modpack|export-type|export-dir|extra-info|checklist|build|done`

### exec —— 执行操作（9 个命令）

| 命令 | 必配参数 | 可选参数 | 说明 | 详细 |
|------|---------|---------|------|------|
| `exec build` | `--repo` `--modpack` | `--git-branch` `--format` `--export` | 构建整合包（可选导出） | [CLI-exec](CLI-exec.md#1-exec-build) |
| `exec export` | `--export` `--format` | `--repo` `--modpack` `--git-branch` | 导出已构建产物（含双模式） | [CLI-exec](CLI-exec.md#2-exec-export) |
| `exec sync-serverconfig` | `--save` | `--repo` `--git-branch` | 从存档同步服务器配置 | [CLI-exec](CLI-exec.md#3-exec-sync-serverconfig) |
| `exec verify-repo` | `--repo` | `--git-branch` `--json` | 仓库完整性校验 | [CLI-exec](CLI-exec.md#4-exec-verify-repo) |
| `exec resolve-pointer` | `<file.pointer>` | `--json` | 解析指针文件为下载 URL | [CLI-exec](CLI-exec.md#5-exec-resolve-pointer) |
| `exec crash-test` | — | `--json` | 故意崩溃（校验崩溃处理） | [CLI-exec](CLI-exec.md#6-exec-crash-test) |
| `exec git-update` | — | `--json` | 安装/更新内置 Git 并写配置 | [CLI-exec](CLI-exec.md#7-exec-git-update) |
| `exec repo-trust` | `--repo` | `--json` | 信任仓库（写入 safe.directory） | [CLI-exec](CLI-exec.md#8-exec-repo-trust) |
| `exec repo-trust-check` | `--repo` | `--json` | 检查仓库是否受信任 | [CLI-exec](CLI-exec.md#9-exec-repo-trust-check) |

## 3. 退出码速查

| 码 | 含义 |
|----|------|
| `0` | 成功（含 Ctrl+C 取消的构建/导出/同步） |
| `1` | 运行期错误（仓库缺失、clone 失败、workspace.json 缺失/解析失败、构建失败、flow 取消等） |
| `2` | 参数/用法错误（未知命令、缺必配项、非法页码/prefill/格式） |
| `0xC0000005` | `exec crash-test` 故意崩溃（ACCESS_VIOLATION） |

## 4. JSON 输出协议

所有 info 命令、`exec verify-repo/resolve-pointer/crash-test/git-update/repo-trust/repo-trust-check`、以及 flow 全部命令，输出统一标记块（**始终写 stdout**）：

```
=====JSON-BEGIN=====
{ "category": "<info|flow|exec>", "command": "<verb>", "data": { ... } }
=====JSON-END=====
```

- `data` 内容随命令而异（详见各分文档的完整 schema 与示例）
- 消费端按 BEGIN/END 标记提取 JSON，不受中间日志行干扰
- `--json` 时人类日志走 stderr；`exec build/export/sync-serverconfig` 不输出 JSON 块（见 CLI-exec）
- flow 无需 `--json` 也输出 JSON 块

## 5. 快速上手

```powershell
# 帮助与版本
NeoServerUpdateModpack.exe --help
NeoServerUpdateModpack.exe -v

# 信息获取
NeoServerUpdateModpack.exe info version --json
NeoServerUpdateModpack.exe info system --json
NeoServerUpdateModpack.exe info git-branches --repo https://example.com/repo.git --json
NeoServerUpdateModpack.exe info modpacks --repo https://example.com/repo.git --json

# 仓库完整性校验 + 指针解析
NeoServerUpdateModpack.exe exec verify-repo --repo https://example.com/repo.git --json
NeoServerUpdateModpack.exe exec resolve-pointer mods/abc123....jar.pointer --json

# 构建整合包（human 输出）
NeoServerUpdateModpack.exe exec build --repo https://example.com/repo.git --modpack client-HBNS --format mcbbs --export C:/out/pack.zip

# 向导流程（收集数据，输出 JSON）
NeoServerUpdateModpack.exe flow console --prefill repo=C:/repo --prefill branch=master --prefill modpack=client-HBNS --prefill format=mcbbs --prefill exportdir=C:/out --prefill name=MyPack --prefill version=1.0.0
```

## 6. 相关约定（速览）

- 所有需要仓库的命令（除 `info status`）都会把远程/本地仓库**克隆到缓存目录**并读取缓存中的 `workspace.json`——本地未提交的改动不会被看到（详见 [CLI-usage 第 5 节](CLI-usage.md#5-仓库缓存与克隆语义)）
- `info status` 不克隆，只检查缓存目录是否存在
- 中文界面 / 英文 CLI 输出；GUI 与 CLI 共用同一套构建引擎与插件目录
