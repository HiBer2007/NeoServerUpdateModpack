# PowerHelper — Markdown 文档阅读器

PowerHelper 是 NeoServerUpdateModpack 的文档阅读器：可独立执行的 GUI 阅读器（单文档 + 文档组），
同时提供 CLI 终端渲染模式（颜色/字体/制表符/表格），并作为主程序与编辑器的帮助文档驱动器。

## 构建方式

| 方式 | 说明 | 产物 |
|------|------|------|
| 常规模式 | 随主构建 `neo_deploy` 部署 | PowerHelper.exe + PowerHelperCore.dll + cmark dll |
| 独立 EXE | 预设 `powerhelper-standalone`（静态 Qt，`POWERHELPER_STANDALONE_ONLY=ON`） | 单个自包含 EXE |

## GUI 用法

```
PowerHelper <file.md>   单文档模式（左侧目录树 + 右侧渲染视图）
PowerHelper <dir>       文档组模式（递归扫描 *.md，按标题组织目录，点击跳转）
PowerHelper             默认打开当前 exe 目录下的 docs/ 文档组
```

## CLI 用法

```
PowerHelper render <file.md>            单文档 → 终端渲染（颜色/粗斜体/制表符/表格对齐）
PowerHelper toc <file.md>               打印标题 TOC（级别 + 文本）
PowerHelper group <dir> [--toc]         文档组文件列表（路径 | 标题）与 TOC 映射
-h/--help/-help/help//h//?/-?           帮助
-v/--version//v                         版本
--json                                  JSON 标记块输出（=====JSON-BEGIN===== / =====JSON-END=====，
                                        人类日志走 stderr）
```

### JSON 协议

```json
{ "category": "powerhelper", "command": "toc|group|render", "data": { ... } }
```

- `toc`：`{file, entries:[{level, text}]}`
- `group`：`{dir, docs:[{path, title, toc?:[{level,text}]}]}`
- `render`：`{file, rendered:"<ANSI 转义后的终端文本>"}`

### 退出码

| 码 | 含义 |
|----|------|
| 0 | 成功 |
| 1 | 运行期错误（文件不存在 / 目录无 .md） |
| 2 | 用法错误（未知命令 / 目标不存在） |

## GFM 支持

表格、删除线、任务列表、自动链接默认启用。表格渲染含 CJK 全角宽度对齐计算。

## GUI 渲染（WebView2）

GUI 阅读器使用系统 **WebView2**（Microsoft Edge 运行时）渲染：

- **表格**：真实 HTML/CSS 布局（边框/表头/斑马纹/横向滚动）
- **图片**：相对路径/本地图片自动内嵌（data URI），支持 png/jpg/gif/svg/webp 等
- **链接**：http(s)/mailto 由外部浏览器打开；相对 `.md` 在阅读器内导航（含锚点跳转）；同文档 `#锚点` 平滑滚动
- **深色模式**：CSS 变量 + `prefers-color-scheme` 自动跟随系统
- 性能：浏览器内核渲染，大文档无卡顿
- **回退**：系统缺失 WebView2 运行时（旧版 Windows）时自动回退 Qt 内置 `QTextBrowser`
  渲染（功能略简，无图片/深色自动适配）

## 集成

- **PowerHelperBridge**（静态库 `PowerHelperBridge`）：`findPowerHelperExe()` / `launchReader(target)` /
  `defaultDocsDir()` / `classifyTarget(path)`——供主程序/编辑器拉起独立阅读器。
- **内嵌组件**：`PowerHelper::MarkdownViewer`（QWidget + WebView2，`setMarkdown`/`loadFile`/
  `scrollToHeading`/`scrollToHeadingText`）可直接嵌入现有 GUI；`renderToHtml` / `renderToTerminal` /
  `extractToc` / `displayWidth` / `expandTabs` 为渲染 API（CLI 终端渲染仍为 ANSI 文本）。
- **帮助驱动器**：主程序构建收敛页出现警告/失败时显示「打开帮助文档」按钮；编辑器「帮助 → 帮助文档」
  菜单；主程序状态栏左下角「帮助文档」标签（按当前页面/状态定位章节）——均经 Bridge 拉起
  PowerHelper 渲染 `deploy/docs`。

## 目录

- [渲染 API 与集成](./PowerHelper-api.md)
- [CLI 参考](./PowerHelper-cli.md)
