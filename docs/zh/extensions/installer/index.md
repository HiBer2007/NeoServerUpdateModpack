---
title: 安装器
description: HiBerCommonInstaller（HIC）安装框架：CLI、流程脚本、拓展与外部下载模式。
lang: zh
status: complete
updated: 2026-11-27
---

# 安装器（HIC）

NSUM 的图形安装器与 `hci_cli` 由 **HiBerCommonInstaller** 框架驱动：安装流程用 JSON 描述，步骤由核心/拓展实现，可被 GUI、TUI、CLI 三种壳复用。

## 组成

| 组件 | 作用 |
|---|---|
| `hci_core` | 零 Qt 核心：变量、流程执行、下载、解压、注册表 |
| `hci_flow` | JSON 流程 + Lua 条件（`when`） |
| `hci_ext` | 拓展宿主（git/winget/apt…） |
| `hci_gui` / `hci_tui` / `hci_cli` | 三种外壳 |

## 页面

- [CLI](./cli.md)：`hci_cli` 参数与退出码；
- [流程脚本](./flow.md)：`install.json` 步骤类型、变量与条件；
- [拓展](./extensions.md)：内置拓展与加载；
- [外部下载模式](./external-download.md)：`--external-download` 与 exit 10 契约。

## NSUM 安装流程

```mermaid
flowchart LR
    A[欢迎] --> B[许可]
    B --> C[选择目录]
    C --> D[可选 Editor]
    D --> E[下载/解压 deploy]
    E --> F[内置 Git 规划]
    F --> G[写 install.conf]
    G --> H[快捷方式/完成]
```

## 目录

| 路径 | 内容 |
|---|---|
| `nsum_installer/*.json` | NSUM 的产品/安装/升级/修复/卸载流程 |
| `nsum_installer/product/*` | 产品页面资源 |
| `nsum_installer/ext/nsum_args_ext.cpp` | `--with-editor` 等参数拓展 |

## 相关

- [编辑器文档释放](../../pack-author/editor-gui.md)（安装器勾选 Editor 时释放板块 2）
