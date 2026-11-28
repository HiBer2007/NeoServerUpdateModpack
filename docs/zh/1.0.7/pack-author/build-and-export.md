---
title: 构建与导出
description: 从仓库构建到四种导出格式：流水线、命令、字段与常见问题。
lang: zh
status: complete
updated: 2026-11-27
---

# 构建与导出

## 流水线

```mermaid
flowchart LR
    A[选择仓库/分支] --> B[fetch + checkout git 分支]
    B --> C[合并父分支]
    C --> D[扫描文件/清单]
    D --> E[指针解析/下载]
    E --> F[同步策略 + L3]
    F --> G[导出格式]
```

## 格式对比

| 格式 | 扩展名 | 体积 | 适用 |
|---|---|---|---|
| `nsum` | `.nsumpack` | ~1 KB（默认） | 服务器玩家、HMCL + bridge |
| `mcbbs` | `.zip` | 完整文件 | 论坛/网盘/通用启动器 |
| `modrinth` | `.mrpack` | 完整文件 | 发布到 Modrinth |
| `hmcl` | 目录 | 完整文件 | 本地实例同步 |

## 命令

```powershell
# 预览（不落盘）
NeoServerUpdateModpack.exe info preview --repo <仓库> --modpack base --format nsum --json

# 构建并导出
NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack base --git-branch main `
    --format nsum --export out.nsumpack

# 显式内嵌文件
NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack base --git-branch main `
    --format nsum --embed-patterns '^config/.*\.json$' --export out.nsumpack

# 仅导出（复用已有构建）
NeoServerUpdateModpack.exe exec export --export out.nsumpack --format nsum --repo <仓库> --modpack base
```

GUI：向导依次选择仓库 → 分支 → 整合包 → 导出类型 → 目录 → 额外信息 → 检查清单 → 构建。

## 参数

| 参数 | 说明 |
|---|---|
| `--repo` | 仓库地址或本地路径 |
| `--modpack` | 整合包分支（`workspace.json` 的 branches） |
| `--git-branch` | Git 分支（checkout 用，默认与 modpack 同名） |
| `--format` | `nsum`/`mcbbs`/`modrinth`/`hmcl` |
| `--export` | 输出文件或目录 |
| `--embed-patterns` | 仅 nsum：内嵌文件正则（可重复/换行/`;`） |
| `--json` | JSON 输出 |

## 导出字段

| 格式 | 必填 | 可选 |
|---|---|---|
| nsum | name、version | author、description、embed_patterns |
| mcbbs | name、version | author、description |
| modrinth | name、version | summary、author、description |
| hmcl | — | — |

## 常见失败

| 现象 | 原因 | 处理 |
|---|---|---|
| `pathspec ... did not match` | git 分支不存在/名称混淆 | 传 `--git-branch main` |
| `No resolver found` | 指针解析器缺失 | 检查 `pointers/` |
| 导出包为空/缺文件 | 分支选错/父链断 | `info preview` 对比 |
| 体积异常大 | 误内嵌全部文件 | 去掉 `--embed-patterns` 或收窄正则 |

## 相关

- [`.nsumpack` 格式](./nsumpack.md)
- [指针](./pointers.md)
