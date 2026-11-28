---
title: 服务端插件
description: NSUM 的 MCDR 插件：服务端核心安装、配置改写、仓库同步与启动。
lang: zh
status: complete
updated: 2026-11-27
---

# 服务端插件

NSUM 服务端由 MCDR 插件驱动（独立仓库），负责在服务器上安装核心、同步仓库内容并启动服务器。

<!-- slot:MCDR_PLUGIN_REPO -->

> **待补**：MCDR 插件仓库与发布页链接完成后在此补充。

## 能力

| 能力 | 说明 |
|---|---|
| 核心安装 | vanilla / plugin / modded / hybrid / proxy / geyser 分类 |
| 配置改写 | 按核心修改 MCDR `config.yml` 的 handler |
| 仓库同步 | 调用 Runtime 同步 `server` 分支 |
| 插件合并 | `plugin/<name>/` 合并进 `plugins/<name>/` |
| 内置 Git | 通过 `gitpath` 解析内置 Git 并前置 PATH |

## 核心分类

| 分类 | 示例 |
|---|---|
| vanilla | vanilla |
| plugin | paper / purpur / folia |
| modded | forge / neoforge / fabric / quilt |
| hybrid | mohist / arclight / cardbord |
| proxy | velocity / bungeecord / waterfall |
| geyser | geyser |

> 仅 `branchType=server` 的分支允许安装插件服核心。

## 流程

```mermaid
flowchart LR
    A[读取实例/分支配置] --> B[解析核心]
    B --> C[安装/更新核心]
    C --> D[同步 server 分支]
    D --> E[改写 config.yml]
    E --> F[启动服务器]
```

## 页面

- [安装](./install.md)：核心选择与安装；
- [同步](./sync.md)：服务端同步流程；
- [配置](./config.md)：config.yml 改写与插件目录。

## 相关

- [bridge](../client-bridge/index.md)
- [同步与启动](../../user/sync-and-launch.md)
