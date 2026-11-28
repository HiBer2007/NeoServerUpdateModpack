---
title: PowerHelper 使用
description: PowerHelper 的渲染/视图/桥接 API、render/toc/group CLI 与宿主拉起阅读器的真实写法。
lang: zh
status: complete
updated: 2026-11-27
---

# PowerHelper 使用

## 快速开始

**宿主侧：打开帮助文档（最常用，只依赖 `PowerHelperBridgeLib`）**

```cpp
#include <powerhelper_bridge.h>

// 打开默认文档组（<exe 目录>/docs）
PowerHelper::Bridge::launchReader(PowerHelper::Bridge::defaultDocsDir());

// 打开单篇文档并跳到某个标题
PowerHelper::Bridge::launchReader("docs/zh/user/quick-start.md",
    QStringList{"--anchor", "快速开始"});
```

**嵌入自定义视图（链接 `PowerHelperCore`）**

```cpp
#include <markdown_viewer.h>

auto* viewer = new PowerHelper::MarkdownViewer(this);
viewer->loadFile("/path/to/guide.md");
connect(viewer, &PowerHelper::MarkdownViewer::openFileRequested,
        this, [viewer](const QString& absPath, const QString& anchor) {
            viewer->loadFile(absPath);
            if (!anchor.isEmpty()) viewer->scrollToHeadingText(anchor);
        });
```

## 公共 API

### 渲染与文档组（`PowerHelperCore`）

| API | 签名 | 说明 |
|---|---|---|
| `renderToHtml` | `QString PH_API renderToHtml(const QString& markdown);` | Markdown → HTML（CommonMark + GFM：tables/strikethrough/autolink/tasklist/tagfilter） |
| `renderToTerminal` | `QString PH_API renderToTerminal(const QString& markdown);` | Markdown → ANSI 终端文本（颜色/粗斜体/制表符/表格列对齐/CJK 全角宽度） |
| `renderToDocument` | `void PH_API renderToDocument(QTextDocument* doc, const QString& markdown, bool darkMode = false);` | Markdown → `QTextDocument`，标题带 `ph-n` 锚点 |
| `extractToc` | `QVector<TocEntry> PH_API extractToc(const QString& markdown);` | 提取 `level >= 1` 的标题目录 |
| `displayWidth` | `int PH_API displayWidth(const QString& text);` | 显示宽度（CJK 全角计 2） |
| `expandTabs` | `QString PH_API expandTabs(const QString& text, int tabWidth = 4);` | 按显示宽度展开 Tab |
| `anchorName` | `QString PH_API anchorName(int index);` | 标题锚点名 `ph-<index>` |
| `scanDocGroup` | `QVector<DocFileInfo> PH_API scanDocGroup(const QString& dir);` | 递归扫描 `.md`，填充 `title` 与 `toc` |
| `MarkdownViewer::setMarkdown` | `void setMarkdown(const QString& markdown);` | 设置内容（WebView2 或回退渲染） |
| `MarkdownViewer::loadFile` | `void loadFile(const QString& path);` | 读文件并渲染，记录基目录用于相对链接 |
| `MarkdownViewer::toc` | `QVector<TocEntry> toc() const { return toc_; }` | 当前文档目录 |
| `MarkdownViewer::scrollToHeading` | `void scrollToHeading(int index);` | 按序号跳标题 |
| `MarkdownViewer::scrollToHeadingText` | `void scrollToHeadingText(const QString& text);` | 按标题文本跳转 |
| `MarkdownViewer::usingWebView` | `bool usingWebView() const;` | 当前是否真的在用 WebView2（否则为 Qt 回退） |
| `MarkdownViewer::openFileRequested` | `void openFileRequested(const QString& absPath, const QString& anchor);` | 点击文档内相对链接，`absPath` 已解析为绝对路径 |

### 桥接（`PowerHelperBridgeLib`，`PowerHelper::Bridge`）

| API | 签名 | 说明 |
|---|---|---|
| `findPowerHelperExe` | `QString findPowerHelperExe();` | 当前进程 exe 同目录的 `PowerHelper.exe` |
| `launchReader` | `bool launchReader(const QString& target, const QStringList& extraArgs = QStringList());` | 拉起独立阅读器（文件或目录），`target` 位于 argv[1] |
| `defaultDocsDir` | `QString defaultDocsDir();` | 默认文档组目录 = 当前 exe 目录 `docs` |
| `classifyTarget` | `TargetKind classifyTarget(const QString& path);` | `File`（`.md`）/`Dir`（文档组）/`Unknown` |

### 外壳 CLI

