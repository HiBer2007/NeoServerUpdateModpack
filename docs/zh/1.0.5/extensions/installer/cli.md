---
title: HIC CLI
description: hci_cli 的参数、退出码与 JSON 输出，用于无界面安装。
lang: zh
status: complete
updated: 2026-11-27
---

# HIC CLI

`hci_cli.exe` 是无界面安装壳：读取 `product.json` 与流程文件，按默认答案或指定参数执行安装。

## 参数

| 参数 | 说明 |
|---|---|
| `--gui` / `--tui` / `--cli` | 外壳模式（默认取产品 `defaultMode`） |
| `-s`, `--silent` | 非交互（使用默认答案） |
| `--path <dir>` | 安装目录覆盖 |
| `--lang en\|zh` | 语言 |
| `--product <file.json>` | 产品配置（默认 `./product.json`） |
| `--flow install\|uninstall\|path` | 流程（也接受流程文件） |
| `--json` | stdout 输出 JSON 协议块 |
| `--verbose` | 调试日志 |
| `--extensions <dir>` | 拓展目录 |
| `--download-threads <1-16>` | 每个下载的并行连接数 |
| `--external-download <dir>` | 只列出下载清单，不实际下载（见外部下载模式） |
| `--help` / `--version` | 帮助/版本 |

## 退出码

| 码 | 含义 |
|---|---|
| 0 | 成功 |
| 1 | 失败 |
| 2 | 用法错误 |
| 10 | 外部下载模式：需要外部准备文件 |

## 示例

```powershell
# 静默安装到指定目录
hci_cli.exe --silent --cli --path "C:\NSUM"

# 只准备下载清单（后续离线安装）
hci_cli.exe --external-download "C:\nsum-dl" --silent --cli

# JSON 协议
hci_cli.exe --json --silent --cli
```

## JSON 输出

- 进度与结果以 JSON 块输出（`--json`）；
- 外部下载模式额外输出 `hci/downloads-required` 块，列出缺失文件。

## 注意

- `--path` 会覆盖产品配置中的默认安装路径；
- 静默模式缺少必填答案时使用产品默认值；
- 失败时返回 1，并在 stderr/日志中给出原因。

## 相关

- [流程脚本](./flow.md)
- [外部下载模式](./external-download.md)
