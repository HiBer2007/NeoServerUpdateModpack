---
title: MCBBS 导出器
description: NeoExporter_MCBBS：生成 MCBBS / PCL / HMCL 通用整合包 zip。
lang: zh
status: complete
updated: 2026-11-27
---

# MCBBS 导出器

生成 `manifest.json` + `mcbbs.packmeta` + `overrides/` 的通用整合包 zip，可被 MCBBS、PCL、HMCL 等启动器导入。

## 元数据

| 项 | 值 |
|---|---|
| format | `mcbbs` |
| 扩展名 | `.zip` |
| 必填字段 | `name`、`version` |
| 可选字段 | `author`、`description` |

## 产物结构

```text
<pack>.zip
├─ manifest.json        # 游戏版本、加载器、作者、文件索引
├─ mcbbs.packmeta       # MCBBS 元数据
└─ overrides/           # 实际文件（mods/config/...）
```

## 用法

```powershell
NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack base --git-branch main `
    --format mcbbs --export out.zip
```

GUI：向导选择「MCBBS 整合包」，填写名称/版本/作者/描述。

## 注意

- zip 内含完整文件（体积较大），适合论坛/网盘分发；
- 不包含游戏本体与加载器；
- 需要更新时玩家需重新下载整包（不像 `.nsumpack` 增量同步）。

## 相关

- [导出器总览](./index.md)
- [构建与导出](../../pack-author/build-and-export.md)
