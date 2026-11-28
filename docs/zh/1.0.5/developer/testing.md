---
title: 测试与验证
description: NSUM 的测试分层与可复制命令：结构自检、端到端 fixture、bridge 自检、真实启动、打包验收与文档校验。
lang: zh
status: complete
updated: 2026-11-27
tags: [testing, e2e, bridge, release]
---

# 测试与验证

本页说明改完代码后按什么顺序验证、每层的通过标准，以及外部测试工作区（端到端 fixture、bridge testbed）如何准备。所有命令都可直接复制，只需替换尖括号占位符。

## 测试分层

| 层级 | 目标 | 入口 | 通过标准 |
|---|---|---|---|
| 构建 | 代码可编译、部署产物齐全 | `cmake --build build --target neo_deploy` | 退出码 0，`build/deploy` 含 EXE、插件 DLL、`docs/` |
| 结构自检 | 版本/插件/仓库结构正确 | `info version`、`info plugins`、`info exporters`、`exec verify-repo` | 字段与 `version.json` 一致；无 `DLL missing` |
| 冒烟 | 类成员/布局变更后无关闭崩溃、无堆损坏 | 启动 GUI → 等待 3–4 秒 → 关闭 | 退出码 0，`crash-report` 不新增，日志无 `HEAP CORRUPTION` |
| 端到端 fixture | 导出/指针/L1/L2/L3/杂项全链 | `run_e2e_cli.ps1` | `E2E-RESULT passed=20 failed=0` |
| bridge 自检 | HMCL 侧导入、同步、启动注入、失败回滚 | `-Dnsum.selftest=true` 等系统属性 | 日志 `[NSUM-TEST]` / `[NSUM-E2E]` 全 PASS |
| 真实启动 | 同步后游戏能进主菜单 | `launch_real.ps1` + 干净仓库 fixture | 同步 `exit=0`，日志出现 `Setting user` / `Sound engine started` |
| 打包验收 | 发布 zip 干净、可安装、版本正确 | 7-Zip 列表 + `hci_cli` 静默安装 | 无 `stores/`、`crash-report/`、`config/`、`*.log`；安装 exit 0 |
| 文档校验 | 双语页对齐、无死链死图 | `tools/docs_check.ps1` | `docs_check OK`（0 error） |

> **注意**：功能正确性验收由维护者本人完成；自动化测试只证明「没坏到可检测」，不替代人工验收。

## 环境前置

- 主工程已按 [贡献指南](./contributing.md) 的构建命令产出 `build/deploy`（测试脚本默认从这个目录取 EXE）。
- 端到端 fixture 与工具脚本放在**仓库之外**的测试工作区（通常命名为 `nsum_e2e`）：`<E2E 根>` 下分 `repo`（整合包仓库）、`target`（预置目标模板）、`work`（同步目标）、`out`（导出包与日志）；脚本顶部有 `$root` 常量指向该根目录。
- bridge 自检需要 FLFHMCL 工作区与其 `testbed`，并把 `nsum-bridge` 的 jar 放进 `<FLFHMCL 工作区>/testbed/mods/`。
- 中文路径是重点回归项：路径含中文/全角括号时同样要跑一遍，不能只用 ASCII 目录。

## 结构自检（等价单测）

本仓库**没有**独立 C++ 单元测试工程（唯一带 `enable_testing()` 的是 `nbtcpp` 子模块，与产品代码无关）。当前可自动化的「单测」是 CLI 的结构与契约自检，覆盖版本注入、插件加载、仓库合法性、工作区校验：

```powershell
$exe = '<仓库根>\build\deploy\NeoServerUpdateModpack.exe'
& $exe -v
& $exe info version --json
& $exe info plugins --json
& $exe info exporters --json
& $exe info pointers --json
& $exe exec verify-repo --repo '<含 workspace.json 的仓库>' --json
```

- `info version --json` 的 `data.version` / `internal_version` / `full_version` / `git_commit` 必须与 `version.json` 及当前提交一致（见 [版本管理](./versioning.md)）。
- `info plugins --json` 中每个解析器都要有 DLL 与 meta，出现 `DLL missing` 说明插件未部署或缺少 `__declspec(dllexport)`。
- `exec verify-repo` 断言 `is_git_repository=true`、`has_workspace=true`，退出码 0。

编辑器侧的工作区校验（重名/父缺失/父链环为 error，缺目录/缺 `branch_config` 只算 warning）：

```powershell
$editor = '<仓库根>\build\deploy\NeoWorkspaceEditor.exe'
& $editor info workspace --repo '<仓库>'
& $editor info branches --repo '<仓库>'
& $editor exec validate --repo '<仓库>'
```

## 冒烟测试

改动被多 target 共享的头文件类成员、Qt 控件布局或插件 ABI 后**必做**。目的只是检测类布局失配导致的初始化堆损坏与关闭崩溃：

