---
title: 版本管理
description: NSUM 双版本号（external/internal/full 六段）的定义、version.json、递增时机、bump 脚本与版本注入位置。
lang: zh
status: complete
updated: 2026-11-27
tags: [versioning, cmake, release]
---

# 版本管理

本页解释 NSUM 为什么有两个版本号、各自何时递增、唯一数据源在哪，以及版本号最终出现在哪些构建产物与界面上。发布时的完整操作步骤见 [发布](./release.md)。

## 双版本号模型

| 名称 | 形态 | 含义 | 递增时机 |
|---|---|---|---|
| 外部版本 `external` | `x.x.x` | 对用户公开的发布版本，也是 Git 标签（`v` + 版本）与 Release 名 | 仅在维护者明确宣布「发布一版」时 |
| 内部版本 `internal` | `x.x.x` | 内部构建序号，用于区分同一次发布之间的迭代 | 每次任务/功能批次完成时 |
| 完整版本 `full` | `x.x.x.x.x.x`（6 段） | `external` + `internal` 拼接，用于资源与日志精确定位构建 | 随两者自动变化 |
| Git 提交 | 短哈希（可带 `-dirty`） | 构建对应的源码状态 | 构建时自动注入 |

设计动机：日常小改动不应污染公开版本号，但又必须能区分「同一公开版本的第几次内部构建」。两者分开后，对外只讲 `1.0.5`，排查问题时看 `1.0.5.0.1.11` 与提交哈希。

## version.json（唯一来源）

仓库根的 `version.json` 是唯一手写来源，`CMakeLists.txt` 在 configure 阶段读取并校验（两段都必须是 `x.x.x`，否则 `FATAL_ERROR`）：

```json
{
  "external": "1.0.5",
  "internal": "0.1.11"
}
```

- 不要在其他文件里手改版本号；`NSUM_INSTALLER_VERSION` 是安装器专属版本，单独定义在根 `CMakeLists.txt` 的 `INSTALLER_ONLY_BUILD` 分支（当前 `1.0.0`），与双版本号无关。
- 不要把整合包的 `pack_format`（`.nsumpack` 文件结构版本，当前为 2）当作产品版本：它只在包结构不兼容时递增，见 [ADR 0006](./adr/0006-nsumpack-v2.md)。

## 递增：tools/bump_version.ps1

脚本参数与语义（`param` 中的 `-Part` 只接受下表六个值）：

| 参数 | 效果 |
|---|---|
| `-Part external-major` | `1.4.2` → `2.0.0` |
| `-Part external-minor` | `1.4.2` → `1.5.0` |
| `-Part external-patch` | `1.4.2` → `1.4.3` |
| `-Part internal-major` | `0.4.2` → `1.0.0` |
| `-Part internal-minor` | `0.4.2` → `0.5.0` |
| `-Part internal-patch` | `0.4.2` → `0.4.3` |
| `-Set external=1.2.3` / `-Set internal=0.5.2` | 直接指定（仍校验 `x.x.x`） |
| `-Show`（或不带参数） | 只打印当前版本，不写文件 |

```powershell
# 只查看
pwsh tools/bump_version.ps1 -Show

# 日常迭代：只动内部版本
pwsh tools/bump_version.ps1 -Part internal-patch

# 发布：先确认内部版本已是最终值，再动外部版本
pwsh tools/bump_version.ps1 -Part external-patch

# 手动对齐
pwsh tools/bump_version.ps1 -Set internal=0.1.12
```

脚本会把两段写回 `version.json`（UTF-8），并打印形如 `[1.0.5]（内部版本号：0.1.11）完整版本：1.0.5.0.1.11` 的一行结果，可据此核对。

递增约定：

1. 普通任务/修复完成 → `internal-patch`；一批功能合并 → `internal-minor`；内部大改（数据结构、模块拆分）→ `internal-major`。
2. 对外发布 → `external-patch`（默认）/`external-minor`/`external-major`；**同一次发布不再递增内部版本**，除非发布后还要出补丁构建。
3. 递增后必须重新 configure 并重建，否则宏还是旧值（见下文）。

## 版本注入

`CMakeLists.txt` 读取 `version.json` 后生成四个预处理宏，通过 `add_compile_definitions` 注入**所有** target：

