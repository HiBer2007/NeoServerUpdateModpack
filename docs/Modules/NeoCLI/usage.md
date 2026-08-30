# NeoCLI 使用文档

> 面向在宿主（主程序/编辑器/构建器等）中**复用 NeoCLI** 的开发者的 API 与惯例说明。
> 命令行为级说明（参数、JSON schema、退出码场景）一律以 [docs/deploy/CLI/](#相关文档) 权威文档为准，本文只讲模块接口与集成。

## 快速开始

NeoCLI 已在根 `CMakeLists.txt` 注册（`add_subdirectory(modules/NeoCLI)`），宿主只需链接；`NeoCore`/`NeoWorkspace`/`NeoBuild`/`Qt6::Core`/`nlohmann_json` 经 PUBLIC 依赖自动传递：

```cmake
target_link_libraries(my_host
    PRIVATE NeoCLI
)
```

头文件引入：

```cpp
#include <arg_parser.h>       // CliCategory / CliCommand / ArgParser
#include <cli_output.h>       // CliOutput
#include <cli_dispatcher.h>   // CliDispatcher
```

## 公共 API

### 1. `arg_parser.h` —— 参数解析

**`CliCategory`**（枚举类）：

| 值 | 含义 |
|----|------|
| `None` | 未识别/默认 |
| `Help` | help 类别（`parse` 实际不赋值此值——help 走 `CliCommand::help` 标志；枚举保留供分发端兼容分支） |
| `Version` | version 类别（`parse` 在"出现 version 标记且无 help 标记"时赋值） |
| `Info` / `Flow` / `Exec` | 三类互斥子命令 |

**`CliCommand`**（解析结果）：

| 成员 | 类型 | 说明 |
|------|------|------|
| `category` | `CliCategory` | 命令类别（默认 `None`） |
| `verb` | `std::string` | 动词（如 `"build"`） |
| `options` | `std::map<std::string, std::string>` | 选项键值（`--repo=x` / `--repo x`） |
| `positional` | `std::vector<std::string>` | 位置参数（目前仅 `exec resolve-pointer <file>` 用） |
| `prefill` | `std::vector<std::string>` | `--prefill k=v` 列表（flow 用，可重复） |
| `json` | `bool` | 是否 `--json` |
| `verbose` | `bool` | 是否 `--verbose` |
| `silent` | `bool` | 是否 `--silent` / `--quiet` |
| `help` | `bool` | 是否出现 help 标记 |
| `error` | `std::string` | 非空 = 解析失败（内容为出错的 token） |

| 方法 | 签名 | 说明 |
|------|------|------|
| `has` | `bool has(const std::string& key) const` | `options` 是否含该键 |
| `get` | `std::string get(const std::string& key) const` | 取选项值，缺省返回空串 |

**`ArgParser`**（解析器）：

| 方法 | 签名 | 说明 |
|------|------|------|
| 构造 | `ArgParser()` | 空构造（无状态） |
| 解析 | `CliCommand parse(int argc, char* argv[])` | 核心解析；失败时 `cmd.error` 非空（**本方法不打印、不退出**） |
| 帮助 | `void printHelp() const` | 全量帮助（stdout） |
| 分类帮助 | `void printHelp(CliCategory category) const` | info/flow/exec 分类帮助 |
| 版本 | `void printVersion() const` | 单行 `NeoServerUpdateModpack CLI v1.0.0`（stdout） |
| 版本号 | `static std::string version()` | 返回 `"1.0.0"`（`arg_parser.cpp` 中 `VERSION` 常量） |
| 类别名→枚举 | `static CliCategory categoryFromName(const std::string& name)` | `"info"`→`Info` 等；未知→`None` |
| 枚举→类别名 | `static std::string categoryName(CliCategory category)` | `Info`→`"info"`；`None`/`Help`/`Version`→`"unknown"` |
| help 标记判定 | `static bool isHelpToken(const std::string& token)` | `-h` `--help` `-help` `help` `/h` `/?` `-?` |
| version 标记判定 | `static bool isVersionToken(const std::string& token)` | `-v` `--version` `/v` |
| 动词存在性 | `static bool verbExists(CliCategory category, const std::string& verb)` | 按内置动词表判定（13 info / 2 flow / 9 exec） |

> 动词表（`arg_parser.cpp` 私有静态函数）：`info` = version, system, git, git-branches, modpacks, status, workspace, preview, plugins, exporters, pointers, history, debug；`flow` = gui, console；`exec` = build, export, sync-serverconfig, verify-repo, resolve-pointer, crash-test, git-update, repo-trust, repo-trust-check。

### 2. `cli_output.h` —— 输出

全部为静态方法；输出目标与颜色见 [注意事项](#注意事项)：

| 方法 | 签名 | 说明 |
|------|------|------|
| info | `static void info(const std::string& msg)` | `[*] msg`，stdout（JSON 模式转 stderr）；quiet 时跳过 |
| success | `static void success(const std::string& msg)` | `[+] msg`，stdout（JSON 模式转 stderr） |
| warning | `static void warning(const std::string& msg)` | `[!] msg`，恒 stderr |
| error | `static void error(const std::string& msg)` | `[-] msg`，恒 stderr（不受 quiet 影响） |
| progress | `static void progress(int percent, const std::string& msg)` | 行内进度条（`\r` 刷新，`p>=100` 换行）；percent 钳制 0–100 |
| table | `static void table(const std::vector<std::string>& headers, const std::vector<std::vector<std::string>>& rows)` | 对齐表格（表头加粗 + 分隔线） |
| separator | `static void separator(char c = '-', int width = 60)` | 分隔线 |
| title | `static void title(const std::string& title)` | 居中标题（`====` 包裹） |
| jsonBlock | `static void jsonBlock(const nlohmann::json& value)` | **恒写 stdout**：`=====JSON-BEGIN=====` + 2 空格缩进 JSON + `=====JSON-END=====` |
| setQuiet | `static void setQuiet(bool quiet)` | 静默模式（仅 error） |
| setVerbose | `static void setVerbose(bool verbose)` | 详细模式（**当前为保留位**，输出逻辑暂未消费 `verbose_`） |
| isQuiet | `static bool isQuiet()` | 查询静默标志 |
| setJsonMode | `static void setJsonMode(bool on)` | `--json` 模式：人类日志转 stderr |
| isJsonMode | `static bool isJsonMode()` | 查询 JSON 模式 |

私有：`CliOutput() = delete;`（纯静态工具类）；`static bool useColors()`（TTY 探测 + Windows 启用 VT 处理）。

### 3. `cli_dispatcher.h` —— 分发

| 方法 | 签名 | 说明 |
|------|------|------|
| 构造 | `CliDispatcher()` | 安装 SIGINT 处理器（若先前非 `SIG_IGN`） |
| 分发 | `int dispatch(const CliCommand& cmd)` | 按 category/verb 路由到命令实现并返回退出码（0/1/2，`crash-test` 为 0xC0000005）；开头重置 `CancelToken` 并同步 quiet/verbose/json 模式 |
| Git 配置 | `void setGitConfig(const std::string& gitPath, bool useSystemGit)` | 注入 git 路径与"是否系统 Git"（驱动 info git / build / git-update 等） |
| 取消查询 | `bool isCancelled() const` | `CancelToken` 是否已取消 |
| 取消 | `void cancel()` | 请求取消（`cancelToken_.request_cancel()`） |

`dispatch` 路由表（私有实现）：

| 类别 | 动词 → 方法 |
|------|------------|
| `Info` | version → `cmdInfoVersion`；system → `cmdInfoSystem`；git → `cmdInfoGit`；git-branches → `cmdListBranches`；modpacks → `cmdListModpacks`；status → `cmdStatus`；workspace → `cmdInfoWorkspace`；preview → `cmdInfoPreview`；plugins → `cmdInfoPlugins`；exporters → `cmdInfoExporters`；pointers → `cmdInfoPointers`；history → `cmdInfoHistory`；debug → `cmdInfoDebug` |
| `Flow` | console → `cmdFlowConsole`（**gui 不在本模块**，见注意事项） |
| `Exec` | build → `cmdBuild`；export → `cmdExport`；sync-serverconfig → `cmdSyncServerConfig`；verify-repo → `cmdVerifyRepo`；resolve-pointer → `cmdResolvePointer`；crash-test → `cmdCrashTest`；git-update → `cmdGitUpdate`；repo-trust → `cmdRepoTrust`；repo-trust-check → `cmdRepoTrustCheck` |
| 兜底 | 未匹配 → `notImplemented`：`Command '<cat> <verb>' is not implemented yet.`，**exit 2** |
| `Help`/`Version`/`cmd.help` | 打印帮助/版本后**直接 return 0**（在 `CLogger::Init` 之前，不触碰日志） |
| 未知类别 | `Unknown command: '<error>'. Use --help for usage.` → exit 2 |

私有辅助（不构成公开 API，供参考）：`gitVersion`、`resolveWorkDir`（`cache/repos/<slug>`）、`resolveRepoPath`（本地路径/URL 判定）、`ensureRepoCloned`（clone/fetch/checkout + dubious ownership 检测）、`findWorkspaceJson`、`findBuildOutputDir`（`.minecraft/versions/<branch>`）。

## 典型用法

**① 宿主解析 + 分发（与 `src/main.cpp` 同款模式）**

```cpp
#include <arg_parser.h>
#include <cli_dispatcher.h>
#include <iostream>

NeoCLI::ArgParser parser;
NeoCLI::CliCommand cmd = parser.parse(argc, argv);

if (!cmd.error.empty()) {   // 参数错误 → 宿主负责输出并返回 2
    std::cerr << "Unknown command: '" << cmd.error
              << "'. Use --help for usage." << std::endl;
    return 2;
}
if (cmd.category == NeoCLI::CliCategory::Version) { parser.printVersion(); return 0; }
if (cmd.category == NeoCLI::CliCategory::Help
    || (cmd.help && cmd.category == NeoCLI::CliCategory::None)) { parser.printHelp(); return 0; }
if (cmd.help) { parser.printHelp(cmd.category); return 0; }
// 注：flow gui 应在 dispatch 前由宿主拦截并交给 GUIWorker 向导（见 main.cpp runFlowGuiMode）

NeoCLI::CliDispatcher dispatcher;
dispatcher.setGitConfig("git", /*useSystemGit=*/true);
return dispatcher.dispatch(cmd);
```

**② 输出 JSON 标记块**

```cpp
#include <cli_output.h>
#include <nlohmann/json.hpp>

nlohmann::json result = {
    {"category", "info"},
    {"command", "version"},
    {"data", {
        {"version", NeoCLI::ArgParser::version()},
        {"build_type", "debug"}
    }}
};
NeoCLI::CliOutput::jsonBlock(result);
// stdout:
// =====JSON-BEGIN=====
// {
//   "category": "info",
//   ...
// }
// =====JSON-END=====
```

**③ 人类可读输出 + 表格 + 进度**

```cpp
CliOutput::setJsonMode(cmd.json);        // --json 时 info/success 等转 stderr
CliOutput::setQuiet(cmd.silent);
CliOutput::title("Git Branches");
CliOutput::info("Repository: " + repoUrl);
CliOutput::table({"Branch"}, {{"master"}, {"dev"}});
CliOutput::progress(50, "merging...");   // 行内进度条
CliOutput::success("Build completed successfully.");
CliOutput::warning("Fetch failed, using local workspace.");
CliOutput::error("Failed to clone repository: ...");
```

**④ 新增子命令的扩展点（改源码，非运行时注册）**

NeoCLI 的分发是**编译期路由**而非注册表：新命令需 ① 在 `arg_parser.cpp` 的 `infoVerbs()/flowVerbs()/execVerbs()` 动词表加动词（否则 `verbExists` 判失败 → exit 2）；② 在 `cli_dispatcher.h` 声明 `int cmdXxx(const CliCommand&)` 并在 `dispatchInfo/dispatchFlow/dispatchExec` 中加路由分支；③ 命令实现遵循「`CliOutput::jsonBlock` 输出 JSON 外壳、人类模式走 `CliOutput::*`、退出码 0/1/2 约定」。若只是"注册一个外部命令处理器"（不修改 NeoCLI），应继承/包装 `CliDispatcher` 或在自己的宿主层做前置路由（如 main.cpp 对 `flow gui` 的做法）。

## 注意事项

- **exit 2 = 参数错误，且 `parse()` 不负责输出**：`cmd.error` 非空时由宿主打印 `Unknown command: '<x>'. Use --help for usage.` 并返回 2。类别缺动词、动词不存在、动词后出现 `info/flow/exec` 均归类为同一错误。
- **`--json` 标记块协议**：`jsonBlock()` **恒写 stdout**，含 `=====JSON-BEGIN=====`/`=====JSON-END=====`；消费端必须按标记提取，不能假设整个 stdout 就是 JSON（spdlog `[builder]` 行也走 stdout）。`--json` 模式下 `info/success/progress/table/separator/title` 自动改道 stderr，`warning/error` 恒 stderr。`flow console` **无需 `--json` 也输出 JSON 块**；`exec build/export/sync-serverconfig` 从不输出 JSON 块（`--json` 被忽略）。
- **verbose 仅 `--verbose`**：`-v` 是 version 族（`-v`/`--version`/`/v`），不再作 verbose；verbose 标志当前为保留位（`cli_output.cpp` 只写入 `verbose_`，输出逻辑未消费）。
- **help/version 优先级**：参数中出现 version 标记且无 help 标记 → 无论类别/动词直接输出版本（exit 0）；出现 help 标记（7 种写法 `-h/--help/-help/help//h//?/-?`）→ 帮助；`info --help`/`flow --help`/`exec --help` 输出分类帮助。
- **`flow gui` 不在 NeoCLI**：`CliDispatcher::dispatchFlow` 只实现 `console`；`flow gui` 由主程序 `src/main.cpp::runFlowGuiMode` 在 `dispatch` 之前拦截，转 `GUIWorker::FlowConfig` 交给 `WizardWindow`。宿主直接 `dispatch()` 一个 `flow gui` 会落到 `notImplemented`（exit 2）。
- **取消语义**：`CliDispatcher` 构造安装 SIGINT 处理器；`dispatch` 开头 `cancelToken_.reset()`。被 Ctrl+C 打断的 `build/export/sync-serverconfig` 输出 `warning("...cancelled by user.")` 并 **exit 0**；`flow console` 的用户取消（`q`/`quit`/`cancel`/必填字段 EOF）输出 `Flow cancelled.` **exit 1**；`flow console` 可选字段 EOF = 跳过。打断 clone/fetch/checkout 同样生效。
- **仓库缓存语义（读缓存副本）**：需仓库的命令经 `ensureRepoCloned` 读 `cache/repos/<slug>` 克隆副本（存在则仅 fetch，fetch 不更新工作树）——本地**未提交**改动不可见，强刷需删缓存目录重克隆；`flow console` 对带 `.git` 的本地路径是唯一例外（直接用原目录，未提交改动可见）。陌生仓库（dubious ownership）会被检测并在 `exec repo-trust` 之前拒绝（exit 1）。
- **GUI 子系统 EXE 控制台特性（宿主复用注意）**：如在 GUI 子系统（`WIN32`）EXE 中复用 NeoCLI 输出，交互式 cmd 下 stdout 可能不连接控制台、PowerShell 捕获时表现为"无输出"，且 UTF-8 中文按 GBK 解码乱码。主程序已改为控制台子系统并做 `SetConsoleOutputCP(CP_UTF8)` + `_setmode(_O_BINARY)` + `holdOrReleaseConsole` 处理（`src/main.cpp`，本模块之外），复用方必须自行处理同款控制台初始化；验证输出用 `[Text.Encoding]::UTF8.GetString` 解码判 0 个 U+FFFD，勿信终端显示层。
- **`info pointers` 帮助文案 vs 实现**：`ArgParser::printHelp()` 中写 `pointers --repo <url> --modpack <b>`，但实现 `cmdInfoPointers` **只要求 `--repo`**（不读 `--modpack`）——权威文档 `CLI-info.md` 附图注已注明"以该表为准"，沿用即可。
- **`exec crash-test` / `git-update` 的"预期非零"**：`crash-test` 故意空指针崩溃，进程退出码 0xC0000005（2 秒延迟后崩，`--json` 时先出 JSON 块）；`git-update` 在 `use_system_git=true` 时不做安装并 exit 1（预期行为，非故障）。
- **日志初始化**：`dispatch` 在 Help/Version/help 分支**之前不 Init 日志**；其余命令先 `CLogger::Init("builder.log", "builder")`（日志文件 `builder.log`，logger 名 `builder`，spdlog 幂等）。宿主已自行 Init 时无影响。
- **颜色只在 TTY 启用**：stdout 非终端（重定向/管道）时无 ANSI 色码；Windows 下启用 `ENABLE_VIRTUAL_TERMINAL_PROCESSING`。
- **`--prefill` 合法性**：仅 flow 使用；必须 `键=值` 且键值均非空（`--prefill repo` / `--prefill repo=` 均 exit 2）；额外字段键以所选格式 exporter meta `fields[].key` 为准（mcbbs/modrinth 必填 `name`、`version`），可 `info exporters --json` 查询。

## 相关文档

命令级权威文档（NeoCLI 的**行为规范**，与实现一致，改 CLI 代码须同步更新）：

| 文档 | 内容 |
|------|------|
| [docs/deploy/CLI/CLI.md](../../deploy/CLI/CLI.md) | 总览：三类子命令索引、全局选项、退出码速查、JSON 协议、快速上手 |
| [docs/deploy/CLI/CLI-usage.md](../../deploy/CLI/CLI-usage.md) | 参数解析规则、帮助/版本优先级、退出码、JSON 协议、仓库缓存与克隆语义、Ctrl+C 取消、输出路由 |
| [docs/deploy/CLI/CLI-info.md](../../deploy/CLI/CLI-info.md) | 13 个 info 命令：参数、完整 JSON schema、成功/失败示例 |
| [docs/deploy/CLI/CLI-flow.md](../../deploy/CLI/CLI-flow.md) | flow gui/console：页面模型、`--prefill`、交互规则、JSON、取消 |
| [docs/deploy/CLI/CLI-exec.md](../../deploy/CLI/CLI-exec.md) | 9 个 exec 命令：参数、双模式、JSON schema、错误 |
| [docs/deploy/CLI/CLI-errors.md](../../deploy/CLI/CLI-errors.md) | 退出码速查（含"预期非零"）、错误清单、参数搭配、故障排查 |

> `docs/CLI.md` 是迁移重定向存根（指向 `docs/deploy/CLI/CLI.md`），非权威内容。