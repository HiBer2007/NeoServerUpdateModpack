# info —— 信息获取命令详解（CLI-info）

> 全部 info 命令**只读**（唯一的写行为是把仓库克隆/更新到缓存目录，见 CLI-usage 第 5 节）。
> 所有命令支持 `--json`；统一 JSON 外壳：`{ "category": "info", "command": "<verb>", "data": {...} }`。
> 退出码：`0` 成功 / `1` 运行期错误 / `2` 缺必配参数或用例错误。

---

## 1. `info version`

软件版本与构建类型。无参数。

```
NeoServerUpdateModpack.exe info version [--json]
```

**JSON 示例（exit 0）：**

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

| 字段 | 类型 | 说明 |
|------|------|------|
| `version` | string | 版本号（arg_parser.cpp 中 `VERSION` 常量） |
| `build_type` | string | `debug`（_DEBUG 构建）或 `release` |

人类模式：`[*] Version: 1.0.0 (debug)`。

> 与 `-v/--version//v` 的区别：version 标记输出 `NeoServerUpdateModpack CLI v1.0.0` 单行且不走 JSON；
> 只有 `info version --json` 才有 JSON 块。

---

## 2. `info system`

平台、目录、磁盘、Git 可用性。无参数。

```
NeoServerUpdateModpack.exe info system [--json]
```

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "system",
  "data": {
    "platform": "Windows 11 Version 25H2",
    "os": "windows",
    "kernel": "10.0.26100",
    "cpu_arch": "x86_64",
    "exe_dir": "C:/.../build/deploy",
    "data_dir": "C:/Users/bob_2/AppData/Local/NeoServerUpdateModpack",
    "cache_dir": "C:/Users/bob_2/AppData/Local/NeoServerUpdateModpack/cache",
    "config_dir": "C:/Users/bob_2/AppData/Local/NeoServerUpdateModpack/config",
    "temp_dir": "...",
    "default_workspace_dir": "...",
    "workdir_free_bytes": 196137779200,
    "git_available": true,
    "git_path": "git",
    "use_system_git": true
  }
}
=====JSON-END=====
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `platform` | string | Qt `prettyProductName()`（如 "Windows 11 Version 25H2"） |
| `os` | string | Qt `productType()`（如 "windows"） |
| `kernel` | string | Qt `kernelVersion()` |
| `cpu_arch` | string | Qt `currentCpuArchitecture()`（如 "x86_64"） |
| `exe_dir` | string | exe 所在目录 |
| `data_dir` | string | 应用数据目录（`%LOCALAPPDATA%/NeoServerUpdateModpack`） |
| `cache_dir` | string | 缓存目录（仓库克隆、构建缓存根） |
| `config_dir` | string | 配置目录（含 `history/main.json`） |
| `temp_dir` | string | 临时目录 |
| `default_workspace_dir` | string | 默认工作区目录 |
| `workdir_free_bytes` | number | exe 所在磁盘可用字节数 |
| `git_available` | bool | 是否检测到 git 可执行文件 |
| `git_path` | string | 实际 git 路径（`install.conf` 的 `git_path`） |
| `use_system_git` | bool | 是否使用系统 Git（false = 内置 Git） |

---

## 3. `info git`

Git 可执行路径与版本。无参数。