| 宏 | 值 |
|---|---|
| `NSUM_EXTERNAL_VERSION` | `external`（`x.x.x`） |
| `NSUM_INTERNAL_VERSION` | `internal`（`x.x.x`） |
| `NSUM_FULL_VERSION` | `external.internal`（6 段） |
| `NSUM_GIT_COMMIT` | `git rev-parse --short HEAD`，工作区脏时追加 `-dirty` |

`project(NeoServerUpdateModpack VERSION ${NSUM_EXTERNAL_VERSION})` 只取外部版本，供 CMake 自身与资源模板使用。

### Windows 资源（VERSIONINFO）

`nsum_add_version_info(<target> <描述> <产品名>)` 用 `resources/version.rc.in` 为每个 EXE/DLL 生成版本资源（插件 DLL 也会调用）：

| 字段 | 取值 |
|---|---|
| `FILEVERSION` / `PRODUCTVERSION` | 外部版本 3 段 + `0`（VERSIONINFO 只支持 4 段数字） |
| `FileVersion` / `ProductVersion` / `BuildVersion` | 6 段完整版本 |
| `InternalVersion` | 内部版本 3 段 |
| `GitCommit` | 短哈希（含 `-dirty`） |

### 运行时呈现

| 位置 | 内容 |
|---|---|
| 启动日志 | `NSUM build info: mode=… external=… internal=… full=… git=…`；随后一行 `NSUM version line: [x]（内部版本号：y） full=… commit=…` |
| CLI banner / `[INIT]` | `NeoServerUpdateModpack [x]（内部版本号：y） build=…` |
| CLI `-v` / `info version` | `Version` / `Internal version` / `Full version` / `Git commit`；`--json` 为 `version`、`internal_version`、`full_version`、`git_commit`、`build_type` |
| GUI 状态栏 | `[x]（内部版本号：y）`，悬浮提示显示完整版本与 Git 提交 |
| GUI「关于」弹窗 | 版本、内部版本、完整版本、Git 提交 |
| 编辑器 About | 同一组宏，与主程序一致 |

## 改动版本号后的操作

宏在 configure 阶段固化，且 `add_compile_definitions` 变化不会可靠触发所有 TU 重编：

```powershell
cmake --preset msvc
cmake --build build --clean-first --target neo_deploy
```

验证：

```powershell
$exe = '<仓库根>\build\deploy\NeoServerUpdateModpack.exe'
& $exe -v
& $exe info version --json
```

`info version` 的 `full_version` 必须等于 `version.json` 的 `external + "." + internal`；`git_commit` 必须与 `git rev-parse --short HEAD` 一致（工作区有改动时期望带 `-dirty`）。

## 版本号与产物命名

| 产物 | 命名 |
|---|---|
| Runtime zip | `NSUM_Runtime_<external>.zip` |
| Runtime CLI zip（HIC 分发） | `NSUM_Runtime_CLI_<external>.zip` |
| 整包安装器 | `NSUM_Installer_<NSUM_INSTALLER_VERSION>.exe` |
| bridge jar | `nsum-bridge-<bridge 版本>.jar`（版本含 FLF 构建号，如 `0.4.4.17`） |
| 元数据条目 | `version` = 外部版本，`meta_version` = 元数据条目序号，二者独立递增 |

## 失败模式

| 现象 | 原因 | 处理 |
|---|---|---|
| `-v` 显示旧版本 | 只改了 `version.json` 没重编 | 重新 configure + 重建；必要时 `--clean-first` |
| configure 报 `version.json external must be x.x.x` | 手改成了 `1.0` 或带前缀 `v` | 恢复为三段纯数字 |
| `info version` 的 `git_commit` 为 `unknown` | 构建环境找不到 Git | 安装 Git 或让 CMake 能找到 `git`；发布构建必须有确定提交 |
| 安装器版本与运行时版本混淆 | `NSUM_INSTALLER_VERSION` 与 `version.json` 是两个版本 | 安装器版本只在根 `CMakeLists.txt` 改；运行时版本只改 `version.json` |
| 元数据 Latest 与 zip 不一致 | 发布时漏改 `metadata/NSUM.json` | 按 [发布](./release.md) 的 metadata 步骤补齐并等站点生效 |

## 相关文档

- [发布](./release.md)
- [测试与验证](./testing.md)
- [贡献指南](./contributing.md)
- [ADR 0006 · 清单型 .nsumpack](./adr/0006-nsumpack-v2.md)
