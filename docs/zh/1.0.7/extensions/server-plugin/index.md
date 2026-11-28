---
title: 服务端插件
description: NSUM 的 MCDR 插件：服务端核心安装、配置改写、仓库同步与启动。
lang: zh
status: complete
updated: 2026-11-28
---

# 服务端插件

NSUM 服务端由 MCDR 插件驱动（独立仓库），负责在服务器上安装核心、同步仓库内容并启动服务器。

- 仓库：[HiBer2007/nsum-bridge-mcdr](https://github.com/HiBer2007/nsum-bridge-mcdr)
- 当前版本：**0.4.0**
- 需要 NSUM Runtime **>= 1.0.7**（旧 Runtime 仍可同步，但没有自动选优/超时回退）

## 能力

| 能力 | 说明 |
|---|---|
| 核心安装 | vanilla / plugin / modded / hybrid / proxy / geyser 分类 |
| 配置改写 | 按核心修改 MCDR `config.yml` 的 handler |
| 仓库同步 | 调用 Runtime 同步 `server` 分支 |
| 多地址选择 | `repo_preference` 透传 Runtime，自动最快 / 超时回退 / 手动指定 |
| 插件合并 | `plugin/<name>/` 合并进 `plugins/<name>/` |
| 内置 Git | 通过 `gitpath` 解析内置 Git 并前置 PATH |

## 插件配置

```json
{
  "repo": "git@github.com:you/your-server-repo.git",
  "repo_preference": "auto",
  "repo_timeout_seconds": 0
}
```

- `repo_preference`：`auto`（默认，最快）/ `ssh` / `https` / `http` / `manual`（只用 `repo`）；
- `repo_timeout_seconds`：每个地址的探测/克隆超时秒数，`0` 使用 Runtime 默认（探测 8s、克隆 120s）。

配置文件位于 MCDR 插件配置目录（`config/nsum_bridge/config.json`）；命令行同步也可用 `--repo-preference` / `--repo-timeout`。

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
