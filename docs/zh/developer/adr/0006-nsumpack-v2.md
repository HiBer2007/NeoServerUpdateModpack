---
title: ADR 0006 · 清单型 .nsumpack（pack_format 2）
description: 整合包默认只发 nsum.index.json 清单，内容由客户端按 commit 同步；附带文件必须用正则显式声明，不默认灌入 overrides。
lang: zh
status: complete
updated: 2026-11-27
---

# ADR 0006 · 清单型 .nsumpack（pack_format 2）

## 状态

已接受（2026-11-27，用户裁定）

## 背景

第一代 `.nsumpack` 把构建产物整体塞进 `overrides/`，本质是一个 zip 快照。问题在真实使用中暴露：

- 同一份内容既在仓库里、又在包里，包体随整合包规模线性膨胀；
- 客户端拿到的是一次性快照，无法知道来源提交，也无法按 commit 增量同步；
- 服务器更新后，旧包内容与仓库状态不一致，排障时无法判断「包是什么时候打的」；
- 客户端只需要「从哪个仓库的哪个提交同步哪个分支」，绝大多数场景根本不需要携带文件。

## 决策

- **默认只写清单**：包根只有 `nsum.index.json`，`pack_format: 2`、`schema: 2`、`type: nsum`。文件结构版本与整合包自身版本互相独立。
- **清单字段**：
  - `repository{url, default_branch, branch, git_branch, commit}`：来源仓库、默认分支、整合包分支、Git 分支、构建提交；
  - `instance{repo, branch, gitBranch, branchType}`：客户端建实例所需的定位信息；
  - `files[]`：`path` / `size` / `sha256` / `commit` / `git_branch`，**只登记不含字节**；
  - `embedded[]`：实际打进包的文件，附命中 `pattern`、`commit`、`git_branch`；
  - `embed_patterns[]`：本次使用的内嵌规则；
  - `runtime.minVersion`：最低 Runtime 版本；
  - 以及 `name`/`author`/`version`/`gameVersion`/`modloader`/`modloaderVersion`/`core`/`description`。
- **附带文件必须显式声明**：`--embed-patterns`（ECMAScript 正则，对仓库相对路径 `search`，换行或 `;` 分隔）。只有命中的文件才写入 `overrides/**` 并登记进 `embedded[]`；未声明时不内嵌任何文件。
- **`save/` 永不进包**：存档配置属于服务端下发内容，由 L3 同步处理，不接受打包。
- **客户端按来源同步**：bridge 读清单后调用 `exec build --format hmcl`，用 `repository.url` + `branch`/`git_branch` 同步；来源 `commit` 记入实例标记用于追溯（不作为同步 pin）。`embedded[]` 按 SHA-256 缓存到 `.hmcl/nsum/cache/files/`（跨整合包/实例复用，命中直接复制，未命中解压后校验 sha 再入缓存）。
- **兼容回退**：无 `embedded` 字段的旧包仍按老语义解压全部 `overrides/`；实例标记新增 `packFormat`/`sourceCommit`/`sourceGitBranch`/`defaultBranch`。
- **扩展名放行**：HMCL 原生只认 `zip`/`mrpack`，`.nsumpack` 由 bridge 的 `MixinModpackHelper` 注入 `isFileModpackByExtension` 放行。

## 备选方案

| 方案 | 优点 | 缺点 |
|---|---|---|
| 继续整包 `overrides/` | 客户端实现简单、离线可用 | 包体大、来源不可追溯、无法增量 |
| 纯清单、完全不支持附带 | 最干净 | 无法携带少量必须的客户端覆盖文件 |
| 全量文件清单 + 每次下载全部内容 | 简单直接 | 大包下载量高，无法利用本地已有缓存 |
| 清单 + 显式内嵌（本决策） | 包最小、来源可追溯、附带文件可控 | 需要维护正则与缓存，旧包需要回退分支 |

## 后果

- 正面：包体从数十 MB 降到 KB 级（fixture 无内嵌 1,058 B）；来源 commit 可追溯；`--embed-patterns ^config/.*\.json$` 实测只内嵌 2/10 个文件（1,631 B）；附带文件按内容寻址缓存，二次安装即使把 `overrides` 从 zip 剥离也能命中缓存。
- 负面：客户端必须能访问来源仓库，否则无法同步；`embed_patterns` 写错会静默不带文件（有 `embedded[]` 与包内列表可核对）；`pack_format` 升级需要客户端同步支持。
- 影响范围：`NeoExporter_NSUM`、`neo_deploy` 的打包流程、CLI `exec build/export`（新增 `--embed-patterns`）、bridge 导入与缓存、实例标记、下游打包验收。

## 相关

- [.nsumpack 格式](../../pack-author/nsumpack.md)
- [NSUM 导出器](../../extensions/exporters/nsum.md)
- [ADR 0001 · 指针文件与内容寻址缓存](./0001-pointer-cache.md)
- [测试与验证](../testing.md)
