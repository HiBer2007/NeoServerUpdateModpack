# PowerHelper 使用文档

本文档面向两类使用者：把 PowerHelper 当**独立阅读器**用的最终用户/脚本，以及把它**集成进宿主程序**（主程序、编辑器）的开发者。API 签名一律摘自实际头文件，与 `modules/PowerHelper/` 源码保持一致；细节以官方文档 [docs/deploy/PowerHelper/](./PowerHelper.md) 为准，冲突时以代码为准。

## 快速开始

### 作为独立阅读器（GUI）

```
PowerHelper <file.md>                  单文档模式（左侧目录树 + 右侧渲染视图）
PowerHelper <dir>                      文档组模式（递归扫描 *.md，按标题组织目录，点击跳转）
PowerHelper                            默认打开当前 exe 目录下的 docs/ 文档组
PowerHelper <file.md> --anchor <标题>   打开后按标题文本跳转（GUI 参数解析：--anchor 任意位置，
                                        目标 = 第一个非 `-` 参数）
```

### 作为独立阅读器（CLI 终端渲染）

```
PowerHelper render <file.md> [--json]  单文档 → 终端渲染（颜色/粗斜体/制表符/表格对齐）
PowerHelper toc <file.md> [--json]     打印标题 TOC（级别 + 文本）
PowerHelper group <dir> [--toc] [--json] 文档组文件列表（路径 | 标题）与 TOC 映射
PowerHelper -h | --help | /?           帮助
PowerHelper -v | --version | /v        版本
```

退出码：`0` 成功；`1` 运行期错误（文件不存在 / 目录无 .md）；`2` 用法错误（未知命令 / 目标不存在）。

### 作为集成库（CMake 链接 + launchReader）

```cmake
# 宿主 CMakeLists.txt（假设 modules/ 已 add_subdirectory 或 target 可见）
target_link_libraries(your_app PRIVATE PowerHelperBridge)
# 需要内嵌 MarkdownViewer / 直接调用渲染 API 时改链 PowerHelperCore
```

```cpp
#include <powerhelper_bridge.h>

// 拉起独立阅读器渲染当前 exe 目录下的 docs/ 文档组
PowerHelper::Bridge::launchReader(PowerHelper::Bridge::defaultDocsDir());

// 打开指定文档并定位章节（extraArgs 放在 target 之后，勿颠倒顺序）
PowerHelper::Bridge::launchReader(
    PowerHelper::Bridge::defaultDocsDir(),
    { "--anchor", "帮助入口" });
```

Bridge 是 STATIC 库、仅依赖 Qt，宿主无需链接 WebView2 SDK；渲染与 UI 都发生在独立进程（PowerHelper.exe）中，与宿主进程隔离。

## 公共 API

> 约定：core 头文件中的导出符号均带 `PH_API` 宏（见 `powerhelper_export.h`：`POWERHELPER_STATIC` 时为空，否则按 `PowerHelperCore_EXPORTS` 展开 `__declspec(dllexport/dllimport)`）。下表签名省略 `PH_API` 前缀。所有符号位于 `namespace PowerHelper`。

### 1. markdown_renderer.h —— 渲染 API

| 签名 | 说明 |
|------|------|
| `QString renderToHtml(const QString& markdown)` | Markdown → HTML（CommonMark + GFM: tables/strikethrough/autolink/tasklist/tagfilter），WebView2 主管线 |
| `QString renderToTerminal(const QString& markdown)` | Markdown → ANSI 终端文本（颜色/粗斜体/制表符/表格列对齐，CJK 全角宽度） |
| `void renderToDocument(QTextDocument* doc, const QString& markdown, bool darkMode = false)` | Markdown → QTextDocument（标题分级/表格/代码块/列表，标题带 `ph-n` 锚点供 TOC 跳转）；`darkMode=true` 深色配色 |
| `QVector<TocEntry> extractToc(const QString& markdown)` | 提取标题目录（level >= 1） |
| `int displayWidth(const QString& text)` | 显示宽度：CJK 全角按 2 计 |
| `QString expandTabs(const QString& text, int tabWidth = 4)` | 制表符展开为空格（按显示宽度对齐到下一制表位） |
| `QString anchorName(int index)` | 标题锚点名：`ph-<index>` |

