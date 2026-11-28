---
title: 命令行（CLI）
description: NSUM 运行时 CLI 的三类命令、JSON 输出与退出码，面向高级用户与自动化。
lang: zh
status: complete
updated: 2026-11-27
---

# 命令行（CLI）

> `--repo-preference auto|ssh|https|http|manual` 与 `--repo-timeout <秒>` 自 **1.0.7** 起提供：auto 并行探测最快地址，manual 只用给定 URL。

> `repo-remotes`、`git-ssh-key` 与多地址导出自 **1.0.6** 起提供。

`NeoServerUpdateModpack.exe` 不带 GUI 参数时进入 CLI 模式，分为三类互斥命令：

```text
info  只读查询（结构化输出）
flow  向导流程（GUI/console，复用 GUI 向导）
exec  执行操作（构建、导出、同步、工具）
```

## 基本用法

```powershell
NeoServerUpdateModpack.exe <category> <verb> [options]
```

全局选项：

| 选项 | 说明 |
|---|---|
| `--json` | 输出 JSON 块（`=====JSON-BEGIN=====` / `=====JSON-END=====`），人类日志走 stderr |
| `--verbose` | 详细日志 |
| `--silent` | 静默（仅错误） |

## info

```powershell
.\NeoServerUpdateModpack.exe info version
.\NeoServerUpdateModpack.exe info system
.\NeoServerUpdateModpack.exe info git
.\NeoServerUpdateModpack.exe info modpacks --repo <仓库> --json
.\NeoServerUpdateModpack.exe info status --repo <仓库> --json
.\NeoServerUpdateModpack.exe info preview --repo <仓库> --modpack base --format nsum --json
.\NeoServerUpdateModpack.exe info plugins --json
.\NeoServerUpdateModpack.exe info exporters --json
```

## flow

```powershell
# 复用 GUI 向导，收集参数后输出 JSON
.\NeoServerUpdateModpack.exe flow gui --from repo --to checklist --prefill repo=<仓库>
# 纯文本向导
.\NeoServerUpdateModpack.exe flow console
```

## exec

```powershell
# 构建并导出 .nsumpack（默认不内嵌文件）
.\NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack base --git-branch main --format nsum --export out.nsumpack

# 显式内嵌部分文件（正则）
.\NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack base --git-branch main --format nsum `
    --embed-patterns '^config/.*\.json$' --export out.nsumpack

# 仅导出（复用已有构建产物）
.\NeoServerUpdateModpack.exe exec export --export out.nsumpack --format nsum --repo <仓库> --modpack base

# 其他
.\NeoServerUpdateModpack.exe exec sync-serverconfig --save <存档> --repo <仓库>
.\NeoServerUpdateModpack.exe exec verify-repo --repo <仓库>
.\NeoServerUpdateModpack.exe exec resolve-pointer <file.pointer>
.\NeoServerUpdateModpack.exe exec git-update

# 维护仓库的多协议地址（写回 workspace.json 的 git.remotes）
.\NeoServerUpdateModpack.exe exec repo-remotes --repo <仓库> `
    --ssh git@github.com:owner/repo.git `
    --https https://github.com/owner/repo.git `
    --http http://github.com/owner/repo.git --primary ssh

# SSH 密钥管理（内置 Git 自动使用 GIT_SSH_COMMAND）
.\NeoServerUpdateModpack.exe exec git-ssh-key --list
.\NeoServerUpdateModpack.exe exec git-ssh-key --generate --comment nsum
.\NeoServerUpdateModpack.exe exec git-ssh-key --public-key
.\NeoServerUpdateModpack.exe exec git-ssh-key --test --host github.com
.\NeoServerUpdateModpack.exe exec git-ssh-key --configure --host github.com
```

GUI 中：主程序「选择仓库源」页输入框右侧 **SSH 密钥...**；编辑器 **Git → SSH 密钥...**。

## JSON 输出

```text
=====JSON-BEGIN=====
{"category":"info","command":"version","data":{...}}
=====JSON-END=====
```

- `category`：命令类别；
- `command`：具体动词；
- `data`：结构化结果（字段见各命令）。

## 退出码

| 码 | 含义 |
|---|---|
| 0 | 成功 |
| 1 | 操作失败（构建失败、校验失败、git 操作失败等） |
| 2 | 参数或用法错误 |

> 安装器 HIC 的 `hci_cli` 另有 exit 10（外部下载暂停），与 Runtime CLI 无关。

## 建议

- 自动化脚本优先使用 `--json`；
- 构建前先 `info preview` 确认文件结构；
- 导出 `.nsumpack` 时保持默认不内嵌，除非确实需要携带文件（见 [打包格式](../pack-author/nsumpack.md)）。
