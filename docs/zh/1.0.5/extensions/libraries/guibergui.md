---
title: HiBerGUILibrary
description: 通用 Qt 组件库：树面板、Git 面板、进度卡片、Toast、文件编辑器等。
lang: zh
status: complete
updated: 2026-11-27
---

# HiBerGUILibrary

`HiBerGUILibrary`（namespace `HiBerGUI`）是零业务依赖的 Qt 组件库，被 NSUM 编辑器与主程序复用。

## 组件

| 组件 | 作用 |
|---|---|
| RepoTreePanel / OutputTreePanel | 仓库树/输出树（配合 DeepTreeBehavior） |
| GitPanel | Git 状态/暂存/提交/历史（异步） |
| AnimatedProgress | 动画进度条（紧凑模式） |
| ProgressCard | 加载/导入进度卡片 |
| ToastNotification | 轻提示 |
| FileContentEditor | 文本/配置编辑 |
| DeepTreeBehavior | 树展开/折叠行为（三角/双击） |

## DeepTreeBehavior

- viewport 事件过滤器，无 QTimer；
- 折叠三角点击 → 展开一层 / 深折叠；
- 双击目录由行为统一处理（面板不得再连 `itemDoubleClicked`）；
- 动画由树的 `setAnimated(true)` 提供。

## 使用

```cpp
#include <repo_tree_panel.h>
#include <deep_tree_behavior.h>

auto* tree = new HiBerGUI::RepoTreePanel(parent);
new HiBerGUI::DeepTreeBehavior(tree, tree);
```

## 注意

- GitPanel 需要宿主注入 Git 路径：`setGitPath(GetDefaultGitPath())`；
- 组件不得反向依赖 NSUM 业务类型；
- 改共享头文件的成员后，所有引用 target 必须重建（布局失配会崩溃）。

## 相关

- [基础库总览](./index.md)
- [模块参考](../../developer/Modules/HiBerGUILibrary/README.md)
