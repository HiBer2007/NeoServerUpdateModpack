---
title: ADR 0003 · MCLHR mod store 与硬链接投影
description: 把整合包模组收进运行时 store，用硬链接投影回 mods/，由 MCLHR 生成启动计划而不把 mod 塞进 classpath。
lang: zh
status: complete
updated: 2026-11-27
---

# ADR 0003 · MCLHR mod store 与硬链接投影

## 状态

已接受（2026-11-17）

## 背景

NSUM 的构建产物是一棵完整的工作目录，模组以 jar 形式落在 `mods/`。这带来两个问题：

- 多个整合包、多个实例共用同一批模组，每个实例各存一份，磁盘占用随实例数线性增长；
- 启动器侧（HMCL + MCLHR）需要知道「这个实例实际加载哪些模组、来自哪个库坐标」，仅有一堆 jar 无法表达来源与复用关系。

MCLHR（上游独立项目，LGPL-2.1）的定位是「库坐标 → 多 store → classpath/启动计划」，并且**明确不把 mod 塞进 classpath**：Fabric 走 `fabric.addMods`、NeoForge 走 `fml.modFolders`，Forge 需要 `mc mods sync --materialize` 才能生效。因此 NSUM 需要把 jar 以 MCLHR 认识的形式存起来，再投影到 `mods/`。

## 决策

- **store 布局**：运行时在 `<runtime>/stores/mods/` 下按 `<stem>_<sha16>.jar` 存放模组内容（stem 为原文件名去扩展名，sha16 为内容 SHA-256 前 16 位），同一内容跨整合包复用。
- **硬链接投影**：`applyMclhrStore()` 在 `exec build --format hmcl` 成功后执行，把 `mods/**/*.jar` 收进 store，再用 `CreateHardLinkW` 从 store 链接回 `mods/`；跨卷无法硬链接时回退复制。
- **索引文件**：写 `<runtime>/.mclhr/mods.json`（`schema`、`generatedBy` 与 `sources[]`——声明 store 路径、`mode: link` 与优先级），供 MCLHR 与后续同步判断来源。
- **bridge 侧同步**：`NSumMclhr.afterSync` 在每次 Runtime 同步成功后调用 `java -jar <runtime>/tools/mclhr/mclhr.jar mc mods sync --instance <实例目录> --mode link --prune`；`mclhr.jar` 缺失时跳过并记日志，不阻断启动。
- **不接管启动**：NSUM 只提供内容与投影，游戏启动、库解析、classpath 生成仍由 MCLHR 与启动器负责。

## 备选方案

| 方案 | 优点 | 缺点 |
|---|---|---|
| 每个实例各存一份 jar | 无额外依赖、行为直观 | 磁盘重复，多实例场景迅速膨胀 |
| 把 mod 加进 classpath 启动 | 实现简单 | MCLHR 明确不支持；版本冲突与隔离问题无解 |
| 只写 MCLHR 索引，不落 store | 仓库/实例最干净 | 每次启动都要从仓库重新取文件，离线不可用 |
| store + 硬链接（本决策） | 省盘、来源可追溯、离线可用 | 依赖文件系统支持硬链接；跨卷需回退复制 |

## 后果

- 正面：模组内容在实例间共享，升级只改 store；`mods/` 仍是启动器认识的普通目录；来源（hash/文件名）可追溯。
- 负面：硬链接与普通文件在资源管理器里难以区分，用户手工改 `mods/` 里的文件会连带影响 store 内容；`prune` 会移除不再被引用的链接，需要确保同步顺序（先同步内容，再跑 MCLHR）。
- 影响范围：`NeoCLI`（构建后收 store）、运行时 `stores/`、bridge（`NSumMclhr`）、`nsum_installer`（内嵌 `tools/mclhr`），以及发布打包（**必须排除 `stores/`**，否则包体会失控）。

## 相关

- [MCLHR 扩展说明](../../extensions/mclhr.md)
- [NeoCLI 模块](../Modules/NeoCLI/README.md)
- [发布流程](../release.md)
- [ADR 0006 · 清单型 .nsumpack](./0006-nsumpack-v2.md)
