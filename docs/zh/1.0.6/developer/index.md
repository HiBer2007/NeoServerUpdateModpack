---
title: 开发者文档
description: NSUM 的总体架构、设计目标、基础框架、总线与拓展接口、ABI、模块参考与流程规范。
lang: zh
status: complete
updated: 2026-11-27
---

# 开发者文档

本板块面向 **NSUM 核心贡献者、插件作者与集成方**。阅读顺序建议：

1. [架构与原理](./architecture/index.md)：总体组织、设计目标、数据模型、构建/同步/指针/下载/安全；
2. [基础框架 / 总线 / ABI](./framework/index.md)：NeoCore 基础框架、模块布局、HIC 事件总线、扩展注册表、插件契约、跨 DLL ABI；
3. [模块参考](./Modules/_meta.json)：34 个模块的 README（定位/边界/依赖/符号）与 usage（API/示例/陷阱）；
4. [测试](./testing.md)、[贡献指南](./contributing.md)、[版本管理](./versioning.md)、[发布](./release.md)；
5. [设计决策（ADR）](./adr/index.md)：关键取舍的历史与理由。

## 代码地图

| 层 | 模块 | 产物 |
|---|---|---|
| 基础 | `NeoCore`、`CommonLoggerCPP` | STATIC |
| 引擎 | `NeoWorkspace`、`NeoBuild` | STATIC |
| 接口 | `NeoCLI`、`GUIWorker`、`NeoWorkspaceEditor` | STATIC / EXE |
| 插件 | `NeoParser_*`、`NeoPointer_*`、`NeoExporter_*`、`NeoEditorExtension_*` | DLL |
| 工具链 | `HiBerCommonInstaller`、`CrashTracker`、`PowerHelper`、`HiBerGUILibrary`、`GitIgnoreMarkup` | 库 / EXE |

## 构建与验证

```powershell
cmake --preset msvc
cmake --build build --config Debug --target neo_deploy

pwsh -NoProfile -File tools/docs_check.ps1
```

完整命令与环境要求见 [testing.md](./testing.md) 与 AGENTS.md。