```
NeoServerUpdateModpack.exe info git [--json]
```

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "git",
  "data": {
    "git_path": "git",
    "use_system_git": true,
    "git_version": "git version 2.52.0.windows.1"
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `git_path` | git 路径（"git" 表示走 PATH） |
| `use_system_git` | true=系统 Git；false=内置 Git（tools/git） |
| `git_version` | `git --version` 输出；取不到时为空串 |

人类模式：git 版本取不到时输出 `[!] Could not determine git version.`（仍 exit 0）。

---

## 4. `info git-branches`

Git 分支列表（本地 + 远端合并去重）。

```
NeoServerUpdateModpack.exe info git-branches --repo <url|path> [--git-branch <b>] [--json]
```

**必配参数：**

| 参数 | 必配 | 说明 |
|------|------|------|
| `--repo` | ✅ | 仓库 URL 或本地路径；缺失 → `No repository URL specified. Use --repo <url>.` exit 2 |
| `--git-branch` | 否 | 先 checkout 该分支再列（clone/fetch 后执行） |

**行为**：仓库克隆/更新到缓存（`ensureRepoCloned`，见 CLI-usage 第 5 节）→ `git branch`；
失败时回退 `git branch -r`（远端分支，去除 `origin/`/`remotes/` 前缀）。

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "git-branches",
  "data": {
    "count": 1,
    "branches": [
      "master"
    ]
  }
}
=====JSON-END=====
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `count` | number | 分支数 |
| `branches` | string[] | 去重排序后的分支名（含 `*` 当前标记剥离） |

**错误：**

| 场景 | 输出 | exit |
|------|------|------|
| 缺 `--repo` | `[-] No repository URL specified...` | 2 |
| clone 失败 | `[-] Failed to clone repository: <stderr>` | 1 |
| 本地/远端列分支均失败 | `[-] Failed to list branches: <stderr>` | 1 |

---

## 5. `info modpacks`

列出 workspace.json 中定义的整合包分支（含隐藏标记、父分支、版本、加载器）。

```
NeoServerUpdateModpack.exe info modpacks --repo <url|path> [--git-branch <b>] [--json]
```

**必配参数：** `--repo`（缺失 exit 2）。

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "modpacks",
  "data": {
    "count": 3,
    "branches": [
      {
        "name": "client-HBNS",
        "parent": "client-base",
        "game_version": "1.21.1",
        "modloader": "neoforge",
        "modloader_version": "21.1.233",
        "hidden": false,
        "description": "..."
      }
    ]
  }
}
=====JSON-END=====
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `count` | number | 分支总数（含 hidden） |
| `branches[]` | array | 每个分支的配置 |
| `branches[].name` | string | 分支名（整合包名） |
| `branches[].parent` | string | 父分支名；根分支为 `""` |
| `branches[].game_version` | string | 游戏版本 |
| `branches[].modloader` | string | 加载器（forge/neoforge/fabric 等） |
| `branches[].modloader_version` | string | 加载器版本 |
| `branches[].hidden` | bool | 是否隐藏（GUI 不显示；CLI 仍列出） |
| `branches[].description` | string | 分支描述 |

人类模式：表格（Branch / Parent / Game Version / Modloader / Hidden / Description）。

**错误：** 缺 `--repo` exit 2；workspace.json 缺失 `[-] workspace.json not found in repository root.` exit 1；
解析失败 `[-] Failed to parse workspace.json.` exit 1（数据问题常见根因见 CLI-errors）。

---

## 6. `info status`

工作区状态。**不克隆**——只检查缓存目录是否存在。

```
NeoServerUpdateModpack.exe info status --repo <url> [--modpack <b>] [--json]
```

**必配参数：** `--repo`（缺失 exit 2）。`--modpack` 可选（提供时附加该分支的详情）。

### 未克隆场景（缓存目录不是 git 仓库）

**exit 0**（注意：不是错误，是有效应答）：

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "status",
  "data": {
    "cloned": false,
    "workdir": "C:/Users/bob_2/AppData/Local/NeoServerUpdateModpack/cache/repos/https___example.com_nope.git",
    "repo": "https://example.com/nope.git"
  }
}
=====JSON-END=====
```

人类模式：`[*] Repository not cloned locally.` + 提示先 `exec build`。

### 已克隆场景

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "status",
  "data": {
    "cloned": true,
    "workdir": "C:/.../cache/repos/<slug>",
    "workspace": "测试整合包",
    "minecraft_version": "1.21.1",
    "modloader": "neoforge",
    "git_remote": "",
    "default_branch": "main",
    "git_status": "## master...\n",
    "head": "6adc093d6295d5760f510129b2ada7485e2f4c46",
    "tracked_files": 42,
    "modpack": {
      "branch": "client-HBNS",
      "inheritance_chain": ["client-HBNS", "client-base", "root"],
      "game_version": "1.21.1",
      "modloader": "neoforge",
      "modloader_version": "21.1.233",
      "built_output": "C:/.../cache/repos/<slug>/.minecraft/versions/client-HBNS",
      "output_files": 128
    }
  }
}
=====JSON-END=====
```

