---
title: PowerHelper
description: NSUM 内置的 Markdown 文档阅读器：WebView2 渲染、CLI 与回退方案。
lang: zh
status: complete
updated: 2026-11-27
---

# PowerHelper

PowerHelper 是文档阅读器：主程序完成页的「打开帮助文档」与编辑器「帮助 → 帮助文档」都通过它拉起 `exe/docs`。

## 组成

| 组件 | 作用 |
|---|---|
| PowerHelperCore（DLL） | Markdown 渲染、目录、文档组扫描 |
| PowerHelperBridgeLib（STATIC） | 集成库：`launchReader(path, extraArgs)` |
| PowerHelper.exe | GUI 阅读器外壳 |

## 渲染

- 首选系统 WebView2（`webview2` vcpkg 包）；
- 缺失 WebView2 运行时自动回退 `QTextBrowser`；
- 支持锚点跳转（`--anchor`）与目录（TOC）。

## CLI

```powershell
PowerHelper.exe <文档路径> [--anchor <id>]
PowerHelper.exe render <md>   # 渲染为 HTML/文本（见 --help）
PowerHelper.exe toc <md>
```

## 启动顺序

`launchReader(target, extraArgs)` 会把 `target` 放在 argv[1]（参数顺序错误会导致把 `--anchor` 当文件而退出 2）。

## 注意

- GUI 子系统 exe 不设置 `$LASTEXITCODE`，脚本验证要用 `Start-Process -Wait`；
- 文档路径必须在 `exe/docs` 下（发布包结构）；
- 中英双语文档由语言目录选择（`docs/zh` / `docs/en`）。

## 相关

- [基础库总览](./index.md)
- [模块参考](../../developer/Modules/PowerHelper/README.md)
