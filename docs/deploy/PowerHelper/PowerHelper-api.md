# PowerHelper 渲染 API 与集成

本页说明 PowerHelperCore 的渲染 API 与两种集成方式。

## 渲染 API（PowerHelper 命名空间）

| 函数 | 说明 |
|------|------|
| `renderToHtml(md)` | MD → HTML（GFM，GUI 阅读器主渲染管线） |
| `renderToTerminal(md)` | MD → ANSI 终端文本（颜色/粗斜体/制表符/表格列对齐） |
| `extractToc(md)` | 提取标题目录 `{level, text}` |
| `displayWidth(text)` | 显示宽度（CJK 全角计 2） |
| `expandTabs(text, w)` | 制表符按显示宽度展开 |

### 终端渲染效果示例

```cpp
PowerHelper render docs/deploy/PowerHelper/PowerHelper.md
```

支持：标题（H1 白色加粗下划线 / H2 青色 / H3 黄色）、粗体、斜体、行内代码反白、
删除线、链接蓝色下划线 + URL、GFM 表格盒式框线（CJK 列宽 + 左/中/右对齐）、
无序/有序/嵌套列表、任务列表 ☑/☐、代码块、引用块、分隔线。

## GUI 渲染（WebView2）

`PowerHelper::MarkdownViewer` 为 QWidget + WebView2 组件（系统 Edge 运行时），
HTML 管线：cmark renderToHtml → 标题双锚点（`h-N` + 标题文本）→ 相对/本地图片
内嵌 data URI → CSS 模板（深色自动适配）→ NavigateToString。

**WebView2 缺失自动回退**：系统未装 WebView2 运行时（旧版 Windows）时，
`MarkdownViewer` 自动回退 Qt 内置 `QTextBrowser`（`renderToDocument` 渲染、
`ph-N` 锚点跳转、相对链接转发 `openFileRequested`）。可用 `usingWebView()`
判断当前是否走 WebView2 渲染。

## 内嵌集成（动态库模式）

```cpp
#include <markdown_viewer.h>

auto* viewer = new PowerHelper::MarkdownViewer(parent);
viewer->setMarkdown(md);            // 或 loadFile(path)
viewer->scrollToHeading(0);         // 跳转标题锚点 (JS scrollIntoView)
```

- 点击 http(s)/mailto 链接 → 系统浏览器；相对 `.md` 链接 → `openFileRequested(path, anchor)` 信号
  由宿主处理（阅读器内部导航）；同文档 `#锚点` → 自动平滑滚动。

## 拉起独立阅读器（PowerHelperBridge 静态库）

```cpp
#include <powerhelper_bridge.h>

PowerHelper::Bridge::launchReader(
    PowerHelper::Bridge::defaultDocsDir());   // 拉起 exeDir/docs 文档组
```

| API | 说明 |
|-----|------|
| `findPowerHelperExe()` | 当前 exe 同目录的 PowerHelper.exe 路径 |
| `launchReader(target, extraArgs)` | QProcess 拉起外壳阅读器（文件或目录；`--anchor <标题文本>` 定位章节） |
| `defaultDocsDir()` | 当前 exe 目录 `/docs` |
| `classifyTarget(path)` | File / Dir / Unknown |

## 独立 EXE（静态 Qt）

预设 `powerhelper-standalone`：`POWERHELPER_STANDALONE_ONLY=ON` + 静态 Qt（`C:/Qt-static`）
+ `x64-windows-static`。此模式下 PowerHelperCore 编译为静态库并入 EXE，无 DLL 依赖。
WebView2 运行时为系统组件，无需分发。