| 字段 | 出现条件 | 说明 |
|------|---------|------|
| `cloned` | 恒有 | 缓存是否已是 git 仓库 |
| `workdir` / `repo` | 恒有 | 缓存路径 / 原始 repo 参数 |
| `workspace` / `minecraft_version` / `modloader` / `git_remote` / `default_branch` | 已克隆 | workspace.json 顶层元信息 |
| `git_status` | `git status` 成功 | 原始输出（可能多行） |
| `head` | `git rev-parse HEAD` 成功 | HEAD 提交哈希（首行） |
| `tracked_files` | `git ls-files` 成功 | 跟踪文件数 |
| `modpack` | 给了 `--modpack` | 该分支详情；`inheritance_chain` 从子到根；`built_output`/`output_files` 仅在 `.minecraft/versions/<branch>` 存在时出现 |

**错误：** 缺 `--repo` exit 2；已克隆但 workspace.json 缺失 exit 1；解析失败 exit 1。

---

## 7. `info workspace`

workspace.json 元信息 + 全部分支的继承链。

```
NeoServerUpdateModpack.exe info workspace --repo <url|path> [--git-branch <b>] [--json]
```

**必配参数：** `--repo`（缺失 exit 2）。

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "workspace",
  "data": {
    "workdir": "C:/.../cache/repos/<slug>",
    "name": "测试整合包",
    "minecraft_version": "1.21.1",
    "modloader": "neoforge",
    "git_remote": "",
    "default_branch": "main",
    "branches": [
      {
        "name": "client-HBNS",
        "parent": "client-base",
        "game_version": "1.21.1",
        "modloader": "neoforge",
        "modloader_version": "21.1.233",
        "hidden": false,
        "description": "...",
        "inheritance_chain": ["client-HBNS", "client-base", "root"]
      }
    ]
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `branches[].inheritance_chain` | string[]，从该分支到根分支的完整链（子→父→…→根） |

人类模式：每分支一行 `name [parent=(root)|<parent>] chain=a -> b -> c`。

**错误：** 缺 `--repo` exit 2；clone 失败 exit 1；workspace.json 缺失/解析失败 exit 1。

---

## 8. `info preview`

虚拟构建文件树预览（不产出最终产物）。

```
NeoServerUpdateModpack.exe info preview --repo <url|path> --modpack <branch> [--format <mcbbs|modrinth|hmcl>] [--git-branch <b>] [--json]
```

**必配参数：**

| 参数 | 必配 | 说明 |
|------|------|------|
| `--repo` | ✅ | 缺失 exit 2 |
| `--modpack` | ✅ | 缺失 → `No modpack branch specified. Use --modpack <branch>.` exit 2 |

**可选参数：**

| 参数 | 默认 | 说明 |
|------|------|------|
| `--format` | `mcbbs` | 仅接受 `mcbbs\|modrinth\|hmcl`，否则 `Unsupported format: <f>. Use mcbbs, modrinth, or hmcl.` exit 2 |
| `--git-branch` | — | clone/fetch 后 checkout 的分支 |

**行为**：克隆/更新缓存 → 用 BuildEngine 在 `<temp>/nsum_preview` 做**虚拟构建** → 按格式生成
预览结构树（`ModpackExporter::previewStructure`）。

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "preview",
  "data": {
    "format": "mcbbs",
    "branch": "client-HBNS",
    "entries": [
      {
        "path": "manifest.json",
        "dir": false,
        "umd": ""
      },
      {
        "path": "overrides/mods/example.jar",
        "dir": false,
        "umd": ""
      }
    ]
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `entries` | 预览结构（由导出插件 `preview_structure` 返回）：每项含 `path`（相对路径）、`dir`（是否目录）、`umd`（扩展字段位）；各格式根不同：mcbbs 含 manifest.json/mcbbs.packmeta/overrides，modrinth 含 modrinth.index.json/overrides，hmcl 含 version.json |

**错误：**

| 场景 | exit |
|------|------|
| 缺 `--repo` / `--modpack` / 非法 `--format` | 2 |
| clone 失败 / workspace.json 缺失 | 1 |
| `[-] Failed to initialize build engine.`（缓存/目录异常） | 1 |
| `[-] Preview build failed: <err>`（如分支依赖缺失、规则错误） | 1 |

---

## 9. `info plugins`

扫描 exe 同级的 `parsers/`、`pointers/`、`exporters/` 目录下所有 `.meta.json` 插件元信息。

```
NeoServerUpdateModpack.exe info plugins [--json]
```

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "plugins",
  "data": {
    "parser_count": 5,
    "pointer_count": 2,
    "exporter_count": 3,
    "parsers": [
      {
        "name": "...",
        "version": "1.0.0",
        "dll": "NeoParser_JSON.dll",
        "file": "NeoParser_JSON.meta.json",
        ...
      }
    ],
    "pointers": [ ... ],
    "exporters": [ ... ]
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `parsers[]` / `pointers[]` / `exporters[]` | 各目录全部 `*.json` 元信息对象，追加 `file`（文件名）；解析失败的文件被跳过 |
| `*_count` | 各目录有效 meta 数量 |

> 目录不存在时对应数组为空、count=0（不报错）。

---

## 10. `info exporters`

导出格式与扩展字段（供自动化判断 `--format` 与 `--prefill` 额外字段键）。

```
NeoServerUpdateModpack.exe info exporters [--json]
```

**JSON 示例（exit 0，节选）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "exporters",
  "data": {
    "exporters": [
      {
        "name": "MCBBS Modpack Exporter",
        "version": "1.0.0",
        "dll": "NeoExporter_MCBBS.dll",
        "format": "mcbbs",
        "extension": ".zip",
        "description": "...",
        "fields": [
          { "key": "name", "label": "整合包名称", "required": true, "group": "基本信息", "placeholder": "..." },
          { "key": "version", "label": "版本号", "required": true, "group": "基本信息", "placeholder": "..." },
          { "key": "author", "label": "作者", "required": false, "group": "基本信息", "placeholder": "..." },
          { "key": "description", "label": "整合包描述", "required": false, "group": "可选信息", "type": "multiline", "placeholder": "..." }
        ],
        "file": "NeoExporter_MCBBS.meta.json"
      },
      { "...": "modrinth（.mrpack，fields 含 name/version/summary/author/description）" },
      { "...": "hmcl（无 fields）" }
    ]
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `exporters[].format` | 格式 id（`mcbbs`/`modrinth`/`hmcl`）——`exec build --format` 与 flow `--prefill format=` 取值 |
| `exporters[].extension` | 扩展名/说明（`.zip`/`.mrpack`/hmcl 为"工作目录"） |
| `exporters[].fields[]` | 额外字段定义：`key`（`--prefill name=...` 的键）、`label`、`required`、`type`（multiline 等）、`group`、`placeholder` |
| `exporters[].file` | meta 文件名 |

> **必读**：flow 的 `--prefill` 额外字段键、`flow console` 的可选/必填提示、以及整合包导出所需元数据，
> 都以这份 `fields` 为准（见 CLI-flow 与 CLI-exec 的 `exec build` 说明）。

---

## 11. `info pointers`

列出仓库的指针文件清单（`workspace.json` 或分支配置中的 `pointer_files`）。

```
NeoServerUpdateModpack.exe info pointers --repo <url|path> [--git-branch <b>] [--json]
```

**必配参数：** `--repo`（缺失 exit 2）。

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "pointers",
  "data": {
    "count": 2,
    "pointers": [
      {
        "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        "resolver": "direct-url",
        "metadata": { "url": "https://example.com/example.jar" }
      }
    ]
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `pointers[].sha256` | 目标文件 SHA-256（指针文件名） |
| `pointers[].resolver` | 使用的指针解析器 id |
| `pointers[].metadata` | 解析器所需元数据（direct-url 为 `{url}`；Modrinth 为 `{project_id, version_id}`）；无元数据时字段省略 |

人类模式：`<sha256 前12位>... resolver=<id>` 每行一个。

**错误：** 缺 `--repo` exit 2；clone 失败/workspace.json 缺失/解析失败 exit 1。

---

## 12. `info history`

读取最近仓库历史（`<exe>/config/history/main.json`，由 GUI/CLI 共用的 `HistoryStore` 维护）。

```
NeoServerUpdateModpack.exe info history [--type local|remote|cache] [--json]
```

**可选参数：**

| 参数 | 说明 |
|------|------|
| `--type` | 过滤类型：`local` / `remote` / `cache`；省略 = 全部；非法值 = 匹配不到任何条目 |

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "history",
  "data": {
    "count": 1,
    "history_dir": "C:/.../build/deploy/config/history",
    "recent_cache_dir": "C:/.../build/deploy/config/history/cache",
    "entries": [
      {
        "type": "local",
        "location": "C:\\Users\\bob_2\\Desktop\\...\\test_repo",
        "cache_path": "C:\\...\\config\\history\\cache\\<md5前16位>"
      }
    ]
  }
}
=====JSON-END=====
```

| 字段 | 说明 |
|------|------|
| `count` | 过滤后的条目数 |
| `history_dir` | 历史文件目录 |
| `recent_cache_dir` | 远程仓库缓存目录 |
| `entries[].type` | `local` / `remote` / `cache` |
| `entries[].location` | 仓库地址/路径 |
| `entries[].cache_path` | 仅 cache 类型条目有；远程仓库缓存路径 |

人类模式：`[local] <location>` 每行一个。

---

## 13. `info debug`

汇总诊断：version + system + git + 插件计数，一次输出。

```
NeoServerUpdateModpack.exe info debug [--json]
```

**JSON 示例（exit 0）：**

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "debug",
  "data": {
    "version": "1.0.0",
    "build_type": "debug",
    "platform": "Windows 11 Version 25H2",
    "os": "windows",
    "cpu_arch": "x86_64",
    "exe_dir": "C:/.../build/deploy",
    "data_dir": "...",
    "cache_dir": "...",
    "config_dir": "...",
    "temp_dir": "...",
    "git_path": "git",
    "use_system_git": true,
    "git_version": "git version 2.52.0.windows.1",
    "parser_count": 5,
    "pointer_count": 2,
    "exporter_count": 3
  }
}
=====JSON-END=====
```

> 字段含义与 `info system` / `info git` / `info plugins` 相同；`git_version` 为空串表示无法获取。

---

## 附：info 命令必配参数总表

| 命令 | `--repo` | `--modpack` | `--format` | 其他 |
|------|:---:|:---:|:---:|------|
| version / system / git / plugins / exporters / debug | — | — | — | 无 |
| history | — | — | — | `--type`（可选） |
| git-branches | ✅ | — | — | `--git-branch`（可选） |
| modpacks | ✅ | — | — | `--git-branch`（可选） |
| status | ✅ | 可选 | — | 不克隆 |
| workspace | ✅ | — | — | `--git-branch`（可选） |
| preview | ✅ | ✅ | 默认 mcbbs | `--git-branch`（可选） |
| pointers | ✅ | — | — | `--git-branch`（可选） |

> 附注：`info pointers` 在帮助文案中写作需要 `--modpack`，但实现仅要求 `--repo`（历史文案差异，
> 以本表为准）。
