# 错误与疑难解答（CLI-errors）

> 本文档汇总 CLI 所有已知错误信息、退出码语义、参数搭配陷阱与故障排查步骤。
> 命令详细行为见 [CLI-info](CLI-info.md) / [CLI-flow](CLI-flow.md) / [CLI-exec](CLI-exec.md)。

## 1. 退出码速查（含"预期非零"）

| 码 | 含义 | 是否故障 |
|----|------|:---:|
| `0` | 成功；或 Ctrl+C 取消的 build/export/sync | 否 |
| `1` | 运行期错误（仓库/配置/构建/解析/取消等） | 通常是 |
| `2` | 用法/参数错误（未知命令、缺必配项、非法值） | 是（调用方问题） |
| `0xC0000005` | `exec crash-test` 故意崩溃 | 否（预期） |
| 预期非零特例 | `exec git-update` 系统 Git 模式 exit 1；`info status` 未克隆 exit 0（非错误）；`verify-repo` 无效仓库 exit 1（校验结果） | — |

## 2. 解析/用法错误（exit 2）——完整清单

| 输入 | 输出 | 说明/解决 |
|------|------|----------|
| 首 token 不是 `info/flow/exec` | `Unknown command: '<x>'. Use --help for usage.` | 拼写错误或想用旧扁平参数（`--cli`/`--list-branches` 等已废弃，不支持） |
| 只给类别不给动词（如 `info`） | `Unknown command: 'info'. Use --help for usage.` | 类别后必须有动词 |
| 类别内动词不存在（如 `info foo`） | `Unknown command: 'foo'. Use --help for usage.` | 用 `info --help` 查动词列表 |
| 动词后出现类别名（如 `flow console info`） | `Unknown command: 'info'. Use --help for usage.` | 选项值不能是 info/flow/exec；如需传值给其他选项用 `--key=值` 形式 |
| 缺 `--repo`（info git-branches/modpacks/status/workspace/preview/pointers、exec build/verify-repo） | `[-] No repository URL specified. Use --repo <url>.` | 补 `--repo` |
| 缺 `--modpack`（info preview、exec build） | `[-] No modpack branch specified. Use --modpack <branch>.` | 补 `--modpack` |
| 缺 `--export`（exec export） | `[-] No export path specified. Use --export <path>.` | 补 `--export` |
| 缺 `--format`（exec export） | `[-] No export format specified. Use --format <mcbbs\|modrinth\|hmcl>.` | 补 `--format` |
| 缺 `--save`（exec sync-serverconfig） | `[-] No save world specified. Use --save <world_name>.` | 补 `--save` |
| 缺位置参数（exec resolve-pointer） | `[-] No pointer file specified. Usage: exec resolve-pointer <file.pointer>` | 补文件路径 |
| `--format` 非法 | `[-] Unsupported format: <f>. Use mcbbs, modrinth, or hmcl.` | 仅 mcbbs/modrinth/hmcl |
| `flow` 的 `--from/--to` 页名非法 | `[-] Invalid --from/--to page: '<x>'. Use --help for usage.` | 页名 ∈ repo\|branch\|modpack\|export-type\|export-dir\|extra-info\|checklist\|build\|done |
| `flow console` 的 `--to build/done` | `[-] --to '<x>' is not supported by console (max is 'checklist'). Use --help for usage.` | console 无构建能力，上限 checklist |
| `flow console` 的 `--from build` | `[-] --from '<x>' is not supported (max build). Use --help for usage.` | 同上 |
| `flow` 的 `--prefill` 缺 `=` 或空键/空值 | `[-] Invalid --prefill '<kv>'. Expected key=value. Use --help for usage.` | 必须是 `键=值` 且都不为空；多字段用多个 `--prefill` |
| `flow console` 起始页 > repo 但未 prefill repo | `[-] repo is required before the branch page. Pass --prefill repo=<url\|path>.` | 非 repo 起始页必须预填 repo |
| `exec build/export` 缺缓存或格式插件 | `[-] Exporter plugin not found for format: <f>` | 检查 `exporters/` 目录 DLL 与 meta.json（见第 7 节） |

