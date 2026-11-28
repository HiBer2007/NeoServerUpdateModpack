---
title: ADR 0005 · 启动流程注入（bridge）
description: 用 Mixin 包裹 HMCL 的 checkGameState 返回值，把 NSUM 同步插到启动任务链最前面，并保证可取消、可回滚、有进度。
lang: zh
status: complete
updated: 2026-11-27
---

# ADR 0005 · 启动流程注入（bridge）

## 状态

已接受（2026-11-08）

## 背景

NSUM 要求「玩家点启动 → 先同步服务器内容 → 再进游戏」。HMCL 的启动任务链是：

```text
checkGameState(...) → 处理游戏依赖 → 登录 → 等待游戏启动
```

只在「处理游戏依赖」阶段挂一个任务（例如通过 `createCompletionTask`）时机太晚：依赖已按旧版本解析，模组变更可能已经生效。另外还有三个必须满足的约束：

- 玩家能取消启动，取消后**不能**继续拉起游戏；
- 同步失败必须中止启动，而不是带着半同步的实例进游戏；
- 同步进度要显示在 HMCL 原生的任务对话框里，不另开窗口。

## 决策

- **注入点**：Mixin 包 `LauncherHelper.checkGameState(HMCLGameInstance, GameSettings.Effective, GameInstanceManifest)` 的返回值（`@At("RETURN")`、`cancellable = true`、`remap = false`），条件为实例目录含 `.NSUM/`。
- **任务链**：`new NSumSyncTask(...).withStage("NSUM 更新检查").thenComposeAsync(original)`；启用分支选择向导时先跑 `NSumBranchSelectTask`，再跑同步任务。同步完成后才把控制权交回原生链。
- **取消语义**：HMCL 取消会中断执行线程，任务侧读 `Task.isCancelled()`；Runtime 子进程的读取循环接收 `BooleanSupplier`，检测到取消即 `destroyForcibly()` 并抛 `InterruptedException`，防止取消后仍然 launch。
- **失败语义**：同步失败调用 `ui.fail()` 并抛异常，任务链中止、游戏不启动；仅当配置 `launchOnSyncFailure=true` 才允许放行。
- **进度 UI**：`TaskExecutorDialogPane` 是 BorderPane，center 是 VBox（标题 + TaskListPane）；把自定义面板插到 index 1 即成为「第一行」。面板用 `Platform.runLater` 挂载，并缓存最后一次 status（挂载前设置的状态否则会丢失，界面停在初始文案）。
- **图标**：实例目录已存在 `icon.*`（例如复制自模板实例）时旧的 `ensureIcon` 会直接返回，改为删除非 `icon.png` 的文件并始终覆盖。
- **分支选择复用同一对话框**：`NSumBranchWizard` 只构建面板，`NSumLaunchDialog.mountSelectionPanel()` 把面板挂进 `task-executor-dialog-layout` 的 VBox，任务线程等待 Future；不新开 Stage。

## 备选方案

| 方案 | 优点 | 缺点 |
|---|---|---|
| 在 `createCompletionTask` 挂任务 | 不改 HMCL 启动链 | 时机在依赖处理之后，可能已按旧内容解析 |
| 替换 HMCL 启动器/自建入口 | 完全可控 | 维护成本极高，失去 HMCL 生态能力 |
| 独立进度窗口 | 实现自由 | 与 HMCL 任务体验割裂，用户否决 |
| Mixin 包裹 checkGameState（本决策） | 真正「第一步」、复用原生任务对话框 | 依赖 HMCL 内部签名，升级可能失效 |

## 后果

- 正面：同步总在启动最前；进度、取消、失败都与 HMCL 原生任务一致；向导与同步共用同一注入点。
- 负面：Mixin 目标签名随 HMCL 版本变化，升级 HMCL 需回归该注入点；`show()` 后再 `showAndWait()` 会抛 `IllegalStateException`，对话框只能 `showAndWait`；用 `Start-Process -WindowStyle Hidden` 启动会让 JavaFX 窗口不可见，测试不能用。
- 影响范围：`nsum-bridge`（`MixinLauncherHelper`、`NSumSyncTask`、`NSumBranchSelectTask`、`NSumLaunchDialog`、`NSumInstanceVisuals`）、HMCL 版本兼容矩阵、bridge 自检的启动策略用例。

## 相关

- [客户端 Bridge 用法](../../extensions/client-bridge/usage.md)
- [任务与进度](../../extensions/client-bridge/tasks.md)
- [测试与验证](../testing.md)
- [ADR 0003 · MCLHR mod store](./0003-mclhr-store.md)
