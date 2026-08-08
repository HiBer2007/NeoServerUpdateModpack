# CLI 基础与参数解析（CLI-usage）

> 本文档讲解 CLI 的命令行语法、参数解析规则、帮助/版本优先级、退出码、JSON 协议、
> stdout/stderr 路由、仓库缓存与克隆语义、Ctrl+C 取消，以及运行环境要求。
> 命令逐个详解见 [CLI-info](CLI-info.md) / [CLI-flow](CLI-flow.md) / [CLI-exec](CLI-exec.md)。

## 1. 命令行语法

```
NeoServerUpdateModpack.exe <category> <verb> [options] [positional...]
```

- `<category>` ∈ `info | flow | exec`（互斥，必须与 `<verb>` 成对出现）
- `<verb>` ∈ 各类别下的动词（见总览表）
- `[options]`：`--key=value` 或 `--key value`；布尔标志直接 `--key`
- `[positional]`：位置参数（目前仅 `exec resolve-pointer <file.pointer>` 使用）
- 选项可用 `/` 前缀代替 `--`（`/json` ≡ `--json`），内部会把 `/` 归一为 `-`

### 选项取值规则（重要）

| 写法 | 结果 |
|------|------|
| `--repo=https://x` | key=`repo`，value=`https://x` |
| `--repo https://x` | key=`repo`，value=`https://x`（消费下一个 token） |
| `--json`（无值） | 布尔标志 true |
| `--repo`（后无 token 或后跟 `-` 开头） | value 为空串 |

`--key value` 形式中，如果下一个 token 是选项（`-` 开头）或 help/version 标记，则不消费，
value 记为空串——因此**值中含空格需整体加引号**（如 `--modpack "my pack"`）。

### 特殊标志

| 标志 | 说明 |
|------|------|
| `--json` | 结构化输出（见第 4 节协议） |
| `--verbose` | 详细日志（保留位，当前无额外分支） |
| `--silent` / `--quiet` | 静默模式，仅输出错误 |
| `--prefill k=v` | 仅 flow 使用，**可重复**；每次一个 `k=v` 对 |

### 位置参数 vs 类别名冲突

解析器会把 `info/flow/exec` 这些词当作"类别"；在动词之后再次出现类别名会被判为语法错误（exit 2），
防止递归调用：

```
NeoServerUpdateModpack.exe flow console info   ← 错误：'info' 不是合法参数，exit 2
```

## 2. 解析流程与错误

解析按以下顺序判定（`arg_parser.cpp`）：

1. **无参数** → 不进入 CLI，启动 GUI
2. 扫描全部 token：
   - 出现 help 标记（`-h/--help/-help/help//h//?/-?`）且无 version 标记 → Help 分支
     - 首 token 是类别 → 该类别帮助；否则全量帮助
   - 出现 version 标记（`-v/--version//v`）且无 help 标记 → **Version 分支**（优先级最高，
     即使同时给了 `info system` 也输出版本）
3. 首 token 不是 `info/flow/exec` → `Unknown command: '<x>'. Use --help for usage.` **exit 2**
4. 只有类别没有动词 → 同上报类别名 **exit 2**
5. 动词不存在于该类别 → 同上报动词名 **exit 2**
6. 其余 token 按选项/位置参数解析

所有解析错误统一格式（写 stderr）：

```
Unknown command: '<bad>'. Use --help for usage.
```

## 3. 退出码

| 码 | 含义 | 出现场景 |
|----|------|---------|
| `0` | 成功 | 所有正常完成；Ctrl+C 取消的 build/export/sync 也返回 0（带 "cancelled" 警告） |
| `1` | 运行期错误 | clone 失败、仓库/workspace.json 缺失或解析失败、构建/导出/同步失败、`verify-repo` 无效、指针解析失败、`git-update` 无内置 Git 可装、flow 中途取消 |
| `2` | 参数/用法错误 | 未知命令/动词、类别后无动词、缺必配选项（`--repo` 等）、非法 `--format`/`--from`/`--to`/`--prefill` |
| `0xC0000005` | 崩溃 | 仅 `exec crash-test` 故意触发（同时产出崩溃报告） |

## 4. JSON 输出协议

### 标记块格式

所有支持 JSON 的命令输出（**stdout**）：

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "version",
  "data": {
    "version": "1.0.0",
    "build_type": "debug"
  }
}
=====JSON-END=====
```

- 缩进 2 空格；`category`/`command`/`data` 三字段为统一外壳，`data` 随命令而异
- 消费端应扫描 `=====JSON-BEGIN=====` / `=====JSON-END=====` 之间的内容解析，
  不要依赖"整个 stdout 就是 JSON"
- **标记块即使没有 `--json` 也会输出**（flow 系列）；`--json` 只是把人类日志改道 stderr

### 支持 JSON 的命令清单

| 类别 | 支持 `--json` 的命令 |
|------|---------------------|
| info | 全部 13 个 |
| flow | 全部（gui/console 恒输出 JSON，无需 `--json`） |
| exec | `verify-repo`、`resolve-pointer`、`crash-test`、`git-update` |
| exec 不支持 | `build`、`export`、`sync-serverconfig`（`--json` 传入被忽略，人类输出） |

## 5. 仓库缓存与克隆语义

### 缓存位置

需要仓库的命令（`info git-branches/modpacks/workspace/preview/pointers`、
`exec build/export/verify-repo/sync-serverconfig`）使用统一逻辑：

```
<cache_dir>/repos/<repo_slug>
```

- `cache_dir` 默认 `%LOCALAPPDATA%/NeoServerUpdateModpack/cache`
  （可用 `info system --json` 的 `cache_dir` 字段确认）
- `repo_slug`：把 URL/路径中的非字母数字字符替换为 `_`，最长 64 字符
  - `https://example.com/repo.git` → `https___example.com_repo.git`
  - `C:\Users\bob_2\Desktop\test_repo` → `C__Users_bob_2_Desktop_NeoServerUpdateModpack_test_repo`

