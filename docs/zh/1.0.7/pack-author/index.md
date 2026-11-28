---
title: 整合包作者
description: NSUM 仓库结构、同步规则、Editor 使用、构建导出与 .nsumpack 格式。
lang: zh
status: complete
updated: 2026-11-27
---

# 整合包作者

本板块面向**整合包作者与服务器维护者**：你要维护一个 NSUM 仓库，定义分支与同步规则，用 Editor 管理内容，最后构建并发布 `.nsumpack`。

## 核心概念

| 概念 | 说明 |
|---|---|
| 仓库 | 含 `workspace.json` 的 Git 仓库，是整合包的唯一事实来源 |
| 整合包分支 | `workspace.json` 的 `branches[]`，决定内容组合（`base`/`client`/`server`…） |
| Git 分支 | 仓库代码分支，构建时 checkout |
| 父分支 | 子分支继承父分支文件 |
| 指针文件 | 以 SHA-256 占位的大文件（模组），按需下载 |
| 同步层级 | L1 基础、L2 整合包分支、L3 存档 serverconfig |
| `.nsumpack` | 导出的清单型整合包（pack_format 3） |

## 工作流

```mermaid
flowchart LR
    A[创建/克隆仓库] --> B[workspace.json 定义分支与策略]
    B --> C[Editor 放入 mods/config/资源]
    C --> D[大文件转指针]
    D --> E[构建预览]
    E --> F[导出 .nsumpack]
    F --> G[玩家导入/同步]
```

## 阅读顺序

1. [仓库结构](./repo-structure.md)：目录、`workspace.json`、`branch_config`；
2. [同步规则](./sync-rules.md)：L1/L2/L3、`sync_policies`、serverconfig；
3. [Editor 界面](./editor-gui.md) 与 [Editor CLI](./editor-cli.md)；
4. [构建与导出](./build-and-export.md)；
5. [`.nsumpack` 格式](./nsumpack.md)；
6. [指针](./pointers.md)、[最佳实践](./best-practices.md)、[发布](./publish.md)。

## 快速开始（命令行）

```powershell
# 查看分支
NeoWorkspaceEditor.exe info branches --repo <仓库>

# 新增客户端分支
NeoWorkspaceEditor.exe exec branch-add --name client --parent base --type client --repo <仓库>

# 写入配置
NeoWorkspaceEditor.exe exec file-write --path branches/client/config/client.json --content "{}" --repo <仓库>

# 校验
NeoWorkspaceEditor.exe exec validate --repo <仓库>

# 构建并导出（默认不内嵌文件）
NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack client --git-branch main --format nsum --export client.nsumpack
```
