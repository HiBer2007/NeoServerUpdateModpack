---
title: ADR 0010 · 模块边界与独立仓库
description: 按「核心库 → 工作区 → 构建 → 前端」分层，插件走 DLL 契约，可复用库拆为独立仓库并以 submodule 引用。
lang: zh
status: complete
updated: 2026-11-27
---

# ADR 0010 · 模块边界与独立仓库

## 状态

已接受（2026-08-30 模块文档体系与独立仓库化落地；此后新增模块沿用同一规则）

## 背景

项目从单一可执行文件逐步长出构建引擎、GUI 向导、编辑器、安装器、崩溃分析工具，早期把所有实现堆在同一层，导致：

- GUI 与业务逻辑互相引用，写 CLI/测试时被迫拉起 Qt Widgets；
- 通用组件（进度条、树、代码编辑器、Git 面板）与领域页面混在一起，无法复用；
- 插件契约不稳定，日志、接口、meta 字段凭记忆实现，跨 DLL 调用经常空指针或找不到导出符号；
- 崩溃分析器、日志库、GUI 组件库这类「可独立发布」的东西被主工程目录结构绑死，别的项目没法直接引用。

## 决策

- **分层依赖单向**：`NeoCore`（接口、插件加载、取消令牌、日志、崩溃捕获）→ `NeoWorkspace`（`workspace.json`、Git、同步、扫描、历史）→ `NeoBuild`（构建引擎、分支合并、导出、serverconfig、指针下载）→ `NeoCLI` / `GUIWorker` / 可执行文件。下层不得反向依赖上层。
- **GUI 再分层**：`HiBerGUILibrary` 只依赖 Qt + nlohmann，放通用组件且**零领域依赖**（不得链接 `GitIgnoreMarkup`）；依赖 WebView2 的组件归 `HiBerGUIWebView2`；向导、页面、领域编辑器留在 `GUIWorker`。
- **插件 = DLL + meta + 导出符号**：解析器、指针解析器、导出器、编辑器扩展各自实现接口，导出 `extern "C" CreateXxx()`（必须 `__declspec(dllexport)`）与日志注册符号 `NEO_DECLARE_PLUGIN_LOG_SINK`；宿主只按 `meta.json` 能力与 `GetProcAddress` 查找，不硬编码类型。
- **可复用库独立成仓**：`CommonLoggerCPP`、`HiBerGUILibCPP`、`GitIgnoreMarkup`、`PowerHelper`、`CrashTracker`、`CrashTrackerHandleLib`、`HiBerCommonInstaller`、`nbtcpp` 以 submodule 引用；主仓库只提交 gitlink，源码改动在各自仓库提交推送。
- **依赖方向与 CMake 顺序显式化**：`NeoCore` PUBLIC 链接 `PowerHelperBridgeLib`（桥接库并入 PowerHelper 仓库）；根 `CMakeLists.txt` 中 `CrashTrackerHandleLib`、`PowerHelper` 必须先于 `NeoCore`，`HiBerGUILibCPP` 必须先于 `GitIgnoreMarkup`。
- **模块文档双份制**：每个模块提供 `README.md`（边界/依赖/构建/公共符号概览）与 `usage.md`（API 逐字摘自头文件的用法），并登记在模块索引。

## 备选方案

| 方案 | 优点 | 缺点 |
|---|---|---|
| 单仓单层、全部静态链接进一个 EXE | 构建简单 | 无法复用、无法单独发布、GUI 与逻辑强耦合 |
| 所有模块都做成 DLL | 边界最硬 | 部署复杂，ABI 与 CRT 问题多，插件之外的模块无收益 |
| 可复用库直接复制进各项目 | 无 submodule 依赖 | 多份副本漂移，修一个 bug 要改多处 |
| 分层 + 插件 + submodule（本决策） | 复用、可测试、契约明确 | 改动需要跨仓提交与 gitlink 更新，构建顺序有约束 |

## 后果

- 正面：CLI/测试不需要 GUI；通用组件可独立发布（LGPL-2.1）；插件新增类型零侵入；模块文档有明确落点。
- 负面：跨仓改动流程长（先推独立仓库，再更新 gitlink）；共享头文件的类成员变更会让旧 obj 按旧 `sizeof` 分配，必须全量重建并冒烟，否则关闭时 `0xC0000374`；CMake `add_subdirectory` 顺序不能随意调整。
- 影响范围：全部 `modules/`、根 `CMakeLists.txt`、`.gitmodules`、`docs/zh/developer/Modules/**`、发布打包与克隆说明（`--recursive`）。

## 相关

- [架构概览](../architecture/overview.md)
- [模块参考](../Modules/NeoCore/README.md)
- [NeoBuild 模块](../Modules/NeoBuild/README.md)
- [贡献指南](../contributing.md)（submodule 与全量重建规则）
- [ADR 0001 · 指针文件与内容寻址缓存](./0001-pointer-cache.md)
