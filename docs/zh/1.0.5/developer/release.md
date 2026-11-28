---
title: 发布流程
description: NSUM 完整发布 runbook：版本递增、构建与部署、清理打包、GitHub Release、元数据更新、上线校验与提交。
lang: zh
status: complete
updated: 2026-11-27
tags: [release, runbook, github, metadata]
---

# 发布流程

本页是从「决定发一版」到「线上可下载」的完整操作序列。每一步都给出可复制命令与验收点；顺序不能随意调整（安装器在 configure 阶段固化内嵌文件清单，必须在部署目录定稿之后再构建）。

## 0. 前置条件

| 项 | 要求 |
|---|---|
| 工具 | `pwsh`、`cmake`（VS 自带或已入 PATH）、`git`、`gh`（已登录）、`7z`（7-Zip，已入 PATH） |
| 构建环境 | Qt 动态版（`msvc` 预设）、静态 Qt 前缀（`installer-static` 预设，用于整包安装器） |
| 仓库状态 | `main` 与 `origin/main` 同步；`git status --porcelain` 中只有本次要发布的改动 |
| submodule | 全部已初始化（`git submodule update --init --recursive`） |
| 待发布功能 | 已合入 `main`，端到端与冒烟按 [测试与验证](./testing.md) 通过 |

后续命令假设下面这些变量已在同一个 PowerShell 会话中设置：

```powershell
$repo   = '<仓库根>'            # 主工程工作副本
$out    = '<产物输出目录>'       # 存放 zip 与发布说明
$meta   = '<metadata 仓库根>'    # 元数据站点仓库
$sevenZip = '7z'

$v    = Get-Content (Join-Path $repo 'version.json') -Raw | ConvertFrom-Json
$ver  = $v.external
$full = "$($v.external).$($v.internal)"
New-Item -ItemType Directory -Force $out | Out-Null
"当前完整版本：$full"
```

> **注意**：当前发布链沿用 `msvc` 预设（Debug 配置），历史发布包即此配置；若要改发 Release 二进制，属于独立变更，需重走本页全部验收步骤。

## 1. 递增版本号

外部版本只在「发布一版」时递增；内部版本在发布前应已是最终值。

```powershell
cd $repo
pwsh -NoProfile -File tools/bump_version.ps1 -Part external-patch
pwsh -NoProfile -File tools/bump_version.ps1 -Show

# 重新读取版本变量（第 0 步读的是递增前的值）
$v    = Get-Content (Join-Path $repo 'version.json') -Raw | ConvertFrom-Json
$ver  = $v.external
$full = "$($v.external).$($v.internal)"
"发布版本：$full"
```

验收：输出行形如 `[1.0.6]（内部版本号：0.1.11）完整版本：1.0.6.0.1.11`，与预期一致。规则细节见 [版本管理](./versioning.md)。

## 2. 构建主工程并部署

```powershell
cd $repo
cmake --preset msvc
cmake --build build --target neo_deploy
```

- 若 `nbtcpp`/`zlib` 报 `C1083` 缺 `stddef.h`/`stdio.h`，先执行 vcvars 初始化再重试（见 AGENTS.md 构建命令）。
- `neo_deploy` 会把 EXE、插件 DLL+meta、`docs/`（`build_docs.ps1 -Profile with-editor`）、vcpkg 运行时 DLL 复制到 `build/deploy`。
- 部署前确认没有正在运行的 NSUM/编辑器进程，被占用的 EXE 会让复制静默跳过。

验收：

```powershell
& "$repo\build\deploy\NeoServerUpdateModpack.exe" info version --json
```

`full_version` 等于 `$full`，`git_commit` 等于 `git rev-parse --short HEAD`（工作区脏时应带 `-dirty`，发布前应提交干净）。

## 3. 清理部署目录

`build/deploy` 会积累本机运行产物，直接打包会泄漏本地数据甚至把包撑到数百 MB（MCLHR store 曾导致 734 MB 包）：

```powershell
$deploy = Join-Path $repo 'build\deploy'
Remove-Item -Recurse -Force "$deploy\stores", "$deploy\crash-report", "$deploy\config" -ErrorAction SilentlyContinue
Get-ChildItem $deploy -Recurse -File -Filter '*.log' | Remove-Item -Force
Get-ChildItem $deploy -Recurse -File -Filter '*.dmp' | Remove-Item -Force

# 体积与条目数粗检（CLI 分发约 50 MB 量级）
"{0:N1} MB" -f ((Get-ChildItem $deploy -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)
(Get-ChildItem $deploy -Recurse -File).Count
```