## 3. 运行期错误（exit 1）——完整清单

| 输出 | 场景 | 解决 |
|------|------|------|
| `[-] Failed to clone repository: <stderr>` | clone 失败（网络/权限/URL 错/本地路径不存在） | 检查 URL 可访问性、git 凭据；本地路径必须存在且可 clone；网络代理 |
| `[-] Repository not found: <repo>` | flow console：repo 既非 URL 也非存在的路径 | 检查路径/URL 拼写；`file://` 路径会先转本地路径 |
| `[-] workspace.json not found in repository root.` | 缓存仓库根没有 workspace.json | 确认该仓库确实是整合包工作区仓库；或缓存是旧克隆（见第 5 节强刷） |
| `[-] Failed to parse workspace.json.` | workspace.json JSON 语法错误或 schema 不兼容 | 见第 6 节（典型：`parent: null`） |
| `[-] Preview build failed: <err>` | info preview 虚拟构建失败 | 查看具体错误：分支继承链缺父分支、规则冲突、插件缺失 |
| `[-] Failed to initialize build engine.` | info preview 构建引擎初始化失败 | 检查缓存目录可写、磁盘空间 |
| `[-] Build failed: <err>` | exec build 构建失败 | 同上；查看 builder.log |
| `[-] Export failed.` / `[-] Export failed for format: <f>` | exec build/export 导出失败 | 检查目标路径可写、导出插件、磁盘空间 |
| `[-] No workspace found. Provide --repo <url> or run from a workspace directory.` | exec export 独立模式找不到 workspace | 加 `--repo`，或在含 workspace.json 的目录运行 |
| `[-] No previously-built output found. Build first with --repo --modpack.` | exec export 独立模式找不到构建产物 | 先 `exec build --repo ... --modpack ...`；或加 `--modpack` 指定 |
| `[-] Repository not cloned. Clone first.` | exec sync-serverconfig 带 `--repo` 但缓存未克隆 | 先 `exec build` 或 `info git-branches --repo <url>` 触发克隆 |
| `[-] Save world not found: <path>` | 存档世界目录不存在 | 确认 `--save` 名称与 `saves/` 下一致；检查 saves 目录解析（见 CLI-exec 第 3 节） |
| `[-] Failed to initialize server config sync.` | sync 初始化失败 | 检查仓库分支 serverconfig 规则与路径 |
| `[!] Failed to sync: <rel>` + exit 1 | sync 部分文件失败 | 查看具体文件；重试；检查目标目录权限 |
| `[-] Repository is invalid.` / exit 1 | verify-repo：非 git 或无 workspace | 用 `--json` 看 `valid` 与各字段定位 |
| `[-] Cannot open pointer file: <path>` | resolve-pointer 文件不存在/不可读 | 检查路径 |
| `[-] Invalid pointer JSON: <what>` | 指针文件不是合法 JSON | 检查文件内容（UTF-8、无 BOM 乱码） |
| `[-] Pointer file missing 'sha256' field.` | 指针缺 sha256 | 检查指针文件 |
| `[-] Pointer file has no resolvers.` | 指针缺 resolvers 数组 | 检查指针文件 |
| `[-] Failed to resolve pointer: <err>` | 无匹配 resolver / resolver 异常 / 空 URL | 检查 `pointers/` 插件是否齐全；metadata（url/project_id/version_id）是否正确 |
| `[-] Failed to get Git URL` / `[-] Download failed` | exec git-update 网络失败 | 检查网络；GitHub API 可达性；重试 |
| `[-] Git install failed: no runnable git.exe found under <dir>` | 解压后找不到可运行 git.exe | 重新 `exec git-update`；手动清理 `<root>/tools/git` 后重试 |
| `Flow cancelled.`（stderr）+ exit 1 | flow gui 中途关闭 / flow console 取消 | 预期行为；重跑并补 `--prefill` 可 headless 完成 |
| `Install config invalid: ...` + exit 1 | `install.conf` 缺失/损坏（install_path/git_path 等） | 运行 `exec git-update` 重新生成，或重装程序 |
| `[WARN] ... Directory not found: <exe>/parsers|pointers|exporters` | 插件目录缺失（启动时警告） | 从构建产物复制三个插件目录到 exe 同级（见第 7 节） |

