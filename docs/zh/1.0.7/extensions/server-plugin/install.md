---
title: 服务端安装
description: MCDR 插件的核心选择、下载与安装（含 BuildTools/API 回退）。
lang: zh
status: complete
updated: 2026-11-27
---

# 服务端安装

## 前置

- 已安装 MCDR 与 Python 运行环境；
- 插件可访问内置 Git（`tools/git`，四种布局）或系统 Git；
- 服务器目录可写。

## 选择核心

在插件配置/命令中选择核心：

| 核心 | 安装方式 |
|---|---|
| Spigot | `BuildTools.jar --rev <mc> --compile spigot` |
| Pufferfish | Jenkins artifact `ci.pufferfish.host/job/Pufferfish-<mc>` |
| Mohist | `mohistmc.com/api/v2`（失败回退 GitHub） |
| Arclight / Cardboard | GitHub Releases 资产匹配 |
| Paper / Purpur / Folia | 官方 API/下载 |
| Fabric / Quilt / NeoForge | 安装器/官方 API |
| Velocity / BungeeCord | 官方下载 |
| Geyser | 官方下载 |

## 安装流程

1. 校验核心与 `branchType=server`；
2. 下载核心 jar；
3. 复制为 `server.jar`（或配置的启动 jar）；
4. 写/更新 MCDR `config.yml`；
5. 首次启动生成配置文件。

## BuildTools 注意

- 需要 JDK 21 与内置 Git（PATH 前置）；
- Windows 证书链问题：`-Djavax.net.ssl.trustStoreType=WINDOWS-ROOT`；
- 缓存 `<server>/.nsum/cache`（约 876 MB）勿提交到仓库；
- 编译约 5 分钟（视机器）。

## 失败处理

| 现象 | 原因 | 处理 |
|---|---|---|
| `unknown core` | 核心名拼写/版本不支持 | 检查核心列表 |
| BuildTools 卡住 | 网络/证书/Java 版本 | 配置 trustStoreType、检查 JDK |
| 下载 404 | 版本无对应资产 | 换核心版本或手动放入缓存 |

## 相关

- [服务端插件总览](./index.md)
- [配置](./config.md)
