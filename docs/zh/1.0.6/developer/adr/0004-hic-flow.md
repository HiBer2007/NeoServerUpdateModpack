---
title: ADR 0004 · HIC 声明式安装流程
description: 用 JSON 流程描述安装步骤，取代与 GUI 硬编码耦合的安装器，使同一流程可被向导、无人值守与离线外部下载复用。
lang: zh
status: complete
updated: 2026-11-27
---

# ADR 0004 · HIC 声明式安装流程

## 状态

已接受（2026-08-31 引入 HiBerCommonInstaller；2026-11-10 补齐内置 Git 全链）

## 背景

旧安装器把「页面 + 安装动作」写死在 GUI 代码里，带来三个现实问题：

- 无法无人值守：bridge 需要在 HMCL 里静默安装 Runtime，旧安装器只能模拟点击；
- 无法离线/受限网络部署：下载失败即整流程失败，没有「列出所需文件、人工放入后重跑」的通道；
- 无法复用：安装、卸载、修复、升级流程互相复制，Git 的下载/复用/兜底逻辑散落在各处。

安装过程本身是线性的、可声明的：收集参数 → 准备 Git → 清理目标 → 解包 payload → 写配置 → 建快捷方式。因此适合用步骤机描述，而不是用代码编排。

## 决策

- **流程即数据**：`install.json` / `uninstall.json` / `repair.json` / `upgrade.json` 由 `hci_flow` 解释执行；步骤分两类：`ui`（`language`/`welcome`/`license`/`path`/`components`/`git`/`elevate`/`finish`）与 `type`（`git_plan`、`download`、`extract`、`git_finalize`、`git_refresh`、`run`、`clean`、`template`、`shortcut`）。
- **条件执行**：`when` 用 Lua 表达式（不等号为 `~=`，带点键名写 `vars['components.editor']`），变量由上下文插值 `${var}` 提供。
- **可选步骤**：`optional: true` 的步骤失败只记 warning、流程继续（Git 下载/解压/收尾均如此），避免受限网络直接装不上。
- **超时控制**：`timeoutSeconds` 下发到下载后端，Git 下载限制 150 秒，防止每个后端各等数分钟。
- **内置 Git，工具目录保留**：`git_plan` 先探测已有 `tools/git`（四种布局 + `--version` 验证），命中则跳过下载；`cleanTarget` 用 `skip:["tools"]` 保证重装不删 Git；`git_finalize` 定位并写入 `git_path`，缺失时回退系统 Git 并写 `use_system_git=true`。
- **单向流程与 payload**：`hci_gui` 的 qrc 内嵌 `HCI_PRODUCT_FILES`（`product.json` + 四个流程 + `LICENSE.txt` + 整个 `deploy/`），因此 configure 之后 `deploy/` 内容变化必须重新 configure。
- **无人值守**：`hci_cli --cli --json --silent --product product.json --flow install.json --path <目录>`，stdout 只输出 `progress`/`message`/`finish` 事件块；bridge 逐行解析驱动自己的进度面板。CLI 分发使用 `install_cli.json`（无 Git 步骤、快捷方式 `enabled:false`），确保不创建桌面/开始菜单项。
- **外部下载模式**：`--external-download <目录>` 只列出缺失文件并输出 `downloads.json`，返回码 10；文件放入 `<目录>/files` 后重跑即可完成。该暂停优先于步骤的 `optional` 语义。

## 备选方案

| 方案 | 优点 | 缺点 |
|---|---|---|
| 继续在 GUI 代码里写流程 | 无需新框架 | 无法无人值守/离线，逻辑重复，难测试 |
| 每步一个外部脚本 | 灵活 | 变量、回滚、进度、错误码各写一套，难维护 |
| 只用 CLI 参数驱动安装 | 简单 | 无法表达条件分支、可选步骤与人机混合页面 |
| JSON 流程 + 步骤机（本决策） | 同一份流程三种壳复用，可测试、可离线 | 需要维护流程 schema 与 Lua 条件语义 |

## 后果

- 正面：安装/卸载/修复/升级共享机制；bridge 可静默安装并回显进度；离线环境有确定的人工介入点；Git 复用让增量安装降到秒级。
- 负面：流程 JSON、payload 清单与 configure 时机强耦合（部署目录变了要重新 configure）；`when` 的条件写法（Lua 语法、`vars['a.b']`）容易写错且错误信息不直观。
- 影响范围：`modules/HiBerCommonInstaller`、`nsum_installer/*.json`、根 `CMakeLists.txt` 的 `INSTALLER_ONLY_BUILD` 分支、bridge 的 Runtime 安装、发布打包。

## 相关

- [安装流程](../../extensions/installer/flow.md)
- [安装器 CLI](../../extensions/installer/cli.md)
- [外部下载模式](../../extensions/installer/external-download.md)
- [发布流程](../release.md)
- [ADR 0005 · 启动流程注入](./0005-bridge-injection.md)
