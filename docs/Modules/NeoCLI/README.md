# NeoCLI 说明文档

## 概述

NeoCLI 是 NeoServerUpdateModpack 的命令行子系统模块（英文输出），负责**命令行参数解析、命令分发与统一输出**。它把"用户敲一条命令 → 拆成结构化请求 → 调用业务引擎 → 以人类可读或 JSON 形式返回结果"整条链路收敛到一个可复用模块中，供主程序（`NeoServerUpdateModpack.exe`）、编辑器、构建器、PowerHelper 等宿主复用。

命令采用**三类互斥子命令**架构：`info`（信息获取，只读）/ `flow`（流程控制，复用 GUI 向导或纯文本引导）/ `exec`（执行操作），外加 help 族（7 种写法）与 version 族（3 种写法）；`--json` 输出使用 `=====JSON-BEGIN=====` / `=====JSON-END=====` 标记块协议；退出码语义统一（0 成功 / 1 取消或操作失败 / 2 参数错误）。

NeoCLI 只做"参数与分发"这一层：具体业务（工作区克隆、虚拟构建、导出、serverconfig 同步、指针解析）全部委托给 `NeoWorkspace` / `NeoBuild`。权威行为文档位于 `docs/deploy/CLI/`（见 [使用文档](usage.md#相关文档)）。

## 设计目标

- **与业务解耦**：参数解析（`ArgParser`）、输出（`CliOutput`）、分发（`CliDispatcher`）各自独立类，宿主按需取用；业务命令只调 NeoWorkspace/NeoBuild 的公开引擎接口，不内嵌业务实现。
- **结构化输出协议**：`info` 全部命令、`flow` 全部命令、`exec` 的 6 个校验/工具类命令统一输出 JSON 标记块，供脚本消费；人类日志在 `--json` 模式下自动改道 stderr，保证 stdout 干净可解析。
- **统一的退出码与错误语义**：解析错误 exit 2、运行期失败/取消 exit 1（构建类命令被 Ctrl+C 取消为 exit 0）、成功 exit 0——宿主无需各自发明约定。
- **可取消性**：持有 `NeoCore::CancelToken`，构造时安装 SIGINT 处理器，所有耗时的克隆/构建/同步/导出均可被 Ctrl+C 打断。
- **为何独立成模块**：CLI 逻辑体积大（三个 .cpp 合计约 2860 行、23 个命令实现 = 13 info + 1 flow console + 9 exec、MinGit 布局探测、插件目录扫描等），且与 GUI 层（GUIWorker/WizardWindow）无耦合，独立 STATIC 库后主程序、未来工具链（编辑器 CLI、PowerHelper 等）可直接链接复用，也便于配合 `docs/deploy/CLI/` 文档组做行为冻结。

## 模块边界

**做什么（本模块职责）**：

- 命令行解析：类别/动词/选项（`--key=value` 或 `--key value`，`/` 前缀归一为 `-`）/ 位置参数 / `--prefill k=v` / help / version 判定。
- 命令分发：`dispatch()` 按 `CliCategory` + `verb` 路由到具体命令实现，并负责设置安静/详细/JSON 模式。
- 输出：人类可读输出（info/success/warning/error/进度条/表格/标题/分隔线）与 JSON 标记块。
- 命令实现：`info` 13 个、`flow console` 1 个、`exec` 9 个命令的组装逻辑（调用 NeoWorkspace/NeoBuild 引擎、拼 JSON、判定退出码）。
- 仓库访问约定：`resolveWorkDir`/`ensureRepoCloned`/`findWorkspaceJson`/`findBuildOutputDir` 等辅助（缓存目录 `cache/repos/<slug>` 克隆语义）。

**不做什么（边界之外）**：

- **不含具体业务实现**——工作区解析/克隆/构建/导出/同步/指针解析都在 `NeoWorkspace`/`NeoBuild` 中；NeoCLI 只编排调用。
- **不含 GUI 逻辑**——`flow gui` 的向导在 `GUIWorker::WizardWindow`，组装发生在主程序 `src/main.cpp`（`runFlowGuiMode`）；`CliDispatcher` 只实现 `flow console`，`flow gui` 若直接经 `dispatch()` 会落到 `notImplemented`（exit 2）。
- **不是进程入口**——EXE 启动、控制台初始化（`SetConsoleOutputCP(CP_UTF8)`、`_setmode(_O_BINARY)`、`holdOrReleaseConsole`）、崩溃处理、`InstallConfig` 加载都在 `src/main.cpp`，不属于本模块。
- **不解析业务数据**——workspace.json、分支继承、插件 meta 的解析归 NeoWorkspace/NeoBuild/插件系统。

## 依赖关系

以 `modules/NeoCLI/CMakeLists.txt` 为准（全部 PUBLIC 链接，消费者可传递获得）：

| 依赖 | 类型 | 用途 | 链接级别 |
|------|------|------|:---:|
| `NeoCore` | 项目 STATIC 库 | `CancelToken`（`<cancel_token.h>`，取消语义）、`BuildTarget`/`ExportMetadata`、插件接口（`IConfigParser`/`IModpackExporter`/`IPluginPointer`）、`ErrorCodes`；`CLogger`（CommonLoggerCPP，经 NeoCore PUBLIC 链传递） | PUBLIC |
| `NeoWorkspace` | 项目 STATIC 库 | `WorkspaceManager`（workspace.json/分支/继承链/pointerFiles）、`GitOperations`（clone/fetch/checkout/status/listBranches/trust）、`HistoryStore` | PUBLIC |
| `NeoBuild` | 项目 STATIC 库 | `BuildEngine`（虚拟构建）、`ModpackExporter`、`ServerConfigSync`、`PointerDownloader`、`platform_api`（目录/磁盘/网络目录） | PUBLIC |
| `Qt6::Core` | 第三方 | `QCoreApplication`/`QProcess`/`QDir`/`QFileInfo`/`QSysInfo`/`QUrl` | PUBLIC |
| `nlohmann_json::nlohmann_json` | 第三方（vcpkg） | JSON 块构造与解析 | PUBLIC |

**反向依赖**（谁链接了 NeoCLI，以 CMake 为准）：

| 消费者 | 方式 | 说明 |
|--------|------|------|
| 主程序 EXE（根 `CMakeLists.txt` 第 166 行） | `target_link_libraries(${PROJECT_NAME} PRIVATE ... NeoCLI ...)` | `src/main.cpp` 用 `NeoCLI::ArgParser` 解析、`NeoCLI::CliDispatcher` 分发；`flow gui` 用 `NeoCLI::CliCommand` 转 `GUIWorker::FlowConfig` 再交给向导 |
| `GUIWorker` | **不依赖** | 其 CMakeLists 未链接 NeoCLI；仅主程序在两者之上做 `flow gui` 组装 |
| 编辑器 / PowerHelper / 其它模块 | **不依赖** | 未链接 NeoCLI（如需复用需自行 `add_subdirectory(modules/NeoCLI)` + 链接） |

## 文件组成

| 文件 | 说明 |
|------|------|
| `include/arg_parser.h` | `CliCategory` 枚举、`CliCommand` 结构、`ArgParser` 解析器（类别/动词/选项/位置参数/help/version 判定与帮助/版本打印） |
| `include/cli_output.h` | `CliOutput` 输出器（人类输出 + JSON 标记块 + quiet/verbose/json 模式开关） |
| `include/cli_dispatcher.h` | `CliDispatcher` 分发器（`dispatch` 入口、Git 配置、取消；私有命令方法声明） |
| `src/arg_parser.cpp` | 解析实现、三个动词表（13 info / 2 flow / 9 exec）、帮助/版本文本（`VERSION = "1.0.0"`） |
| `src/cli_output.cpp` | ANSI 颜色/TTY 探测、`--json` 输出路由、进度条/表格/标题/分隔线实现、`jsonBlock` |
| `src/cli_dispatcher.cpp` | 全部命令实现、仓库缓存/克隆辅助、MinGit 布局探测（git-update）、`CliBuildProgress`（`IBuildProgress` 的 CLI 适配器）、SIGINT 处理器 |
| `CMakeLists.txt` | STATIC 库定义、头文件目录、PUBLIC 依赖声明 |

## 构建集成

- **Target 类型**：`add_library(NeoCLI STATIC ...)` —— 静态库，与其他共享库（NeoCore/NeoWorkspace/NeoBuild/CommonLoggerCPP）同一体系，部署时被链接进 EXE。
- **包含路径**：`PUBLIC include`；`PRIVATE src`、`../NeoCore/include`、`../NeoWorkspace/include`、`../NeoBuild/include`（PRIVATE 的头只在实现里用，不对外暴露）。
- **链接方式**：PUBLIC 链接 `NeoCore NeoWorkspace NeoBuild Qt6::Core nlohmann_json::nlohmann_json`，宿主只需 `target_link_libraries(宿主 PRIVATE NeoCLI)` 即可传递获得全部依赖。
- **注册**：根 `CMakeLists.txt` 已 `add_subdirectory(modules/NeoCLI)`（第 140 行）。主程序链接片段：

```cmake
add_executable(${PROJECT_NAME} ${APP_SOURCES})
target_link_libraries(${PROJECT_NAME}
    PRIVATE NeoCore NeoWorkspace NeoBuild NeoCLI GUIWorker HiBerGUILibrary CrashTrackerHandleLib
)
```

- **依赖组件**：Qt6::Core + nlohmann-json（vcpkg 管理）；无需 Widgets/Network（GUI 与网络由被调用方承担）。

## 命名空间与公共符号

所有公共符号位于 **`namespace NeoCLI`**（与业务模块 NeoCore/NeoWorkspace/NeoBuild 平级）。三个主要类一句话概览：

| 符号 | 一句话说明 |
|------|-----------|
| `NeoCLI::CliCategory` | 枚举 `None/Help/Version/Info/Flow/Exec`——命令类别（互斥子命令架构的类别轴） |
| `NeoCLI::CliCommand` | 解析结果结构：`category`/`verb`/`options`(map)/`positional`/`prefill`/`json`/`verbose`/`silent`/`help`/`error`，附 `has()`/`get()` 便捷查询 |
| `NeoCLI::ArgParser` | 命令行解析器：`parse(argc, argv)` 产出 `CliCommand`；`printHelp()`/`printVersion()` 打印；若干静态工具（版本、类别名、help/version token、动词存在性） |
| `NeoCLI::CliOutput` | 终端输出器：`info/success/warning/error/progress/table/separator/title` 人类输出 + `jsonBlock` 标记块 + `setQuiet/setVerbose/setJsonMode` 模式开关 |
| `NeoCLI::CliDispatcher` | 命令执行入口：`dispatch(const CliCommand&)` 返回退出码；`setGitConfig`/`isCancelled`/`cancel`；内部持有 `NeoCore::CancelToken` |

---

> **一致性说明**：本说明与命令级权威文档 `docs/deploy/CLI/`（`CLI.md` 总览、`CLI-usage.md` 基础、`CLI-info.md` / `CLI-flow.md` / `CLI-exec.md` 命令详解、`CLI-errors.md` 错误排查）核对一致；差异与存疑点见 [usage.md 注意事项](usage.md#注意事项)。