## 4. 参数搭配陷阱（"必须搭配使用"）

| 命令 | 必须成对/搭配 | 说明 |
|------|--------------|------|
| `info preview` | `--repo` **且** `--modpack` | 缺任一都 exit 2；`--format` 可选默认 mcbbs |
| `exec build` | `--repo` **且** `--modpack` | 缺任一 exit 2；`--export` 仅 mcbbs/modrinth 时打包 |
| `exec export` | `--export` **且** `--format` | 缺任一 exit 2 |
| `exec export` 双模式 | `--repo` 与 `--modpack` **要同时给**（或同时不给） | 只给一个会走"独立导出"，`--modpack` 单独给时用作产物定位 |
| `exec sync-serverconfig` | `--repo` 与 `--git-branch` 可搭配 | 不给 `--repo` 时按当前目录解析 saves；给了 `--repo` 则必须已克隆 |
| `info status` | `--repo` 必给，`--modpack` 可选 | 不给 `--modpack` 就没有分支详情块 |
| flow 非 repo 起始页 | `--prefill repo=...` 必给 | 否则 exit 2 |
| flow 必填额外字段 | `name`、`version`（mcbbs/modrinth） | 不 prefill 时 gui 停在 extra-info 页交互；console 必填字段 EOF → 取消 exit 1 |
| `--git-branch` | 只对克隆类命令有效 | clone/fetch 后 checkout；`info status` 无此参数 |

## 5. 缓存与数据一致性排查

| 症状 | 原因 | 解决 |
|------|------|------|
| 本地改了 workspace.json，CLI 仍显示旧数据 | CLI 读的是 `cache/repos/<slug>` 克隆副本（fetch 只更新 refs，不更新工作树） | 提交改动并推送；或删除 `%LOCALAPPDATA%/NeoServerUpdateModpack/cache/repos/<slug>` 强制重克隆 |
| `workspace.json` 解析失败，但本地看着正常 | 缓存副本仍是旧提交 | 同上强刷；`info workspace --json` 可确认读取路径 |
| 换了一台机器/换了用户 | 缓存目录随 LOCALAPPDATA 变化 | 重新 `exec build` 或 `info git-branches` 触发克隆 |

> 缓存目录：`info system --json` → `data.cache_dir`；
> 仓库缓存子目录：`cache/repos/<slug>`（slug 规则见 CLI-usage 第 5 节）。

## 6. workspace.json 数据问题（解析失败的常见根因）

`WorkspaceManager::parseBranches` 对分支字段做类型校验，以下写法会导致**整个分支列表解析失败**
（表现为：`info modpacks` count=0、`flow console` 提示 "No modpack branches defined"、
日志 `Error parsing branches: [json.exception.type_error.302] type must be string, but is null`）：

| 字段 | 合法值 | 非法值 |
|------|--------|--------|
| `parent` | 字符串（根分支为 `""` 或缺省） | **`null`**（最常见！） |
| `name` / `description` / `modloader_version` | 字符串 | `null` |

修复：把 `parent: null` 改为 `parent: ""`（或删除该字段），并提交推送，再强刷缓存。

## 7. 插件与部署排查