```powershell
$exe = '<仓库根>\build\deploy\NeoServerUpdateModpack.exe'
$before = (Get-ChildItem '<仓库根>\build\deploy\crash-report' -Recurse -File -ErrorAction SilentlyContinue).Count
$p = Start-Process $exe -PassThru
Start-Sleep -Seconds 4
$p.CloseMainWindow() | Out-Null
$p.WaitForExit(15000) | Out-Null
"exit=$($p.ExitCode)"
"crash before=$before after=$((Get-ChildItem '<仓库根>\build\deploy\crash-report' -Recurse -File -ErrorAction SilentlyContinue).Count)"
```

窗口没建好就关闭会漏检：必须先确认进程存活 3–4 秒再发关闭。出现 `HEAP CORRUPTION`、0xC0000374 或 `crash-report` 新增，说明有 TU 按旧类尺寸分配对象，按 [贡献指南](./contributing.md) 的方式全量重建。

## 端到端 fixture

fixture 由两个脚本维护（都在仓库外的测试工具目录）：

| 脚本 | 作用 |
|---|---|
| `make_fixture.ps1` | 只生成 fixture（`repo` 仓库 + `target` 模板 + 空 `work`/`out`），用于手工排查 |
| `run_e2e_cli.ps1` | 自包含：重建 fixture → CLI 导出 `.nsumpack` → CLI 同步到 `work` → 断言 20 项 |

```powershell
pwsh -NoProfile -File '<E2E 工具目录>\make_fixture.ps1'
pwsh -NoProfile -File '<E2E 工具目录>\run_e2e_cli.ps1'
```

用例分类（编号即脚本里的断言名）：

| 组 | 覆盖内容 | 项数 |
|---|---|---|
| A 导出 | `nsum.index.json` 的 `schema`/`type`/`gameVersion`/`modloader`/`modloaderVersion`；`files[]` 含指针解析结果；包内无 `save/`、无 `.pointer` | 3 |
| B 指针 | 指针被解析并落盘；落盘内容 SHA-256 与远端一致 | 2 |
| C L1 文件夹策略 | `mirror` 覆盖并删多余；`incremental_add` 保留旧文件；`incremental_overwrite` 保留并新增；`skip` 不动目标；`default` 兜底删多余 | 5 |
| D L2 文件特化 | `full` 整文件覆盖；`partial` 按 `tracked_keys` 合并且保留本地键 | 2 |
| E L3 存档 serverconfig | `full`+`mirror` 覆盖；目标缺 `serverconfig` 的存档自动创建；`ignore` 保持目标；`.rule` 规则不外泄 | 4 |
| F 杂项 | `mods` 镜像清除 `.jar.disabled`；自定义模组收获到 `.NSUM/custom/mod`；`modpack.cfg` 不被覆盖 | 3 |
| G 退出码 | 同步命令退出码为 0 | 1 |

通过标准：最后一行输出

```text
E2E-RESULT passed=20 failed=0
```

> **警告**：fixture 里的 jar 是纯文本假 jar，只适合校验导出与同步规则。真实启动测试必须用真 jar 或纯 config 包，否则 NeoForge 会报 `zip END header not found`（属 fixture 特例，不是产品缺陷）。

## bridge 自检

bridge 侧自检跑在 HMCL 运行时里，由 JVM 系统属性触发。先取 testbed 的启动命令（脚本不转发自定义 `-D`，所以先 `-DryRun` 打印再追加属性）：

```powershell
cd '<FLFHMCL 工作区>'
pwsh -NoProfile -File .\scripts\run-testbed.ps1 -Channel stable -DryRun
# 把打印出的 java 命令补上下面的 -D 参数后执行
```

常用属性：

| 系统属性 | 作用 |
|---|---|
| `nsum.selftest=true` | 打开自检（实例标记读写、Runtime 安装、真实同步） |
| `nsum.runtime.source=<deploy 目录>` | 指定待安装/待同步的 Runtime 源 |
| `nsum.selftest.repo` / `.branch` / `.gitbranch` | 自检同步用的仓库、整合包分支、Git 分支 |
| `nsum.selftest.cliDist=<nsum_cli_dist>` | 追加 HIC CLI 静默安装自检 |
| `nsum.selftest.e2e.zip=<.nsumpack>` | 端到端：把包按导入流程装进临时实例再真实同步（18 项） |
| `nsum.selftest.e2e.target=<模板目录>` | 预置「玩家本地已有内容」的目标模板 |
| `nsum.selftest.e2e.base=<版本目录>` | 启动策略自检（`handles()` + 同步后交回 HMCL） |
| `nsum.selftest.e2e.failBase=<版本目录>` | 安装失败路径：必须抛错、旧目录/jar/实例标记保留（4 项） |
| `nsum.selftest.e2e.switchBase=<版本目录>` | 三元组切换；配 `switchLoader`/`switchVersion`/`switchGame`/`switchFromLoader`/`switchFromVersion`（每场景 2 项） |
| `nsum.selftest.download=true` | 走一次真实 FileDownloadTask 下载 |

通过标准：日志出现 `[NSUM-TEST] … PASS` 与 `[NSUM-E2E] … PASS`，无 `FAIL`。同步日志落在 `<HMCL 工作目录>/.hmcl/nsum/logs/sync-*.log`，失败时先看这里再看 game log。

