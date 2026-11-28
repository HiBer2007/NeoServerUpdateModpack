---
title: ADR 0008 · 编辑器无头 CLI
description: 为 AI 与脚本提供无 QApplication 的编辑器命令行：单 JSON 输出、路径限定仓库内、git 操作带安全开关。
lang: zh
status: complete
updated: 2026-11-27
---

# ADR 0008 · 编辑器无头 CLI

## 状态

已接受（2026-11-27）

## 背景

整合包仓库的日常维护（建分支、写文件、转指针、提交推送）此前只能在 `NeoWorkspaceEditor` 的 GUI 里完成。让 AI 或脚本驱动时有两个选择：模拟鼠标点击，或者给编辑器加一个真正的命令行入口。

模拟点击不可行：需要显示环境、坐标随分辨率漂移、失败时拿不到结构化原因。另一个约束是编辑器的 GUI 逻辑与 `WorkspaceManager` 深度耦合，如果为了 CLI 启动 `QApplication`，无显示环境会直接失败。

## 决策

- **入口分流**：`main.cpp` 最前面判断 `info|exec|help`，命中即进入 `editor_cli`，**不创建 `QApplication`**；但保留 `QCoreApplication`（`InstallConfig::load` 需要 `applicationDirPath` 才能解析内置 Git）。
- **输出契约**：stdout 只输出**一个** JSON 对象——成功 `{ok, command, repo, data}`，失败 `{ok, command, error}`；日志必须走 stderr sink，否则 `WorkspaceManager` 的日志会混进 JSON。退出码：0 成功、1 运行失败、2 用法错误。
- **仓库定位**：`--repo` 缺省为当前目录，必须含 `workspace.json`；所有路径参数拒绝绝对路径与 `..`，只能落在仓库内。
- **命令集**：
  - `info`：`workspace`、`branches`、`branch <名>`、`tree`、`pointers`、`sync`、`git`、`file <相对路径>`；
  - `exec`：`validate`、`branch-add/set/rename/remove`、`file-write/delete/move`、`json-get/json-set`（JSON Pointer，支持数组与 `~0`/`~1`）、`pointer-add/remove`、`git-status/add/commit/push/pull/fetch/log/diff/checkout/reset/config`。
- **与 GUI 语义一致**：`branch-add` 同时维护 `branches/<name>/` 与 `branch_config/<name>.json`（复制默认四段 `file_manifest`/`pointer_files`/`serverconfig_sync`/`custom_mods`）；`pointer-add` 必须写 `branch_config/<sha>.pointer` 并更新 `file_manifest`/`pointer_files`（与 GUI 指针面板一致，而不是写 `branches/.../<name>.pointer`）。
- **校验**：`exec validate` 以 `WorkspaceManager` 加载结果为准，另加手写的重名、父缺失、父链环检测——这三类算 error；缺目录、缺 `branch_config` 只算 warning。
- **Git 安全**：QProcess 调用解析后的内置 Git（不依赖 PATH），环境设 `GIT_TERMINAL_PROMPT=0`、`GCM_INTERACTIVE=never` 防止交互挂起；推送不提供 `--force`（只有 `--force-with-lease`），hard reset 必须显式 `--confirm-hard`，commit 必须显式 `--message`；git 失败时 JSON 仍带 `data.exit_code/stdout/stderr` 且 `ok:false`，便于调用方读原因。
- **编码**：非 ASCII 路径与提交信息经 UTF-8 argv + `QString` 转换，已实测中文仓库可用。

## 备选方案

| 方案 | 优点 | 缺点 |
|---|---|---|
| GUI 自动化（点击/坐标） | 不改代码 | 需要显示环境，脆弱，无结构化错误 |
| 独立 CLI 可执行文件 | 进程隔离清晰 | 重复实现全部仓库逻辑，维护成本高 |
| 长期运行的 HTTP/MCP 服务 | 可交互、可流式 | 引入端口、鉴权、生命周期管理，超出当前需求 |
| 主程序内嵌无头子命令（本决策） | 复用 `WorkspaceManager`，零额外进程 | 与 GUI 逻辑需保持同步，入口分流要小心 |

## 后果

- 正面：AI/脚本可原子化操作仓库并拿到结构化结果；无显示环境可用；危险操作有显式开关；错误可诊断。
- 负面：命令语义与 GUI 两处维护，改 `branch_config` 结构时必须同时改 `editor_cli.cpp` 与指针面板；stdout 契约一旦破坏（例如误把日志打到 stdout）会直接让调用方解析失败。
- 影响范围：`modules/NeoWorkspaceEditor`（`editor_cli.*`、`main.cpp`）、`NeoWorkspace`（校验与分支模型）、文档 `pack-author/editor-cli`、自动化脚本。

## 相关

- [编辑器 CLI（打包者视角）](../../pack-author/editor-cli.md)
- [NeoWorkspaceEditor 模块](../Modules/NeoWorkspaceEditor/usage.md)
- [NeoWorkspace 模块](../Modules/NeoWorkspace/README.md)
- [测试与验证](../testing.md)
