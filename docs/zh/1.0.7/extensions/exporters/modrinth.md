---
title: Modrinth 导出器
description: NeoExporter_Modrinth：生成 .mrpack（Modrinth Modpack Format）。
lang: zh
status: complete
updated: 2026-11-27
---

# Modrinth 导出器

生成 `.mrpack`：`modrinth.index.json` + `overrides/`（可选 `client-overrides/`、`server-overrides/`），用于发布到 Modrinth 或支持该格式的启动器。

## 元数据

| 项 | 值 |
|---|---|
| format | `modrinth` |
| 扩展名 | `.mrpack` |
| 必填字段 | `name`、`version` |
| 可选字段 | `summary`、`author`、`description` |

## 产物结构

```text
<pack>.mrpack
├─ modrinth.index.json     # 依赖、文件列表（含 downloads/hashes）
├─ overrides/              # 客户端+服务端共用覆盖
├─ client-overrides/       # 仅客户端
└─ server-overrides/       # 仅服务端
```

`modrinth.index.json` 关键字段：

| 字段 | 说明 |
|---|---|
| `formatVersion` | 1 |
| `game` | `minecraft` |
| `versionId` | 版本标识（来自 `version` 字段） |
| `name` | 整合包名称 |
| `files[]` | `path`、`hashes`、`downloads`、`env` |
| `dependencies` | `minecraft`、`fabric-loader`/`neoforge` 等 |

## 用法

```powershell
NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack base --git-branch main `
    --format modrinth --export out.mrpack
```

## 注意

- 指针文件会转换为 `files[]` 的 `downloads` 条目；
- 未匹配到下载地址的文件进入 `overrides/`；
- 发布到 Modrinth 前需检查 `env`（client/server）与依赖版本。

## 相关

- [导出器总览](./index.md)
- [构建与导出](../../pack-author/build-and-export.md)
