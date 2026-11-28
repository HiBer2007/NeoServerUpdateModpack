---
title: EditorDemo 使用
description: 运行 editor_demo 做 Qt 版与 WebView2 版编辑器对比、merge 预览演示与 WebView2 运行时自检。
lang: zh
status: complete
updated: 2026-11-27
---

# EditorDemo 使用

## 快速开始

```powershell
# 构建（默认构建包含 editor_demo）
cmake --build build --target editor_demo

# 运行：三个标签页，默认停在 Qt 版
.\build\...\editor_demo.exe

# 直接切到 WebView2 页（诊断运行时是否可用）
.\build\...\editor_demo.exe --web
```

窗口标题为「编辑器双版本演示 (Qt 版 vs WebView2 版)」，标签页依次为 `Qt 版编辑器`、`WebView2 版编辑器`、`merge 预览对比`。

## 公共 API

演示程序没有导出 API，可复用的是它演示的调用点（全部来自 `HiBerGUILibrary` / `HiBerGUIWebView2`）：

| API / 调用点 | 签名 | 演示内容 |
|---|---|---|
| `editor_demo` 命令行 | `editor_demo.exe [--web]` | `--web` = 启动即切到 WebView2 标签页 |
| `registerCodeEditorFactory` | `void registerCodeEditorFactory(CodeEditorKind kind, CodeEditorFactoryFn fn);` | `main` 内把 `CodeEditorKind::Web` 注册到 `createWebCodeEditor` |
| `createCodeEditor` | `ICodeEditor* createCodeEditor(CodeEditorKind kind, QWidget* parent);` | 按 `CodeEditorKind::Qt` / `Web` 取实现 |
| `CodeEditor::setLanguage` | `void setLanguage(const QString& langId) override;` | 语言下拉（`builtinLanguages()`） |
| `CodeEditor::setTabWidth` | `void setTabWidth(int spaces) override;` | Tab 宽度切换（2 / 4） |
| `CodeEditor::setDarkMode` | `void setDarkMode(bool dark) override;` | 深/浅色切换 |
| `CodeEditor::setReadOnly` | `void setReadOnly(bool ro) override;` | 只读/可编辑切换 |
| `CodeEditor::setRegionHighlights` | `void setRegionHighlights(const QVector<RegionHighlight>& regions) override;` | 预置 `{3, 4, "#2f6b31", "tracked"}` 区域标记 |
| `CodeEditor::addAction` | `void addAction(const EditorAction& action) override;` | 工具条动作 `fmt`（"缩进检查"） |
| `MergePreviewDialog::MergePreviewDialog` | `explicit MergePreviewDialog(CodeEditorKind kind, QWidget* parent = nullptr);` | 两版 merge 预览窗口 |
| `MergePreviewDialog::setContent` | `void setContent(const QString& content, const QString& info, const QString& langId, const QVector<RegionHighlight>& highlights = {});` | 载入 merge 样例与标记 |
| `WebCodeEditor::usingWebView` | `bool usingWebView() const;` | WebView2 是否就绪（状态栏提示） |

## 典型用法

### 对比两版编辑器

1. 在 `Qt 版编辑器` 页点「载入 JSON」或「载入 properties」，切换语言/Tab/主题/只读，观察行号、高亮、区域标记；
2. 切到 `WebView2 版编辑器` 页重复同样操作；
3. 状态栏出现「WebView2 版就绪。」说明运行时可用的（源码在启动 500 ms 后检查 `usingWebView()`）；
4. 若 Web 页显示错误占位而不是编辑器，说明系统缺 WebView2 运行时或 `WebView2Loader.dll` 未随 EXE 复制。

### 对比 merge 预览

在 `merge 预览对比` 页点「打开 Qt 版 merge 预览」/「打开 Web 版 merge 预览」，两者喂同一份 merge 样例：

- 黄色（`#8a6d1a`，浅色 `#f7e8a8`）= 源文件全量覆盖的行；
- 绿色（`#2f6b31` / `#b7e4c7`）= 追踪的键值对行（列区间 2–33）。

### 自检 WebView2 运行时

```powershell
# 缺运行时的典型表现：Web 页显示占位、状态栏无「就绪」
.\editor_demo.exe --web

# 确认加载器是否已随构建复制到输出目录
Get-ChildItem <构建输出目录>\WebView2Loader.dll
```

`WebView2Loader.dll` 由 `CMakeLists.txt` 的 POST_BUILD 从 `vcpkg_installed/<triplet>/bin/` 复制；系统侧还需要 Edge WebView2 Runtime。

## 注意事项

- **只是演示，不要当产品入口**：`editor_demo` 不在 `neo_deploy` 部署列表里，也不会进发布包；产品里请用 `NeoWorkspaceEditor` 或 `hci_gui` 等正式外壳。
- **样例数据硬编码在 `src/main.cpp`**：要新增对比场景（如 TOML/SNBT、超长文件性能）直接改 `sampleJson()` / `sampleProps()` / `sampleMerge()`。
- **Web 版依赖 vcpkg 的 webview2 包**：构建需要 `unofficial-webview2` 已安装；缺失时 `HiBerGUIWebView2` 无法配置。
- **Qt 版零 WebView2 依赖**：只关心 Qt 版时可以不看 Web 页；但如果链接了 `HiBerGUIWebView2`，仍会在 POST_BUILD 复制 `WebView2Loader.dll`。
- **区域标记列区间语义**：`RegionHighlight` 的 `startColumn`/`endColumn` 为 0-based、`endColumn` 不含；任一为负则整行背景——演示里绿色那条特意用了列区间。
- **控制台子系统**：EXE 未标 `WIN32`，从终端启动时会占用终端输出 `qDebug()`；`fmt` 动作的日志就打到终端。

## 相关文档

- [README.md](./README.md)
- [HiBerGUILibrary 说明](../HiBerGUILibrary/README.md)
- [HiBerGUILibrary 使用](../HiBerGUILibrary/usage.md)
