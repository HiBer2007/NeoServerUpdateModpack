# NeoBuild 说明文档

## 概述

NeoBuild 是项目的**构建引擎**模块（C++17 + Qt6，静态库）。它将「git 仓库同步 → 分支继承链层叠合并 → 指针解析与哈希缓存下载 → 目标工作目录策略同步 → serverconfig 规则同步 → 导出/预览」整条整合包构建链路封装为纯逻辑代码：向下依赖 NeoCore（进度/结果/取消/插件接口）与 NeoWorkspace（工作区/同步/扫描），向上被**主程序向导**（GUIWorker）、**CLI**（NeoCLI）、**编辑器**（NeoWorkspaceEditor）复用，并被三个导出插件（NeoExporter_MCBBS / Modrinth / HMCL）内嵌，实现 `IModpackExporter::build_modpack` 插件内构建入口——MVP 中导出插件既是打包格式实现者，也是构建引擎的宿主。

## 设计目标

- **与 GUI 解耦**：构建逻辑（流程编排、下载、合并、同步、导出）不依赖任何界面层，向导、CLI、编辑器、插件可共享同一套引擎；GUI 仅通过 `IBuildProgress` 观察进度、通过 `CancelToken` 请求取消。
- **插件化扩展**：导出格式（`IModpackExporter`）、指针解析（`IPluginPointer`）均为运行时插件，新增格式/平台零改动引擎。
- **离线友好与零信任**：按 SHA-256 去重缓存下载结果；每次使用缓存前重算哈希，防止磁盘损坏/篡改导致的静默数据错误。
- **可编排、可预览**：构建拆分为 `stepXxx` 分步方法供精细控制；`previewStructure` / UMD 预览不写文件即可模拟最终结构（供构建清单页/IDE 输出树展示）。
- **单一职责**：独立出模块后，NeoCore 专注基础能力，NeoWorkspace 专注工作区，NeoBuild 专注"把这些东西串成一次构建/导出"。

## 模块边界

**做什么（本模块职责）**：

- 构建流程编排：`BuildEngine::init/build` 及各 `stepXxx` 分步（clone/fetch、checkout、分支合并、文件/指针处理、custom mods、serverconfig、finalize、目标目录同步）。
- 分支层叠合并：`BranchMerger` 按继承链逐层合并 `branches/<branch>/`，应用 `branch_manifest.json` 的 delete/override 标记与 `.overrides/`。
- 指针解析与下载：`PointerDownloader` 加载指针解析器插件、解析 URL、QNetwork 下载、SHA-256 缓存与零信任校验。
- 同步执行：`SyncPolicyExecutor`（构建目录 → HMCL 目标工作目录，L1 文件夹策略 + L2 配置文件特化 + mods mirror 特殊处理）；`ServerConfigSync`（L3 serverconfig 规则同步）。
- 导出与预览：`ModpackExporter` 扫描/加载导出插件 DLL 并调用导出；`BuildEngine::previewStructure`、`generateUmdStructure*` 生成虚拟构建预览。
- 辅助：JAR modId 提取（`mod_metadata`）、平台 API（目录/OS/git 定位/磁盘空间）。

**不做什么（边界外）**：

- 不含任何 GUI（向导在 `GUIWorker`，通用组件在 `HiBerGUILibrary`）。
- 不含 CLI 参数解析与 JSON 协议（`NeoCLI`）。
- 不含配置解析器、指针解析器、导出格式插件的**实现**——这些是独立插件 DLL（`NeoParser_*` / `NeoPointer_*` / `NeoExporter_*`），NeoBuild 只负责加载与调用。
- 不持久化用户配置/不管理 git 仓库创建（前者在各宿主，后者归 `NeoWorkspace`）。

## 依赖关系

以 `modules/NeoBuild/CMakeLists.txt` 为准：

| 依赖 | 链接级别 | 用途 |
|------|----------|------|
| NeoCore | PUBLIC | `BuildProgress`/`BuildResult`/`ExportMetadata`/`PointerInfo`/`CancelToken`/`IBuildProgress`/`IPluginPointer`/`IModpackExporter`/`PluginLoader` |
| NeoWorkspace | PUBLIC | `WorkspaceManager`/`GitOperations`/`SyncEngine`/`FileScanner`/`SyncPolicy`/`BranchManifest` |
| CommonLoggerCPP | 传递（经 NeoCore PUBLIC 链） | `CLogger` / `ILogSink` / `LoggerLogSink`（NeoBuild 未直接链接） |
| Qt6::Core | PUBLIC | 基础类型、`QFile`/`QCryptographicHash` 等 |
| Qt6::Network | PUBLIC | `QNetworkAccessManager` 下载指针文件 |
| libzippp::libzippp | PUBLIC | ZIP 导出支持（构建目录打包为 .zip 等） |
| tomlplusplus::tomlplusplus | PRIVATE | TOML 解析（插件内部使用，不对外传递） |

**反向依赖**（均需链接 NeoBuild，NeoBuild 静态链接进各自目标）：

