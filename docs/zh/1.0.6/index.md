---
title: NSUM 文档
description: NSUM（NeoServerUpdateModpack）官方文档：主程序使用、整合包制作、开发与拓展参考。
lang: zh
status: complete
updated: 2026-11-27
---

# NSUM 文档

NSUM（NeoServerUpdateModpack）是 Minecraft 服务器整合包的**仓库化构建与自动更新**工具链：
服务端维护 Git 仓库，客户端通过 HMCL + nsum-bridge + NSUM Runtime 同步并启动。

## 从这里开始

| 你是谁 | 去哪里 |
|---|---|
| 玩家 / 服务器成员 | [用户指南](./user/index.md)：安装、导入整合包、同步与启动 |
| 整合包作者 | [整合包作者](./pack-author/index.md)：仓库结构、Editor、构建导出与 `.nsumpack` |
| 开发者 / 贡献者 | [开发者](./developer/index.md)：总体架构、设计目标、总线与拓展接口、ABI、模块参考 |
| 想找某个拓展 | [拓展功能](./extensions/index.md)：解析器、指针、导出器、安装器、bridge、MCDR、基础库 |

> 文档提供中文与英文两个版本，右上角语言切换；英文页与中文页一一对应。

## 版本

本文档对应 **NSUM Runtime 1.0.5 / 内部 0.1.11**。页面内标注了适用版本的例外情况。

## 相关链接

- 主仓库：[HiBer2007/NeoServerUpdateModpack](https://github.com/HiBer2007/NeoServerUpdateModpack)
- bridge 仓库：[HiBer2007/nsum-bridge](https://github.com/HiBer2007/nsum-bridge)
- 元数据站点：[metadata.hibernet.top](https://metadata.hibernet.top)