### 克隆/更新行为（`ensureRepoCloned`）

1. 缓存目录不存在 → `git clone <url> <cache>`（超时 300s）
2. 已存在 → `git fetch`（不 pull、不 reset）
3. 若给了 `--git-branch` → `git checkout <branch>`

### ⚠️ 三个高频误区

1. **读的是缓存副本，不是你的工作目录**：命令从 `cache/repos/<slug>/workspace.json`
   读取配置。你在源目录**未提交**的修改不会被看到——必须 `git commit`（并让 fetch 拉到）
   才能生效。
2. **fetch 不更新工作树文件**：已存在缓存后只 fetch，远端变了但缓存工作树文件不自动变；
   若想强刷，删除 `cache/repos/<slug>` 目录重新克隆。
3. **本地路径仓库也会被克隆进缓存**：`--repo C:\...\test_repo` 会被 clone 到缓存，
   本地未提交改动同样不可见。（`flow console` 对本地带 `.git` 的路径直接使用原目录，
   是唯一例外。）

## 6. Ctrl+C 取消

- CLI 安装 SIGINT/控制台 Ctrl+C 处理器 → 请求 `CancelToken` 取消
- `exec build` / `exec export` / `exec sync-serverconfig` 取消时输出
  `warning("... cancelled by user.")` 并返回 **exit 0**
- 打断正在进行的 clone/fetch/checkout 同样生效

## 7. 输出路由（stdout / stderr / 颜色）

| 内容 | 默认（无 `--json`） | `--json` 模式 |
|------|---------------------|----------------|
| JSON 标记块 | stdout | stdout（不变） |
| `[*]` info / `[+]` success / 进度条 / 表格 / 标题 | stdout | **stderr** |
| `[!]` warning / `[-]` error | stderr | stderr |
| `[INIT]` / 横幅 / `[EXEC]` 行（main 输出） | stdout | stderr |
| spdlog 日志（`[builder]`） | stdout（标记协议容忍） | stdout（标记协议容忍） |

- 颜色自动启用：仅当 stdout 是终端（TTY）时（重定向到文件/管道时无 ANSI 色码）
- `flow console` 的交互提示：默认 stdout，`--json` 时改 **stderr**（保持 stdout 纯 JSON）

## 8. 运行环境要求

| 项 | 说明 |
|----|------|
| Git | 系统 Git（PATH）或内置 Git（`exec git-update` 安装到 `<root>/tools/git`，写 `install.conf`）；`InstallConfig::load` 失败 → stderr 报错 exit 1 |
| 插件目录 | 与 exe 同级的 `parsers/`（5）、`pointers/`（2）、`exporters/`（3）目录；缺失时启动打印 `[WARN]`（不阻止运行，但相关功能不可用） |
| Qt 运行库 | 部署目录含 Qt6*.dll（windeployqt 部署） |
| 网络 | 远程仓库克隆、指针下载（Modrinth API 等）、`git-update` 下载 MinGit 需要网络 |
| 崩溃处理 | 进程内安装 crash handler，崩溃报告写 `<exe>/crash-report/<date_time>/` |

### 日志文件

- CLI/构建器日志：`builder.log`（exe 同目录，spdlog）
- GUI 日志：`gui.log`
- 崩溃报告：`crash-report/<date_time>/crash_<ts>.dmp/.trace/.meta`

## 9. 自动化测试钩子

| 环境变量 | 作用 |
|----------|------|
| `NSUM_FLOW_AUTOFINISH=1` | `flow gui` 启动 1.5s 后自动触发"下一步"，用于无人值守冒烟测试 |

## 10. 文档阅读器 PowerHelper

`PowerHelper.exe` 是独立的 Markdown 文档阅读器（随部署目录分发，另含 CLI 模式），
同时作为主程序与编辑器的帮助文档驱动器（详见 `docs/deploy/PowerHelper/`）：

```
PowerHelper <file.md>    GUI 单文档模式
PowerHelper <dir>        GUI 文档组模式（TOC + 跳转）
PowerHelper render <file.md>   CLI 终端渲染（ANSI 颜色/表格）
PowerHelper toc <file.md>      CLI 标题目录
PowerHelper group <dir>        CLI 文档组列表 / TOC 映射
```

- CLI 独立 JSON 协议：`{category:powerhelper, command, data}` + `=====JSON-BEGIN=====`/`=====JSON-END=====` 标记
- 退出码：0 成功 / 1 运行期错误 / 2 用法错误
- help 族（7 写法）/ version 族（3 写法）与主程序一致
