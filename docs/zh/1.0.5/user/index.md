---
title: 用户指南
description: NSUM 主程序（NSUM Runtime）的安装、导入整合包、同步与启动、更新与排障。
lang: zh
status: complete
updated: 2026-11-27
---

# 用户指南

本板块面向**玩家与服务器成员**：你只需要安装 NSUM Runtime、把服务器提供的整合包导入 HMCL，之后每次启动都会自动同步服务器内容。

## 你会用到的三样东西

| 组件 | 作用 | 获取方式 |
|---|---|---|
| HMCL + FLFHMCL | 启动器与 Fabric 运行时注入 | 见 FLFHMCL 文档（待补） |
| nsum-bridge | HMCL 的 NSUM 整合包类型、启动方式与 NSUM 页面 | 随服务器提供或 metadata 站点 |
| NSUM Runtime | 负责仓库同步、构建与启动前检查的 CLI/运行时 | 由 bridge 自动下载安装，或用安装器 |

## 推荐阅读顺序

1. [安装](./installation.md)：装好 Runtime 与 bridge；
2. [快速开始](./quick-start.md)：导入整合包并进入游戏；
3. [主程序界面](./launcher-guide.md)：向导、构建页与常用操作；
4. [同步与启动](./sync-and-launch.md)：理解每次启动发生了什么；
5. [更新](./updates.md)、[常见问题](./faq.md)、[故障排查](./troubleshooting.md)。

## 重要概念（一句话版）

- **仓库**：服务器维护的 Git 仓库，存的是整合包的“配方”；
- **整合包分支**：仓库里的 `base`、`client` 等分支，决定你拿到哪些文件；
- **同步**：Runtime 按仓库内容更新你的游戏实例；
- **`.nsumpack`**：服务器发给你的整合包文件，默认只含仓库信息与文件清单，体积极小。