```cpp
struct TocEntry {
    int level = 0;
    QString text;
};
```

### 2. markdown_viewer.h —— MarkdownViewer 组件

`class PH_API MarkdownViewer : public QWidget`（Q_OBJECT）。可嵌入现有 GUI 的阅读器视图：WebView2 渲染（真实 HTML/CSS 表格/图片/链接/深色模式）；系统缺少 WebView2 运行时（或 `onEnvCreated(nullptr)`）时自动回退 Qt 内置 `QTextBrowser` 渲染。

| 成员 | 说明 |
|------|------|
| `explicit MarkdownViewer(QWidget* parent = nullptr)` | 构造 |
| `~MarkdownViewer() override` | 析构（见注意事项第 4 条） |
| `void setMarkdown(const QString& markdown)` | 设置并渲染 Markdown 内容 |
| `void loadFile(const QString& path)` | 从文件加载并渲染 |
| `QVector<TocEntry> toc() const { return toc_; }` | 当前文档目录 |
| `void scrollToHeading(int index)` | 跳转标题锚点（WebView2 下为 JS scrollIntoView） |
| `void scrollToHeadingText(const QString& text)` | 按标题文本跳转（TOC 匹配） |
| `bool usingWebView() const` | 当前是否为 WebView2 渲染（false = Qt 回退中） |

信号：

| 信号 | 说明 |
|------|------|
| `void tocChanged()` | 文档目录变化 |
| `void openFileRequested(const QString& absPath, const QString& anchor)` | 点击文档内**相对链接**（指向其他 .md/文件），`absPath` 已解析为绝对路径，由宿主处理 |

另有 public COM 回调入口（WebView2 事件，一般宿主不需要直接调用）：`void onEnvCreated(ICoreWebView2Environment* env)` / `void onControllerCreated(ICoreWebView2Controller* controller)` / `void onMessageReceived(const QString& json)`。

### 3. doc_group.h —— 文档组扫描

| 签名 | 说明 |
|------|------|
| `QVector<DocFileInfo> scanDocGroup(const QString& dir)` | 扫描目录下所有 .md（递归、排序），填充标题与 TOC |

```cpp
struct DocFileInfo {
    QString relPath;   // 相对文档组根, 正斜杠
    QString absPath;
    QString title;     // 首个一级标题或文件名
    QVector<TocEntry> toc;
};
```

### 4. powerhelper_bridge.h —— Bridge 集成工具

`namespace PowerHelper::Bridge`（STATIC 库，仅 Qt 依赖）：

| 签名 | 说明 |
|------|------|
| `QString findPowerHelperExe()` | 定位外壳 EXE（当前进程 exe 同目录 `PowerHelper.exe`） |
| `bool launchReader(const QString& target, const QStringList& extraArgs = QStringList())` | 拉起独立阅读器（文件或目录）；成功返回 true。`target` 置于 argv[1]，`extraArgs`（如 `--anchor`）追加其后 |
| `QString defaultDocsDir()` | 默认文档组目录 = 当前 exe 目录 `/docs` |
| `TargetKind classifyTarget(const QString& path)` | 目标类型分类 |

```cpp
enum class TargetKind { File, Dir, Unknown };  // 目录→文档组, .md 文件→单文档
```

### 5. powerhelper_cli.h / reader_window.h —— 外壳 EXE 侧

| 签名 | 说明 |
|------|------|
| `int runCli(int argc, char* argv[])` | CLI 入口（命令 `render`/`toc`/`group`，见下） |
| `explicit ReaderWindow(QWidget* parent = nullptr)` | 阅读器主窗口 |
| `void openFile(const QString& path)` | 打开单文档 |
| `void openGroup(const QString& dir)` | 打开文档组 |
| `void scrollToHeadingText(const QString& text)` | 打开单文档后按标题文本跳转（TOC 匹配，找不到忽略） |

### CLI 参数速查

