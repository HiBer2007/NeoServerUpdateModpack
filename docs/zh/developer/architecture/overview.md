---
title: 总体架构
description: NSUM 组件、进程边界与数据流：Runtime、CLI/GUI、bridge、安装器、编辑器、MCDR 与插件。
lang: zh
status: complete
updated: 2026-11-27
---

# 总体架构

NSUM 由多个进程/组件组成，围绕一个 Git 仓库工作。

## 组件

| 组件 | 形态 | 职责 |
|---|---|---|
| NSUM Runtime | `NeoServerUpdateModpack.exe` + `deploy/` | 构建、同步、导出、下载、CLI |
| GUI | 同 EXE（无参数） | 向导/构建页 |
| CLI | 同 EXE（`info/flow/exec`） | 无界面操作与 JSON |
| nsum-bridge | HMCL 扩展 jar | NSUM 页面、启动注入、Runtime 安装 |
| HIC 安装器 | 独立 EXE | 安装 Runtime/Editor、内置 Git |
| NeoWorkspaceEditor | 独立 EXE | 仓库内容编辑器 + CLI |
| MCDR 插件 | Python 插件 | 服务端核心安装/同步 |
| 插件 DLL | `parsers/pointers/exporters/editor/extension` | 解析、下载、导出、编辑扩展 |

## 进程与数据流

```mermaid
flowchart TB
    subgraph Client
        HMCL[HMCL + FLFHMCL] --> Bridge[nsum-bridge]
        Bridge --> Runtime[NSUM Runtime]
        Runtime --> Plugins[(插件 DLL)]
        Runtime --> Instance[实例目录]
        Editor[NeoWorkspaceEditor] --> Repo[(Git 仓库)]
        Editor --> Plugins2[(编辑器扩展)]
    end
    subgraph Server
        MCDR[MCDR 插件] --> RuntimeS[Runtime]
        RuntimeS --> ServerDir[服务器目录]
    end
    Repo --> Runtime
    Repo --> RuntimeS
    Metadata[metadata 站点] --> Bridge
    Metadata --> HIC[HIC 安装器]
    HIC --> Runtime
```

## 模块分层

```mermaid
flowchart LR
    NeoCore --> CommonLogger[CommonLoggerCPP]
    NeoWorkspace --> NeoCore
    NeoBuild --> NeoWorkspace
    NeoBuild --> NeoCore
    NeoCLI --> NeoBuild
    GUIWorker --> NeoBuild
    Editor[NeoWorkspaceEditor] --> NeoWorkspace
    Editor --> GUIWorker
    Plugins --> NeoCore
    HIC --> NeoCore
```

- **NeoCore**：接口（IConfigParser/IPluginPointer/IModpackExporter）、PluginLoader、路径、并行下载器；
- **NeoWorkspace**：workspace.json、分支、Git 操作、同步策略；
- **NeoBuild**：构建引擎、分支合并、指针下载、serverconfig、导出器加载；
- **NeoCLI/GUIWorker**：CLI 与 GUI 的领域层；
- **插件**：以 DLL 形式在运行时加载。

## 数据目录

| 目录 | 内容 |
|---|---|
| `.hmcl/nsum/runtime/` | Runtime |
| `.hmcl/nsum/repos/` | 克隆的仓库 |
| `.hmcl/nsum/cache/files/` | 内嵌文件缓存（SHA-256） |
| `.minecraft/versions/.cache/` | 构建侧指针缓存（SHA-256） |
| `config/history/` | GUI 历史与远程缓存 |

## 关键约束

- Runtime 不依赖系统 Git（内置 MinGit）；
- 插件与宿主通过导出符号交互，跨 DLL 传递 STL/Qt 对象有严格约束（见 [ABI](../framework/abi.md)）；
- 所有路径以 UTF-8 处理（Windows argv 重建）；
- 同步失败默认中止启动。

## 相关

- [设计目标](./design-goals.md)
- [数据模型](./data-model.md)