| 症状 | 原因 | 解决 |
|------|------|------|
| `CreateExporter not found` / 加载插件失败 | 插件 DLL 的 `extern "C"` create 函数缺 `__declspec(dllexport)`（历史全局坑，已修） | 确认当前构建的 DLL 有导出：`dumpbin /exports <dll>` 应见 `CreateParser/CreatePointer/CreateExporter`；重构建复制到 exe 同级目录 |
| `info plugins` count=0 | 目录不存在或 meta.json 损坏 | 复制 `parsers/` `pointers/` `exporters/`（各含 .dll + .meta.json）到 exe 同级；meta 需可被 JSON 解析 |
| 指针解析失败 | `pointers/` 缺 direct-url/modrinth 插件 | 补齐插件；`info plugins --json` 检查 pointer_count=2 |
| 导出失败 | `exporters/` 缺对应格式插件 | `info exporters --json` 确认 format 存在；DLL 可加载（依赖 Qt 库齐全） |

## 8. Git / 网络排查

| 症状 | 处理 |
|------|------|
| clone 超时（300s） | 检查网络/代理；确认 URL 公开或凭据可用（`git credential`）；超大仓库可先 `git clone --depth 1` 自测 |
| `info git` 版本为空 | git 未装或 PATH 没配；运行 `exec git-update` 安装内置 Git（`install.conf` 会被写出） |
| `exec git-update` 走系统模式（exit 1） | 这是**预期行为**：`use_system_git=true` 时不做安装。需要内置 Git → 修改 `install.conf` 的 `use_system_git=false` 再运行 |
| 内置 Git 报路径不对 | MinGit 布局为 `tools/git/mingw64/bin/git.exe`（无顶层 bin/）；`C:\Program Files\Git\cmd\git.exe` 是 shim 不能独立拷贝 |

## 9. JSON 消费注意事项

- 用 BEGIN/END 标记提取，不要假设 stdout 只有 JSON（spdlog `[builder]` 行也走 stdout，标记协议容忍）
- `--json` 模式：stdout = JSON 块；stderr = 人类日志。重定向时分开 `2>err.txt 1>out.txt`
- 字段缺失≠解析失败：可选字段（`extra`、`cache_path`、`metadata`、`git_status`、`modpack` 等）按文档说明的条件性出现
- 数值字段（`count`、`workdir_free_bytes`）是 JSON number；`build_type` 是 debug/release 字符串

## 10. flow 专项排查

| 症状 | 原因/解决 |
|------|----------|
| `flow gui` 全预填却闪现窗口 | 说明仍有页面未被 prefill 满足；补齐到终点页所需全部键（含格式必填额外字段） |
| `flow gui` 停在某页不前进 | 该页无 prefill 值，等待人工输入；或仓库分支/整合包加载失败（看终端 `[builder]` 日志） |
| `flow gui` 输出 `"modpack": ""` | 终点页 ≤ modpack 且 prefill 缺失/时序问题（旧 bug 已修）；确保 `--prefill modpack=<b>` |
| `flow console` 管道输入时卡住 | 必填字段在等输入；EOF 后必填字段=取消 exit 1。给足输入行或 prefill |
| `flow console` 输出乱码 | 终端代码页；JSON 本身 UTF-8，用 `ConvertFrom-Json`/`jq` 解析不受影响 |
| 整合包列表为空 | workspace branches 解析失败（第 6 节）或全为 hidden 分支 |

## 11. 崩溃与日志定位

| 症状 | 定位 |
|------|------|
| 进程崩溃（非 crash-test） | 崩溃报告 `<exe>/crash-report/<date_time>/`（.dmp/.trace/.meta）；`builder.log` 最后日志 |
| `crash-test` 无报告生成 | crash handler 未装/目录被清；检查 `crash-report/` 可写 |
| 堆损坏（HEAP CORRUPTION） | 冒烟打开/关闭 GUI 复现；`main.cpp` 三阶段堆检查点输出到日志；回传报告 |
| 需要上报 | 附上完整 `crash-report/<date_time>/` 内容 + 复现命令 |
