# NeoWorkspace 说明文档

## 概述

NeoWorkspace 是项目的**工作区引擎**（STATIC 静态库，namespace `NeoWorkspace`），承载整合包「工作区」这一领域模型的全部无界面逻辑：`workspace.json` 的加载、校验与查询（`WorkspaceManager`）；Git 命令封装（`GitOperations`，含 clone/fetch/checkout/status、SSH 密钥、陌生仓库 dubious ownership 信任）；文件同步引擎（`SyncEngine`，SHA-256 校验 + 哈希缓存 + 配置合并）；同步策略数据模型（`SyncPolicy`/`SyncPolicyFile`/`SyncPolicyFolder`）；文件扫描与差异（`FileScanner`）；最近仓库历史（`HistoryStore`，`<exe>/config/history/main.json` + 远程缓存目录）。模块解决「多个宿主（GUI 向导、工作区编辑器、构建引擎、CLI）需要同一套工作区/Git/同步逻辑」的复用问题，供 `GUIWorker`、`NeoWorkspaceEditor`、`NeoBuild`、`NeoCLI` 与主程序共用。

## 设计目标

- **领域逻辑与 UI 分离**：本模块不依赖任何 GUI 库（仅依赖 `Qt6::Core`），向导/编辑器/CLI 只是薄壳。
- **多宿主复用**：同一份工作区解析、Git 封装、同步算法被四种宿主消费，避免逻辑漂移。
- **能力沉淀为原子 API**：把容易踩坑的细节（`git status -z` 解析约定、safe.directory 正斜杠规范化、路径编码、dubious ownership 三件套）收敛在模块内一次性解决。
- **可测试性**：纯逻辑类无 QObject/MOC 依赖，可脱离 Qt 事件循环单测（`HistoryStore` 除外，见注意事项）。

## 模块边界

| 做（职责内） | 不做（职责外） |
|---|---|
| `workspace.json` 解析、校验、查询（名称/MC 版本/加载器/远程/默认分支） | GUI、向导页面、编辑器（由 `GUIWorker`/`NeoWorkspaceEditor` 承担） |
| 分支元数据（`description`/`hidden`/`parent` 继承链）与分支清单/标记（`BranchManifest`、`FileMarker`） | 构建、导出、插件加载、网络下载（由 `NeoBuild`/`NeoPointer_*`/`NeoExporter_*` 承担） |
| Git 命令封装（clone/pull/fetch/checkout/branch/status/rev-parse/log/ls-files/init/remote/add/commit/push）、SSH 密钥生成与测试、dubious ownership 检测与信任 | 解析 git 输出为结构化结果（`GitOperations` 返回原始 `stdout`，`-z` 解析约定见 usage.md 注意事项） |
| 文件同步（哈希校验、缓存读写、配置经 `IConfigParser` 合并）、同步策略模型 | 策略的**执行**（规则匹配/镜像/增量执行由 `NeoBuild` 的 `sync_policy_executor` 承担） |
| 文件扫描、指针文件读取（`parsePointerFile`）、目录差异 | 指针解析为下载 URL（由 `NeoPointer_*` 插件承担） |
| 最近仓库历史持久化（`main.json` + 缓存目录） | 远程仓库本身的管理（克隆时机/缓存策略由宿主决定） |

## 依赖关系

| 方向 | 依赖 | 说明 |
|---|---|---|
| 依赖（模块） | `NeoCore`（PUBLIC） | `ErrorCode`/`AnalyzeGitError`（git_operations.cpp 经 `<git_analyzer.h>`）、`CancelToken`（取消检查）、`IConfigParser`/`TrackingMode`（sync_engine.h）、`PointerInfo`（workspace_manager.h / file_scanner.h）均来自 NeoCore |
| 依赖（模块，间接） | `CommonLoggerCPP` | 经 `NeoCore` PUBLIC 链传递（`CLogger::Info/Error/...`，work 记录在 `<logger.h>`），本模块直接使用 |
| 依赖（第三方） | `Qt6::Core`（PUBLIC） | `QProcess`/`QFile`/`QCryptographicHash`/`QDir` 等 Qt Core 组件 |
| 依赖（第三方） | nlohmann-json | 公共头 `workspace_manager.h`/`file_scanner.h` 含 `<nlohmann/json.hpp>`；本项目由根 `find_package(nlohmann_json REQUIRED)` + vcpkg 环境提供 include；✅ 已修复（2026-08-30）：`modules/NeoWorkspace/CMakeLists.txt` 已 PUBLIC 链接 `nlohmann_json::nlohmann_json`，脱离本项目 vcpkg 布局复用时可直接复用该传递 |
| 反向 | `NeoBuild` | `target_link_libraries(NeoBuild PUBLIC ... NeoWorkspace)`、PRIVATE include `../NeoWorkspace/include` |
| 反向 | `NeoCLI` | 链接 `NeoWorkspace`，PRIVATE include `../NeoWorkspace/include` |
| 反向 | 主程序（根 CMakeLists） | `add_executable(${PROJECT_NAME} ...)` 链接 `PRIVATE ... NeoWorkspace ...` |
| 反向 | `NeoWorkspaceEditor` | 链接 `NeoWorkspace` |
| 反向（仅头引用） | `GUIWorker` | 仅 PRIVATE include `../NeoWorkspace/include`（如 repo_page.cpp 用 `HistoryStore`），不直接链接；符号由最终 EXE（主程序/编辑器均链接 NeoWorkspace）解析 |

