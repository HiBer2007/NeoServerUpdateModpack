---
title: 安装器拓展
description: HIC 拓展宿主与内置拓展（git/winget/apt、NSUM 参数拓展）。
lang: zh
status: complete
updated: 2026-11-27
---

# 安装器拓展

HIC 的拓展是可插拔的步骤提供者：核心不认识的功能（Git、包管理器、产品特化参数）都由拓展注册。

## 内置拓展

| 拓展 | 提供 | 典型步骤/能力 |
|---|---|---|
| `ext/git`（hci.git） | 内置 Git 规划/安装/定位 | `git_plan`、`git_finalize`、`git_refresh` |
| `ext/winget`（hci.winget） | Windows 包管理器 | `winget install` 类步骤 |
| `ext/apt`（hci.apt） | Debian 包管理器 | `apt install` 类步骤 |
| `nsum_installer/ext/nsum_args_ext` | NSUM 参数 | `--with-editor` 等 |

## 加载

- 安装器通过 `--extensions <dir>` 指定目录；
- 拓展以 DLL 形式提供，注册步骤类型与命令；
- 未加载对应拓展时，流程中的类型会报 `unknown step type`。

## 编写拓展

1. 实现 HIC 拓展接口（`hci/extension.h`）；
2. 导出注册函数；
3. 提供 `*.json` 声明（名称/版本）；
4. 放入拓展目录并在流程中引用步骤类型。

## NSUM 参数拓展

`nsum_args_ext` 为安装器增加产品特化参数：

| 参数 | 作用 |
|---|---|
| `--with-editor` | 安装 Editor 并释放整合包作者文档 |

## 注意

- 拓展失败默认使流程失败；需要容错时在步骤上标 `optional`；
- 拓展目录必须在流程开始前可读；
- 版本不匹配的拓展可能注册失败，日志会给出原因。

## 相关

- [流程脚本](./flow.md)
- [CLI](./cli.md)