> **警告**：不要删除 `deploy/tools`（内置 Git 与 mclhr 相关工具所在）和 `deploy/docs`。

## 4. 构建安装器与 CLI 分发目录

`hci_gui` 的内嵌清单（`HCI_PRODUCT_FILES` → qrc）在 **configure 时固化**；第 3 步删过 `deploy/stores` 等目录后必须重新 configure，否则 ninja 会报文件缺失：

```powershell
cd $repo
cmake --preset installer-static
cmake --build build_nsum_installer --target hci_cli_dist hci_gui
```

产物：

- `build_nsum_installer\nsum_cli_dist\`：`hci_cli.exe` + `product.json` + `install.json`（无快捷方式流程）+ `LICENSE.txt` + `deploy/`
- `build_nsum_installer\NSUM_Installer_<安装器版本>.exe`：单文件整包安装器

验收：`& "$repo\build_nsum_installer\hci_cli.exe" --version` 退出码 0，版本与 `CMakeLists.txt` 中的 `NSUM_INSTALLER_VERSION` 一致。

## 5. 打 zip

发布两个包：Runtime CLI 分发包（bridge 自动安装用，元数据 `download_link` 指向它）与 Runtime 整目录包（手工部署用）。

```powershell
$zipRuntime = Join-Path $out "NSUM_Runtime_$ver.zip"
$zipCli     = Join-Path $out "NSUM_Runtime_CLI_$ver.zip"
Remove-Item $zipRuntime, $zipCli -ErrorAction SilentlyContinue

# 整目录包：deploy 内容位于 zip 根，排除本地运行产物
& $sevenZip a -tzip $zipRuntime "$repo\build\deploy\*" -r `
    '-xr!stores' '-xr!crash-report' '-xr!config' '-x!*.log' '-x!*.dmp'

# CLI 分发包：nsum_cli_dist 内容位于 zip 根（hci_cli.exe 必须在顶层）
& $sevenZip a -tzip $zipCli "$repo\build_nsum_installer\nsum_cli_dist\*" -r `
    '-xr!stores' '-xr!crash-report' '-xr!config' '-x!*.log' '-x!*.dmp'
```

历史 runbook 里写作 `/XD stores crash-report config /XF *.log`，语义相同：这三个目录与日志必须排除。

打包验收：

```powershell
& $sevenZip l $zipCli | Select-String '(?i)(stores|crash-report)[\\/]|[\\/]config[\\/]|\.log$'   # 期望无输出
Get-Item $zipRuntime, $zipCli | Select-Object Name, Length
```

再按 [测试与验证](./testing.md) 的「打包验收」做一次静默安装 + `info version` 对照，确认包能独立跑起来。

## 6. 创建 GitHub Release

```powershell
# 发布说明：写版本亮点、破坏性变更、配套 bridge 版本要求
notepad "$out\notes.md"

gh release create "v$ver" `
    $zipRuntime $zipCli `
    --title "NSUM $ver" `
    --notes-file "$out\notes.md" `
    --repo HiBer2007/NeoServerUpdateModpack
```

> **提示**：`gh` 会输出 Release URL；把该 URL 记下来填进元数据 `release_page`。

## 7. 更新元数据站点

元数据仓库维护两个清单：`metadata/NSUM.json`（运行时）与 `metadata/nsum-bridge.json`（bridge jar）。二者结构相同：

| 字段 | 含义 |
|---|---|
| `Latest` | 当前推荐版本（客户端据此判断更新） |
| `List[]` | 版本清单，**新版本插到数组最前面** |
| `List[].version` / `meta_version` | 产品版本 / 元数据条目序号（每次追加条目递增） |
| `download_link` | 下载地址；≥10 MB 走 GitHub Release，<10 MB 可托管站点 `public/` |
| `release_page` | GitHub Release 页面 |
| `sha_256` / `size_bytes` | 产物校验与体积 |
| `bridge_versions`（NSUM）/ `nsum_versions`（bridge） | 兼容区间 |
| `notes` | 本版变更摘要 |

计算产物哈希：

```powershell
Get-FileHash $zipCli -Algorithm SHA256 | Select-Object Hash
(Get-Item $zipCli).Length
```

操作步骤：

1. 修改 `Latest` 为新外部版本；
2. 在 `List` 头部插入条目（`meta_version` 递增），字段按上表；
3. 若同时发布 bridge jar 且体积 <10 MB，把 jar 复制到 `public/nsum-bridge/`，`download_link` 指向站点路径；
4. 提交并推送：

```powershell
cd $meta
git status --porcelain                 # 只应包含 metadata/*.json 与 public/nsum-bridge/ 下的 jar
git add -A
git commit -m "NSUM: release $ver"
git push
```

> **注意**：站点为 Vercel 部署，push 后约 30–60 秒生效。不要立刻断言失败，先等待再复验。

## 8. 上线校验

```powershell
# 1) 元数据 Latest 已切换
(Invoke-RestMethod 'https://metadata.hibernet.top/api/metadata?type=NSUM' -TimeoutSec 30).Latest