> 依赖拓扑依据：`modules/NeoWorkspace/CMakeLists.txt`、`modules/NeoBuild/CMakeLists.txt`、`modules/NeoCLI/CMakeLists.txt`、`modules/GUIWorker/CMakeLists.txt`、根 `CMakeLists.txt`。

## 文件组成

| 文件 | 类别 | 说明 |
|---|---|---|
| `include/workspace_manager.h` | 公共头 | `WorkspaceManager`、`BranchConfig`、`BranchManifest`、`FileMarker` |
| `include/git_operations.h` | 公共头 | `GitOperations`、`GitResult` |
| `include/sync_engine.h` | 公共头 | `SyncEngine`、`SyncResult` |
| `include/sync_policy.h` | 公共头 | `SyncPolicy`、`SyncPolicyFile`、`SyncPolicyFolder`（纯数据模型，无对应 .cpp） |
| `include/file_scanner.h` | 公共头 | `FileScanner`、`FileEntry`、`DiffResult` |
| `include/history_store.h` | 公共头 | `HistoryStore`、`RecentRepo`、`RepoType` |
| `src/workspace_manager.cpp` | 实现 | workspace.json 解析/校验/分支继承/清单/策略合并 |
| `src/git_operations.cpp` | 实现 | QProcess 封装 git、SSH、dubious ownership 三件套 |
| `src/sync_engine.cpp` | 实现 | 文件/配置同步、SHA-256 缓存 |
| `src/file_scanner.cpp` | 实现 | 递归扫描、指针读取、差异计算 |
| `src/history_store.cpp` | 实现 | 历史 JSON 读写（`<exe>/config/history/`） |
| `CMakeLists.txt` | 构建 | `add_library(NeoWorkspace STATIC ...)`，见下 |

> 注：`src/` 有 5 个 .cpp，与 CMakeLists 列出的 5 个源文件一一对应；`sync_policy.h` 为纯头文件。

## 构建集成

- **target 类型**：`add_library(NeoWorkspace STATIC ...)` —— 静态库（引擎核心层，与 NeoCore/NeoBuild 同类）。
- **头文件目录**：`target_include_directories(NeoWorkspace PUBLIC include PRIVATE src PRIVATE ../NeoCore/include)` —— 消费方只需 `#include "workspace_manager.h"` 等即可。
- **链接方式**：`target_link_libraries(NeoWorkspace PUBLIC NeoCore Qt6::Core)` —— `NeoCore` 与 `Qt6::Core` 为 PUBLIC 传递，消费方链接 `NeoWorkspace` 即自动获得。
- 模块内无 QObject 派生类，无 MOC/automoc 需求；日志统一走 `CLogger`（CommonLoggerCPP，经 NeoCore 传递）。
- 引入方式（根 CMakeLists 已 `add_subdirectory(modules/NeoWorkspace)`）：

```cmake
target_link_libraries(你的目标 PRIVATE NeoWorkspace)   # 传递获得 NeoCore + Qt6::Core
```

## 命名空间与公共符号

命名空间 **`NeoWorkspace`**（`#pragma once` 头文件，位于 `include/`）。

| 符号 | 一句话概览 |
|---|---|
| `WorkspaceManager` | 工作区核心：加载/校验 `workspace.json`，查询分支、继承链、目录、清单、指针、同步策略 |
| `BranchConfig`（嵌套） | 单分支元数据：name/parent/gameVersion/modloader/modloaderVersion/description/hidden |
| `BranchManifest` | 分支清单：分支名 + 文件标记表（path → `FileMarker`），JSON 序列化/反序列化 |
| `FileMarker` | 文件标记枚举：`None`/`Delete`/`Override` |
| `GitOperations` | Git 命令封装（QProcess），含 SSH 生成/测试与 dubious ownership 信任三件套 |
| `GitResult` | Git 执行结果：exitCode/stdout/stderr/`NeoCore::ErrorCode` |
| `SyncEngine` | 文件/配置同步引擎：SHA-256 校验、哈希缓存、`IConfigParser` 合并 |
| `SyncResult` | 同步结果：success/synced/conflicted/failed/messages |
| `SyncPolicyFile` | L2 配置文件策略（mode: full/partial/ignore + tracked_keys/tracked_lines） |
| `SyncPolicyFolder` | L1 文件夹策略（policy: skip/mirror/incremental_add/incremental_overwrite/default） |
| `SyncPolicy` | 有效同步策略（顶层与分支级合并结果） |
| `FileScanner` | 递归扫描目录/指针文件、SHA-256、指针 JSON 读取、新旧文件差异 |
| `FileEntry` | 扫描条目：相对/绝对路径、sha256、大小、是否指针、修改时间 |
| `FileScanner::DiffResult` | 差异结果：added/removed/modified/unchanged |
| `HistoryStore` | 最近仓库历史（`<exe>/config/history/main.json`）与缓存目录 |
| `RecentRepo` / `RepoType` | 历史条目（location/cachePath）与仓库类型（Remote/Local/Cache） |

详细 API 清单与调用示例见 [usage.md](usage.md)。