| 消费方 | 入口 |
|--------|------|
| 主程序 NeoServerUpdateModpack（根 EXE） | `src/main.cpp` 分发 CLI/GUI；向导流程 |
| GUIWorker（向导页面） | 构建清单/虚拟构建预览、构建执行 |
| NeoCLI（CLI 子系统） | `info` 预览（`BuildEngine` 虚拟构建 + `ModpackExporter::previewStructure`）、`exec` 构建 |
| NeoWorkspaceEditor（编辑器 EXE） | 输出树 UMD 预览（`generateUmdStructureFromLayers`） |
| NeoExporter_MCBBS / NeoExporter_Modrinth / NeoExporter_HMCL（导出插件 DLL） | `IModpackExporter::build_modpack` 内嵌 `BuildEngine` |

## 文件组成

| 头文件 (include/) | 实现 (src/) | 职责 |
|------|------|------|
| build_engine.h | build_engine.cpp | 构建流程编排：`init`/`build` 分步执行/导出/预览；进度报告与取消检查 |
| branch_merger.h | branch_merger.cpp | 分支继承链层叠合并（`BranchLayer`/`MergeResult`，delete/override 标记 + `.overrides/`） |
| mod_metadata.h | mod_metadata.cpp | 从 JAR 提取 modId（NeoForge > Forge mods.toml > Fabric fabric.mod.json > Forge 旧版 mcmod.info） |
| modpack_exporter.h | modpack_exporter.cpp | 导出插件扫描/加载/卸载（`CreateExporter` + `.meta.json`），导出与预览 |
| platform_api.h | platform_api.cpp | 平台目录（AppData/Cache/Config/Temp/默认工作区）、OS 判定、git 定位、磁盘空间 |
| pointer_downloader.h | pointer_downloader.cpp | 指针解析插件加载、URL 解析、下载、SHA-256 缓存与零信任校验 |
| sync_policy_executor.h | sync_policy_executor.cpp | 构建目录 → 目标工作目录按 `SyncPolicy` 同步（L1/L2 + mods mirror + hashes.json） |
| umd_generator.h | umd_generator.cpp | U/M/D 虚拟构建预览（落盘目录版 + 分支层叠内存合并版） |
| serverconfig_sync.h | serverconfig_sync.cpp | serverconfig 规则同步（L3，`save/[save]/serverconfig` 规则文件） |

## 构建集成

- **CMake target**：`NeoBuild`，类型 `add_library(... STATIC)`（根 `CMakeLists.txt:96` `add_subdirectory(modules/NeoBuild)`）。
- **头文件**：`target_include_directories(NeoBuild PUBLIC include PRIVATE src)` + PRIVATE 纳入 `../NeoCore/include`、`../NeoWorkspace/include`。公共 include 目录随链接传递，使用方直接 `#include <build_engine.h>` 等。
- **链接**：PUBLIC 链 `NeoCore`、`NeoWorkspace`、`Qt6::Core`、`Qt6::Network`、`cpr::cpr`、`libzippp::libzippp`；PRIVATE 链 `tomlplusplus::tomlplusplus`（不传递）。
- **MOC**：NeoBuild 头文件不含 `Q_OBJECT` 类，无 MOC 需求；插件 DLL 消费方在插件 CMake 中把 `../NeoBuild/include` 加入 PRIVATE include 并链接 `NeoBuild`（NeoExporter_* 即此模式）。

## 命名空间与公共符号

所有公共符号位于命名空间 **`NeoBuild`**；进度/结果/取消/插件接口类型（`BuildProgress`、`BuildResult`、`IBuildProgress`、`CancelToken`、`ExportMetadata`、`IModpackExporter`、`IPluginPointer`、`PointerInfo`）来自 `NeoCore`。

| 符号 | 一句话概览 |
|------|-----------|
| `class BuildEngine` | 构建引擎入口：`init` → `build`（或分步 `stepXxx`）→ `exportModpack`/`previewStructure`；维护 merged file_manifest / pointer_files |
| `class BranchMerger` + `struct BranchLayer` / `MergeResult` | 分支继承链层叠合并，产出 `merge` / `mergeDirectories` / manifest 读写 |
| `class PointerDownloader` + `DownloadProgress` / `DownloadResult` / `ResolveResult` | 指针解析插件注册/扫描、下载、SHA-256 缓存与零信任校验 |
| `class ModpackExporter` | 导出插件扫描加载、`exportModpack`、`previewStructure`、按格式查询插件实例 |
| `class SyncPolicyExecutor` + 内嵌 `struct Result` | 构建目录 → 目标工作目录策略同步（L1 文件夹/L2 配置文件/mods mirror） |
| `class ServerConfigSync` + `ServerConfigEntry` + `enum class ServerConfigMode` / `ServerConfigFolderMode` | serverconfig 规则同步（`save/[save]/serverconfig` + `.rule/` 规则） |
| `generateUmdStructure` / `generateUmdStructureFromLayers`（自由函数） | 生成 U/M/D 虚拟构建预览 JSON（`umd_generator.h`） |
| `extractModIds`（自由函数） | 从 JAR 提取 modId（`mod_metadata.h`） |
| 平台 API 函数集 | 目录/OS/git/磁盘空间查询（`platform_api.h`） |

类型别名（NeoBuild 命名空间内，摘自 build_engine.h）：

```cpp
using BuildProgress = NeoCore::BuildProgress;
using BuildResult = NeoCore::BuildResult;
```