# 2) 新条目的下载地址可达（GitHub Release）
(Invoke-WebRequest -Method Head 'https://github.com/HiBer2007/NeoServerUpdateModpack/releases/download/v<版本>/NSUM_Runtime_CLI_<版本>.zip' -TimeoutSec 30).StatusCode

# 3) bridge 清单与 jar 可达
(Invoke-RestMethod 'https://metadata.hibernet.top/api/metadata?type=nsum-bridge' -TimeoutSec 30).Latest
(Invoke-WebRequest -Method Head 'https://metadata.hibernet.top/nsum-bridge/nsum-bridge-<版本>.jar' -TimeoutSec 30).StatusCode
```

`type` 名称以元数据仓库 `lib/metadata-mapping.ts` 的映射为准（当前为 `NSUM`、`nsum-bridge`）。三项都应返回新版本与 HTTP 200。

再用发布包做一次真实安装（不要只信本机构建目录）：

```powershell
$probe = Join-Path $env:TEMP "nsum_release_probe_$ver"
Remove-Item $probe -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive $zipCli -DestinationPath "$probe\dist"
& "$probe\dist\hci_cli.exe" --cli --json --silent --product product.json --flow install.json --path "$probe\runtime"
& "$probe\runtime\NeoServerUpdateModpack.exe" info version --json
```

期望：安装退出码 0、`install.conf` 存在且 `git_path` 指向内置 Git、`full_version` 与发布版本一致。

## 9. 提交发布记录

```powershell
cd $repo
git status --porcelain                 # 先确认没有误入的本机文件
git add -A
git commit -m "release: NSUM $ver"
git push
```

- 提交信息统一用 `release: NSUM <外部版本>`。
- 同步 `PLAN.md` 的功能更新记录与已知问题表（维护流程要求），并记录本版产物大小与 SHA-256。
- 打标签：`gh release create` 会按 `v<版本>` 自动创建标签；若手工打标签，保证与 Release 同名。不要重复创建同名标签。
- 最后核对 Release 附件、元数据、本地提交三者版本一致。

## 10. bridge jar 的发布（按需）

bridge 与主程序分仓、分版本，只在 bridge 有改动时发布：

```powershell
# 先确保 FLF 库存在（新产物文件名会带 FLF 构建号）
cd '<FLFHMCL 工作区>'
.\gradlew.bat buildAll

cd '<bridge 仓库>'
.\gradlew.bat build
Get-ChildItem build\libs -Filter 'nsum-bridge-*.jar'
```

- 产物名形如 `nsum-bridge-0.4.4.17.jar`；若新构建号与本仓库既定版本不一致，按现有 jar 发布，不要强改版本号。
- 发布（把 `$bridgeVer` 换成实际 jar 版本）：

```powershell
$bridgeVer = '0.4.4.17'
gh release create "v$bridgeVer" "build\libs\nsum-bridge-$bridgeVer.jar" `
    --title "nsum-bridge $bridgeVer" `
    --notes-file "$out\bridge-notes.md" `
    --repo HiBer2007/nsum-bridge
```

- <10 MB 的 jar 同时复制到元数据仓库 `public/nsum-bridge/`，并在 `nsum-bridge.json` 的 `Latest` 与 `List` 头部登记 sha256、size、`nsum_versions`（最低兼容的 NSUM 版本）。

## 回滚

| 场景 | 操作 |
|---|---|
| 元数据刚推送、产物有问题 | 把 `Latest` 改回上一版本并推送；保留坏版本条目（便于排查），不要让客户端再选中它 |
| GitHub Release 附件命名错误 | 删除该 Release 重新 `gh release create`，客户端只认元数据链接 |
| 运行时回归 | 恢复上一版本 tag 的构建产物重新上传，`meta_version` 递增并写清回归原因 |

## 相关文档

- [版本管理](./versioning.md)
- [测试与验证](./testing.md)
- [贡献指南](./contributing.md)
- [安装器流程](../extensions/installer/flow.md)
- [ADR 0004 · 声明式安装流程](./adr/0004-hic-flow.md)
