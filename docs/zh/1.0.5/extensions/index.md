---
title: 拓展功能
description: NSUM 所有可插拔拓展的分组介绍：解析器、指针、导出器、编辑器、安装器、bridge、服务端、基础库与 MCLHR。
lang: zh
status: complete
updated: 2026-11-27
---

# 拓展功能

NSUM 的核心是“仓库 + 同步 + 启动”，其余能力以拓展形式提供。本板块按类型分组：

## 分组

| 分组 | 内容 | 运行位置 |
|---|---|---|
| [配置解析器](./parsers/index.md) | JSON/YAML/TOML/SNBT/TXT/Properties | Runtime 同步 |
| [指针解析器](./pointers/index.md) | DirectURL/Modrinth | Runtime 下载 |
| [导出器](./exporters/index.md) | MCBBS/Modrinth/HMCL/NSUM | 构建导出 |
| [编辑器扩展](./editor/index.md) | 配置/指针编辑扩展 | NeoWorkspaceEditor |
| [安装器](./installer/index.md) | HIC 壳与 git/winget/apt 拓展 | 安装器 |
| [客户端 bridge](./client-bridge/index.md) | HMCL 集成、启动注入、下载 | HMCL |
| [服务端插件](./server-plugin/index.md) | MCDR 核心安装/同步/配置 | 服务器 |
| [基础库](./libraries/index.md) | CrashTracker/PowerHelper/GUI 库/忽略标记 | 通用 |
| [MCLHR](./mclhr.md) | mod store 与硬链接投影 | Runtime + bridge |

## 选择指南

```mermaid
flowchart TD
    A[你要扩展什么?] --> B{同步时合并配置}
    B -->|是| C[写 IConfigParser 解析器]
    A --> D{下载方式}
    D -->|是| E[写 IPluginPointer 解析器]
    A --> F{新的分发格式}
    F -->|是| G[写 IModpackExporter 导出器]
    A --> H{编辑器能力}
    H -->|是| I[写编辑器扩展]
    A --> J{安装步骤}
    J -->|是| K[写 HIC 拓展]
```

## 加载目录

| 拓展 | 目录 |
|---|---|
| 解析器/指针/导出器 | `<runtime>/parsers`、`pointers`、`exporters` |
| 编辑器扩展 | `<runtime>/editor/extension/{parsers,pointers}` |
| HIC 拓展 | 安装器 `extensions` 目录 |
| bridge | HMCL `mods/` |
| MCDR 插件 | 服务器 `plugins/` |

## 相关

- [开发者：插件契约](../developer/framework/plugin-contracts.md)
- [开发者：模块参考](../developer/Modules/_meta.json)
