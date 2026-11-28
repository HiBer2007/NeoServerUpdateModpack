---
title: HMCL 导出器
description: NeoExporter_HMCL：把构建产物同步到 HMCL 工作目录/实例。
lang: zh
status: complete
updated: 2026-11-27
---

# HMCL 导出器

`NeoExporter_HMCL` 不生成压缩包，而是把构建产物**直接同步到目标目录**（通常是 HMCL 实例目录）。它是玩家侧 Runtime 同步使用的格式。

## 元数据

| 项 | 值 |
|---|---|
| format | `hmcl` |
| 扩展名 | 无（目录） |
| 字段 | 无 |

## 用法

```powershell
# 同步到实例目录
NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack base --git-branch main `
    --format hmcl --export <实例目录>
```

bridge 启动时等价调用（内部）：

```text
exec build --format hmcl --export <实例目录> --repo <仓库> --modpack <分支> --git-branch <分支>
```

## 行为

1. 合并分支并生成输出；
2. 按 `sync_policies` 写入目标目录（mirror 会删除多余项）；
3. 指针文件按缓存/下载解析；
4. 应用 `serverconfig`（L3）；
5. 可选：生成 MCLHR store 并硬链接到 `mods/`（`applyMclhrStore`）。

## 保护

`mirror` 删除前保护：

- 根级 `options.txt`、`log4j2.xml`、`natives*`、根级 `*.jar`；
- `saves/`；
- `.NSUM/`、`<id>.json`、`icon.*` 等实例文件。

## 注意

- `--export` 必须是目录，不是文件；
- 不要指向 HMCL 安装目录本身（会污染启动器文件）；
- 同步日志会记录 copied/deleted 数量。

## 相关

- [导出器总览](./index.md)
- [同步与启动](../../user/sync-and-launch.md)
