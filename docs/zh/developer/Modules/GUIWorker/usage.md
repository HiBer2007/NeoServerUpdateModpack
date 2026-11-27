---
title: GUIWorker 使用
description: 嵌入向导、构建进度与编辑器面板的 API 与注意事项。
lang: zh
status: complete
updated: 2026-11-27
---

# GUIWorker 使用

## 快速开始

```cpp
#include <wizard_window.h>

auto* wizard = new GUIWorker::WizardWindow(parent);
wizard->show();
```

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `WizardWindow` | `explicit WizardWindow(QWidget* parent)` | 向导 |
| `BuildPage::setProgress` | 见 `build_page.h` | 更新进度 |
| `ModpackContentIde` | `explicit ModpackContentIde(QWidget*)` | 内容 IDE |
| `PointerEditorPanel` | 见头文件 | 指针编辑 |
| `EditorExtensionRegistry::scan` | 见头文件 | 扫描编辑器扩展 |
| `BranchEditor` / `RepoEditor` | 见头文件 | 分支/仓库设置 |

## 典型用法

### 构建进度

```cpp
GUIWorker::GuiBuildProgress progress(buildPage);
engine.build(branch, gitBranch, output, &progress.m_token);
```

### 编辑器扩展

```cpp
GUIWorker::EditorExtensionRegistry registry;
registry.scan("editor/extension");
```

### flow 复用

`flow gui` 通过 WizardWindow 预填页面并收集结果；页面跳转需遵循 flow 守卫，避免加载完成回调回跳。

## 注意事项

- 页面不得直接执行 Git/网络，走引擎；
- 修改页面类成员后所有 `new Xxx()` 的 TU 必须重建；
- 扩展注册表必须在主窗口构造前初始化；
- 日志用 `CLogger`（英文）。

## 相关

- [README.md](./README.md)
- [编辑器扩展](../../../extensions/editor/index.md)
