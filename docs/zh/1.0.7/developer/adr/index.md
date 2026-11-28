---
title: 设计决策（ADR）
description: NSUM 架构决策记录（ADR）索引：编号规则、状态含义、模板结构，以及 10 篇已接受决策的速览。
lang: zh
status: complete
updated: 2026-11-27
tags: [adr, architecture, decisions]
---

# 设计决策（ADR）

本页是 NSUM 架构决策记录（Architecture Decision Record）的入口：记录**已经发生**的关键技术决策、当时的约束与代价，供后来者判断「为什么是这样」以及「什么时候可以推翻」。

## 什么情况写 ADR

- 决策影响多个模块或长期维护成本（同步模型、包格式、下载链、注入点、模块边界）。
- 存在多个可行方案，且当时的选择有明确权衡，未来可能被重新讨论。
- 决策伴随不可逆的数据格式或对外契约（`.nsumpack`、`workspace.json`、CLI JSON、元数据字段）。

以下内容**不写** ADR：一次性缺陷修复、界面文案调整、依赖小版本升级、纯重构（除非改变模块边界）。

## 编号与状态

| 项 | 约定 |
|---|---|
| 编号 | 四位递增（`0001`…），一经分配不复用 |
| 文件名 | `<编号>-<kebab-case 英文短名>.md` |
| 状态 | `已接受（日期）` / `已废弃` / `已被 ADR-XXXX 取代` |
| 日期 | 决策真正落地或被明确裁定的日期，不是文档撰写日期 |
| 修订 | 决策改变时**新增** ADR 并在旧篇标注「已被取代」，不改写历史结论 |

## 结构

每篇固定五段：状态 → 背景（问题与约束）→ 决策（做了什么、不做什么）→ 备选方案（优点/缺点）→ 后果（正面/负面/影响范围），末尾给相关文档。允许内嵌 Mermaid 图说明数据流，但不引用不存在的图片。

## 决策速览

| 编号 | 标题 | 状态 | 一句话 |
|---|---|---|---|
| [ADR 0001](./0001-pointer-cache.md) | 指针文件与内容寻址缓存 | 已接受 | 仓库只存 SHA-256 指针，内容按哈希缓存并零信任校验 |
| [ADR 0002](./0002-l1-l2-l3-sync.md) | 三层同步体系（L1/L2/L3） | 已接受 | 文件夹策略、文件特化、存档 serverconfig 分层合并 |
| [ADR 0003](./0003-mclhr-store.md) | MCLHR mod store 与硬链接投影 | 已接受 | jar 收进 store，硬链接回 `mods/`，不塞 classpath |
| [ADR 0004](./0004-hic-flow.md) | HIC 声明式安装流程 | 已接受 | 安装步骤用 JSON 流程描述，支持可选步骤与无人值守 |
| [ADR 0005](./0005-bridge-injection.md) | 启动流程注入（bridge） | 已接受 | Mixin 包裹 `checkGameState`，同步成为启动第一步 |
| [ADR 0006](./0006-nsumpack-v2.md) | 清单型 `.nsumpack`（pack_format 2） | 已接受 | 默认只发清单，附带文件需显式正则声明 |
| [ADR 0007](./0007-parallel-download.md) | HTTP Range 并行下载 | 已接受 | 分片并发 + 失败即回退单流 + 强制时限 |
| [ADR 0008](./0008-editor-cli.md) | 编辑器无头 CLI | 已接受 | 单 JSON stdout、限定仓库内、git 操作有安全开关 |
| [ADR 0009](./0009-bilingual-docs.md) | 中英双语文档体系 | 已接受 | 中文 canonical、英文镜像、门禁与分档打包 |
| [ADR 0010](./0010-module-boundaries.md) | 模块边界与独立仓库 | 已接受 | 分层库 + 插件 DLL + 可复用库拆为 submodule |

## 与代码的关系

ADR 描述「为什么」，具体 API、字段与命令以代码和模块文档为准：

- 架构原理见 [架构概览](../architecture/overview.md)；
- 模块 API 见 [模块参考](../Modules/NeoCore/README.md)；
- 流程与命令见 [测试与验证](../testing.md)、[发布](../release.md)。

发现代码与 ADR 冲突时：以代码为准，若冲突意味着决策已改变，新增一篇 ADR 说明新决策与迁移方式。

## 相关文档

- [贡献指南](../contributing.md)
- [版本管理](../versioning.md)
- [架构概览](../architecture/overview.md)
- [数据模型](../architecture/data-model.md)