| 参数 | 说明 |
|------|------|
| `render <file.md> [--json]` | 终端渲染单文档 |
| `toc <file.md> [--json]` | 打印标题 TOC（每行 `<缩进><级别> <标题文本>`，H1 缩进 0、H2 缩进 1 依此类推） |
| `group <dir> [--toc] [--json]` | 文档组文件列表（`<相对路径> | <标题>`）；`--toc` 追加每文档标题清单 |
| `-h` `--help` `-help` `help` `/h` `/?` `-?` | 帮助 |
| `-v` `--version` `/v` | 版本 |
| `--json` | JSON 标记块输出：`=====JSON-BEGIN=====` / `=====JSON-END=====`（`{category:"powerhelper", command, data}`），人类日志走 stderr |

JSON `data` 协议：`toc` → `{file, entries:[{level,text}]}`；`group` → `{dir, docs:[{path,title,toc?:[{level,text}]}]}`；`render` → `{file, rendered:"<ANSI 转义后的终端文本>"}`。

## 典型用法

### 1. 集成方拉起帮助文档（主程序 done_page / 编辑器帮助菜单）

```cpp
#include <powerhelper_bridge.h>

// 打开默认文档组（exe 目录/docs）；PowerHelper.exe 缺失时返回 false
bool ok = PowerHelper::Bridge::launchReader(PowerHelper::Bridge::defaultDocsDir());
if (!ok) {
    // 提示「无法打开帮助」——确保部署目录完整
}

// 带锚点定位到具体章节（extraArgs 恒追加在 target 之后）
PowerHelper::Bridge::launchReader(
    PowerHelper::Bridge::defaultDocsDir(),
    { "--anchor", "构建失败" });
```

### 2. 内嵌渲染器（链 PowerHelperCore）

```cpp
#include <markdown_viewer.h>

auto* viewer = new PowerHelper::MarkdownViewer(parent);
viewer->setMarkdown(md);            // 或 viewer->loadFile(path)
viewer->scrollToHeading(0);         // 跳转第一个标题

// 相对 .md 链接点击 → 宿主处理导航
connect(viewer, &PowerHelper::MarkdownViewer::openFileRequested,
        this, [](const QString& absPath, const QString& anchor) {
    // 在宿主内加载 absPath（anchor 为锚点名/文本）
});
```

### 3. 无 GUI 终端渲染（脚本 / 日志输出）

```cpp
#include <markdown_renderer.h>

const QString ansi = PowerHelper::renderToTerminal(markdown);   // ANSI 文本，含表格列对齐
const auto toc = PowerHelper::extractToc(markdown);             // 只看目录
```

或直接命令行：

```
PowerHelper render README.md
PowerHelper toc README.md --json
```

### 4. 文档组扫描

```cpp
#include <doc_group.h>

const auto docs = PowerHelper::scanDocGroup(PowerHelper::Bridge::defaultDocsDir());
for (const auto& d : docs)
    qInfo("doc: %s | %s", qUtf8Printable(d.relPath), qUtf8Printable(d.title));
```

## 注意事项