| 命令 | 用法 | 说明 |
|---|---|---|
| `render` | `PowerHelper render <file.md> [--json]` | 终端渲染（ANSI） |
| `toc` | `PowerHelper toc <file.md> [--json]` | 输出标题目录 |
| `group` | `PowerHelper group <dir> [--toc] [--json]` | 列出文档组文件（可选含 TOC） |
| 帮助/版本 | `-h/--help/-help/help//h//?/-?`、`-v/--version//v` | 与主程序一致的 help/version 族 |
| GUI | `PowerHelper <file.md|dir> [--anchor <标题>]` | 打开文档；`--anchor` 在任意位置可解析，目标 = 第一个非 `-` 参数 |

## 典型用法

### CLI 读取文档结构（供脚本/AI 使用）

```powershell
# 终端渲染一篇文档
PowerHelper.exe render docs\zh\user\quick-start.md

# 取目录（JSON 标记块）
PowerHelper.exe toc docs\zh\user\quick-start.md --json

# 列出整个文档组并带 TOC
PowerHelper.exe group docs\zh --toc --json
```

`--json` 输出形如：

```text
=====JSON-BEGIN=====
{
  "category": "powerhelper",
  "command": "toc",
  "data": [{"level": 1, "text": "快速开始"}]
}
=====JSON-END=====
```

> 注意：`--json` 时人类可读的渲染文本走 **stderr**，stdout 只保留 JSON 标记块。

### 在编辑器/主程序里提供「打开帮助」按钮

```cpp
connect(helpButton, &QPushButton::clicked, this, [] {
    const QString dir = PowerHelper::Bridge::defaultDocsDir();
    if (!PowerHelper::Bridge::launchReader(dir))
        QMessageBox::warning(nullptr, "帮助", "未找到 PowerHelper.exe");
});
```

`defaultDocsDir()` 是 `<当前 exe 目录>/docs`，所以发布包里 `docs/` 必须与主程序同级；阅读器自身定位失败时 `launchReader` 返回 `false`。

### 自定义渲染输出（不用视图）

```cpp
const QString md = QString::fromUtf8(file.readAll());

QTextDocument doc;
PowerHelper::renderToDocument(&doc, md, /*darkMode=*/true);
textBrowser->setDocument(&doc);

const QString ansi = PowerHelper::renderToTerminal(md);   // 终端/日志里直接打印
const auto toc = PowerHelper::extractToc(md);
```

## 注意事项

- **宿主不要直接链 `PowerHelperCore`**：只需要「打开文档」时链 `PowerHelperBridgeLib` 即可，核心与 WebView2 依赖留在外壳 EXE 里，避免宿主被 WebView2 运行时绑死。
- **`launchReader` 的参数顺序**：`target` 固定放 argv[1]，`extraArgs` 追加其后。历史上把 target 放在 `--anchor` 之后导致外壳把 `--anchor` 当文件路径、`exit 2` 秒退。
- **WebView2 缺失时自动回退**：`MarkdownViewer` 检测不到运行时会用 `QTextBrowser` 渲染（`usingWebView() == false`），表格/图片表现会退化但不崩溃；排查时先看这个返回值。
- **相对链接由宿主承接**：视图只发 `openFileRequested(absPath, anchor)`，加载下一篇文章是宿主的责任（见快速开始示例）。
- **`PH_API` 取决于链接方式**：默认 `PowerHelperCore` 是 SHARED（`__declspec(dllexport/dllimport)`）；`POWERHELPER_STANDALONE_ONLY=ON` 时编译为 STATIC 并定义 `POWERHELPER_STATIC`，此时宏为空。切换模式必须重新 configure 并全量重建，否则会出现导出符号不匹配的链接错误。
- **手工搬运 EXE 要带齐 DLL**：`PowerHelper.exe` 依赖同目录的 `PowerHelperCore.dll` 与 `libcmark-gfm*.dll`（构建期 POST_BUILD 复制）；只拷 EXE 会启动失败。
- **CLI 退出码**：`0` 成功、`1` 读文件失败、`2` 用法错误；未知参数且不是文件时不会误开 GUI（直接 `2`）。
- **终端输出始终带 ANSI 序列**：`renderToTerminal` 无条件插入 `\x1b[...]` 颜色/样式码，重定向到文件时也会保留；需要无颜色文本时不要用 `render` 子命令（`--json` 里的 `data.rendered` 同样是带码的字符串）。

## 相关文档

- [README.md](./README.md)
- [总体架构](../../architecture/overview.md)
- [用户文档：故障处理](../../../user/troubleshooting.md)
