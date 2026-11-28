---
title: 版本说明
description: NSUM 各版本的主要变化与发布链接。
lang: zh
status: complete
updated: 2026-11-27
---

# 版本说明

完整发布记录见 [GitHub Releases](https://github.com/HiBer2007/NeoServerUpdateModpack/releases)。

## 1.0.5（2026-11-27）

- 编辑器 CLI：`info` / `exec` / `help`，含内置 Git 命令，供 AI/脚本操作仓库；
- `.nsumpack` 改为清单型 `pack_format 2`：默认只含 `nsum.index.json`，附带文件需 `--embed-patterns` 显式声明并登记来源提交；
- 配套 bridge `0.4.4.17`：按 `embedded[]` 安装并以 SHA-256 缓存；
- 文档体系重构（四板块、中英双语、Nextra-ready）。

## 1.0.4（2026-11-26）

- 运行时与 HIC 的依赖下载支持多线程 Range（失败回退单流）；
- bridge 并行下载器。

## 1.0.3

- 修复中文路径导致的崩溃（UTF-8 argv/路径全链路）；
- 内置 Git 定位与自动更新完善。

## 更早版本

见 GitHub Releases 页面与对应 tag。

## 相关

- [更新](../user/updates.md)
- [发布流程](../developer/release.md)