> **注意**：用 `pwsh` 执行 testbed 脚本。PowerShell 5.1 读取无 BOM 的 UTF-8 脚本会按 ANSI 解码，直接报语法错误。

## 真实启动验证

在干净仓库（仅 config，不含假 jar）上验证「同步 → 启动 → 进主菜单」：

```powershell
pwsh -NoProfile -File '<E2E 工具目录>\launch_real.ps1' -Tag run -WaitSeconds 80
```

脚本按窗口相对坐标点击启动按钮（多显示器/缩放下必须用窗口坐标，不能用全屏坐标），随后列出 `java` 进程。人工核对：

- bridge 日志：`同步结束: exit=0` 之后才拉起游戏；
- `<实例>/logs/latest.log`：出现 `Setting user`、`LWJGL`、`Sound engine started`；
- 进程存活 ≥ 2 分钟，`crash-reports` 不新增。

不要用 `Start-Process -WindowStyle Hidden` 启动 HMCL 做这项验证：JavaFX 窗口会变成不可见，点击与截图全部失效。

## 打包验收

发布前对 zip 做三项核对（完整 runbook 见 [发布](./release.md)）：

```powershell
$sevenZip = '7z'   # 已加入 PATH，否则写 7z.exe 的绝对路径
& $sevenZip l '<产物>\NSUM_Runtime_CLI_<版本>.zip' | Select-String '(?i)(stores|crash-report)[\\/]|[\\/]config[\\/]|\.log$'
```

期望输出为空。出现 `stores/`（MCLHR 本地 store）会把包撑到数百 MB，是历史事故的直接原因。

静默安装到临时目录，验证 HIC 流程与版本注入：

```powershell
$dist = '<解压目录>\NSUM_Runtime_CLI_<版本>'
& "$dist\hci_cli.exe" --cli --json --silent --product product.json --flow install.json --path '<临时运行目录>'
& '<临时运行目录>\NeoServerUpdateModpack.exe' info version --json
```

退出码 0、`NeoServerUpdateModpack.exe` 落地、`install.conf` 存在，且 `info version` 的 `full_version` 等于 `外部.内部`。

离线/外部下载模式（受限网络验收）：

```powershell
& "$dist\hci_cli.exe" --cli --json --silent --external-download '<下载目录>' --product product.json --flow install.json --path '<临时运行目录>'
# 期望 exit 10，<下载目录>\downloads.json 列出所需文件
# 把文件放进 <下载目录>\files 后重跑同一命令，期望 exit 0
```

`.nsumpack` 清单核对：`pack_format` 为 2，`files[]` 每项有 `path/size/sha256/commit/git_branch`，未声明 `--embed-patterns` 时包内只有 `nsum.index.json`（详见 [整合包格式](../pack-author/nsumpack.md)）。

## 文档校验

文档与代码同 PR 时先跑门禁：

```powershell
pwsh -NoProfile -File tools/docs_check.ps1
pwsh -NoProfile -File tools/build_docs.ps1 -Profile with-editor -Out '<输出目录>'
```

`docs_check` 检查：front-matter（`title`/`status`/`lang`）、中英页面一一对应、链接与图片可达、锚点（warning）、`user/` 与 `pack-author/` 不得相对链接 `developer/`/`extensions/`、占位槽位登记、代码围栏成对、`_meta.json` 条目与页面一致。通过时输出 `docs_check OK`；`planned` 页只计数量不算错误，发布前必须清零。

## 失败模式

| 现象 | 原因 | 处理 |
|---|---|---|
| `E2E` 在 A 组失败，`files[]` 为空 | `file_manifest` 或 `pointer_files` 格式写成数组 | 见 [数据模型](architecture/data-model.md)，两个字段都是对象 |
| 同步后目标多出 `.pointer` | 指针未解析（`No resolver found`） | 确认 `deploy/pointers` 有对应 DLL 与 meta；`BuildEngine::init` 会扫描该目录 |
| 中文目录报 `No mapping for the Unicode character` | 路径经 ANSI 解释 | 全面改用 `fsu8::` / `fs::u8path` / `u8string`，EXE 入口重建 UTF-8 argv |
| 冒烟时关闭崩溃、0xC0000374 | 共享头类成员变更但 TU 未重编 | 删旧 obj 或 `--clean-first` 全量重建后重跑冒烟 |
| `docs_check` 报 `broken link/image` | 相对路径写错或目标页未创建 | 按站点内相对 `.md` 路径修正，不引用不存在的图片 |
| bridge 自检全 SKIP | 没有带 `-Dnsum.selftest.*` 或没放 bridge jar | 用 `-DryRun` 打出命令核对 `-D`，确认 `testbed/mods` 内的 jar 版本 |

## 相关文档

- [发布](./release.md)
- [版本管理](./versioning.md)
- [贡献指南](./contributing.md)
- [同步引擎](architecture/sync-engine.md)
- [安装器 CLI](../extensions/installer/cli.md)
- [客户端 Bridge 用法](../extensions/client-bridge/usage.md)
