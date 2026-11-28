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

auto* wizard = new GUIWorker::WizardWindow(parent); // parent 可省略（默认 nullptr）
wizard->show();
```

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `WizardWindow` | `explicit WizardWindow(QWidget* parent = nullptr)` | 向导（9 页） |
| `BuildPage::startBuild` | 见 `build_page.h` | 启动构建线程 |
| `BuildPage` 信号 | `progressUpdated` / `logLine` / `buildFinished` | 进度/日志/完成 |
| `ModpackContentIde` | `explicit ModpackContentIde(QWidget* parent)` | 内容 IDE |
| `PointerEditorPanel` | 见头文件 | 指针编辑 |
| `EditorExtensionRegistry::scan` | 见头文件 | 扫描编辑器扩展 |
| `BranchEditor` / `RepoEditor` | 见头文件 | 分支/仓库设置 |

## 典型用法

### 构建

```cpp
auto* page = new GUIWorker::BuildPage(parent);
connect(page, &GUIWorker::BuildPage::buildFinished, this, [](bool ok){ /* ... */ });
page->startBuild(config);
```

> `GuiBuildProgress` 是 `BuildPage` 的私有嵌套类，外部不要直接构造；进度通过 `BuildPage` 信号获取。

### 编辑器扩展

```cpp
GUIWorker::EditorExtensionRegistry registry;
registry.scan("editor/extension");
```

### flow 复用

`flow gui` 由宿主 `src/main.cpp` 驱动 `WizardWindow` 预填页面并收集结果；页面跳转需遵循 flow 守卫，避免加载完成回调回跳。

## 注意事项

- 页面不得直接执行 Git/网络，走引擎；
- 修改页面类成员后所有 `new Xxx()` 的 TU 必须重建；
- 扩展注册表必须在主窗口构造前初始化；
- 日志用 `CLogger`（英文）。

## 相关

- [README.md](./README.md)
- [Editor 界面](../../../pack-author/editor-gui.md)