| # | 陷阱 | 说明 |
|---|------|------|
| 1 | WebView2 缺失自动回退 | `MarkdownViewer` 经 `GetAvailableCoreWebView2BrowserVersionString` 探测，失败/空或 `onEnvCreated(nullptr)` 时走 `activateFallback()`：QTextBrowser + `renderToDocument` + `ph-n` 锚点跳转。回退渲染无图片/深色自动适配；用 `usingWebView()` 判断当前渲染后端 |
| 2 | 控制台子系统行为 | PowerHelper 为**控制台子系统**（CMake 不加 `WIN32`），CLI stdout 原生连接控制台，无需重定向即可见输出。GUI 模式按 `GetConsoleProcessList` 判定：与父进程共享终端（进程数 >1）→ 保持并设 UTF-8 代码页 + VT 处理；双击独占控制台（==1）→ `FreeConsole()`。**注意**：`docs/deploy/PowerHelper/PowerHelper-cli.md` 中「GUI 子系统(WIN32)」捕获建议为 2026-08-07 定案前记录，与现状有差异，实际以本表为准 |
| 3 | 帮助打开链路参数顺序 | `Bridge::launchReader` 必须把 `target` 放 argv[1]、`extraArgs` 追加其后（曾把 `--anchor` 前置导致其被当目标文件 → `QFileInfo.exists()==false` → exit 2 秒退，什么都不打开）。PowerHelper 侧 `--anchor` 任意位置可解析、目标 = 第一个非 `-` 参数；未知参数且非文件 → exit 2，不误开 GUI。GUI 模式为异步拉起，脚本验证退出码须 `Start-Process -Wait`（历史「GUI 子系统 exe 不设 `$LASTEXITCODE`」记录见于改控制台子系统之前，现状以本表为准） |
| 4 | WebView2 生命周期 / COM 细节（core 内部，扩展时注意） | 本 SDK `wrl/client.h` 无 `Make` 模板 → 回调对象用 `ComPtr<I>(new Handler(this))` 显式构造；回调跨线程队列捕获裸 COM 指针必须 AddRef/Release（否则退出期 use-after-free 0xC0000005）；析构 = `controller->Close()` + ComPtr `Detach()`，**不 CoUninitialize、不 delete impl_**（异步关闭竞态，交由进程退出回收） |
| 5 | CLI 崩溃报告 | `render`/`toc`/`group` 入口须 `SetCrashCliMode(true)`（HiBerCTM），崩溃由 `CrashTracker --cli` 控制台接管而非弹 GUI；崩溃报告在 exe 目录 `/crash-report/<date_time>/`。GUI 回归冒烟须「启动 → 等 3-4s 确认存活 → CloseMainWindow → exit 0」，并对比 crash-report 文件数，避免窗口未建完就关漏检 |
| 6 | 终端编码判定 | UTF-8 字节流在 GBK 码页终端显示乱码属显示层正常现象；判定用 `[Text.Encoding]::UTF8` 解码后 0 个 U+FFFD + 关键子串命中。CLI 入口已设 `SetConsoleOutputCP(CP_UTF8)`+`SetConsoleCP(CP_UTF8)`+`ENABLE_VIRTUAL_TERMINAL_PROCESSING`+`_O_BINARY` |
| 7 | 共享头文件改成员必重建所有引用 target | `markdown_viewer.h` 曾教训：DLL 按新偏移写成员越出 EXE 旧布局分配的对象 → `QArrayData::deref` 下溢双重释放 0xC0000005。改动 core 头类成员后 `--clean-first` 全量重建 + GUI 冒烟，勿信 ninja "no work to do" |
| 8 | cmark-gfm 0.29.0.13 新版 API（core 内部） | 扩展节点**无** `CMARK_NODE_TABLE`/`STRIKETHROUGH` 常量，判类型用 `cmark_node_get_type_string` 字符串；表格对齐用 `cmark_gfm_extensions_get_table_alignments`；表格框线字符必须 `QChar(0x2500)`（`QLatin1Char('─')` 截断 U+2500 成 NUL） |
| 9 | GUI 参数目标识别 | 目标 = 第一个非 `-` 参数；✅ 已修复（2026-08-30）：`--anchor` 与目标路径解析已从 `fromLocal8Bit` 改为 `fromUtf8`，Windows 下含非 ASCII 标题文本的锚点按 UTF-8 正确解码 |
| 10 | 锚点匹配语义 | `scrollToHeadingText` 按**包含匹配**（`contains`）定位首个命中标题，非精确相等；多个标题含同一子串时命中第一个 |

## 相关文档

- `docs/deploy/PowerHelper/PowerHelper.md` — 模块总览（构建方式 / GUI 用法 / GFM 支持 / WebView2 特性 / 集成说明）
- `docs/deploy/PowerHelper/PowerHelper-api.md` — 渲染 API 与集成（WebView2 管线、内嵌示例、独立 EXE 模式）
- `docs/deploy/PowerHelper/PowerHelper-cli.md` — CLI 参考（render/toc/group、JSON 协议、退出码、示例）
- `docs/deploy/main/operation-guide.md` — 主程序帮助入口（左下角「帮助文档」标签、完成页「打开帮助文档」按钮、缺失 PowerHelper.exe 的提示行为）
- `docs/Modules/PowerHelper/README.md` — 本模块说明文档（设计目标 / 模块边界 / 依赖 / 构建集成）