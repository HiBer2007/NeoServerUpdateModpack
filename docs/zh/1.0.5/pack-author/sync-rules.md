---
title: 同步规则
description: NSUM 三层同步（L1/L2/L3）、sync_policies、文件标记与 serverconfig 规则。
lang: zh
status: complete
updated: 2026-11-27
---

# 同步规则

同步决定“仓库里的哪些文件、以什么方式进入实例”。NSUM 分三层：

| 层级 | 来源 | 说明 |
|---|---|---|
| L1 | 基础分支（通常 `base`） | 所有分支共享的基础内容 |
| L2 | 整合包分支（含父链合并） | 分支覆盖与最终构建产物 |
| L3 | 存档 `serverconfig` | 按存档目录同步服务端配置 |

## 同步策略 sync_policies

```json
{
  "default_folder_policy": "mirror",
  "folders": {
    "mods": "mirror",
    "config": "mirror",
    "resourcepacks": "incremental_add",
    "shaderpacks": "incremental_overwrite",
    "logs": "skip"
  },
  "files": {
    "config/partial.json": { "mode": "partial", "tracked_keys": ["keep", "server"] }
  }
}
```

| 策略 | 行为 |
|---|---|
| `mirror` | 以仓库为准：新增/覆盖/删除多余文件 |
| `incremental_add` | 只新增，不覆盖/删除 |
| `incremental_overwrite` | 新增并覆盖，不删除 |
| `skip` | 不同步 |
| `partial`（文件级） | 只同步 `tracked_keys` 指定的配置键 |

> `default_folder_policy` 作用于未显式声明的目录；根目录有保护清单（`options.txt`、`log4j2.xml`、`natives*`、根级 jar 等）。

## 文件标记

分支清单可为单个文件设置标记：

| 标记 | 效果 |
|---|---|
| `delete` | 在合并时删除父分支同名文件 |
| `override` | 强制用本分支文件覆盖 |

## serverconfig（L3）

仓库路径：

```text
branches/<branch>/save/[save]/serverconfig/
```

- `save` 是字面目录名；`[save]` 是占位（任意存档目录名）；`serverconfig` 为同步目标；
- 存档级规则存放在 `.rule/globle.json`（默认模式）与 `.rule/list.json`（文件清单）；
- 由 Runtime 在同步时把对应文件写入实例存档目录。

```json
// .rule/globle.json
{ "default_mode": "full", "folder_mode": "mirror" }
```

## 自定义 mod 目录

`branch_config/<branch>.json`：

```json
{
  "custom_mods": {
    "enabled": true,
    "path": "D:/shared-mods",
    "merge_order": "post"
  }
}
```

- `merge_order=post`：仓库 mod 之后合并（同名以外部为准）；
- 用于多整合包共享客户端 mod，避免重复入库。

## 合并与删除的保护

- `mirror` 删除多余文件前会保护：
  - 根级关键文件（`options.txt`、`log4j2.xml`、`natives*`、根级 `*.jar`）；
  - `saves/` 存档目录；
  - 显式策略路径与实例标记文件（`.NSUM/`、`<id>.json`、`icon.*`）。
- 删除/覆盖动作会写日志，出问题时按日志恢复。

## 验证

```powershell
# 预览构建结构（不落盘）
NeoServerUpdateModpack.exe info preview --repo <仓库> --modpack base --format nsum --json

# 实际构建到实例
NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack base --git-branch main --format hmcl --export <实例目录>
```
