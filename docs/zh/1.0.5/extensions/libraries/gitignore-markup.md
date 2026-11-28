---
title: GitIgnoreMarkup
description: .gitignore 的标记/逆向标记系统与编辑 GUI。
lang: zh
status: complete
updated: 2026-11-27
---

# GitIgnoreMarkup

GitIgnoreMarkup 管理 `.gitignore` 中的**区块标记**，让工具可以安全地增删自己负责的规则而不破坏用户手写内容。

## 概念

| 概念 | 说明 |
|---|---|
| 标记区 | 由开始/结束注释包围的一段规则 |
| 标记名 | 区块标识（如 `nsum-build`） |
| 逆向标记 | 从现有 `.gitignore` 反推区块归属 |

## 使用

- 编辑器「扩展」或仓库设置中打开忽略规则编辑；
- 选择区块 → 增删规则；
- 保存时只重写标记区，其他行保持原样。

## 规则

- 标记区外的内容永不修改；
- 同名标记区重复时保留最后一个并警告；
- 规则去重与排序仅在标记区内进行。

## 注意

- 手工删除标记注释会让该区块变为普通规则（不再被工具管理）；
- 换行风格（CRLF/LF）在写回时统一；
- 该库仅依赖 Qt Core + 编辑 GUI 组件，可独立发布。

## 相关

- [基础库总览](./index.md)
- [模块参考](../../developer/Modules/GitIgnoreMarkup/README.md)
