# NeoExporter_HMCL 说明文档

## 概述

`NeoExporter_HMCL` 是 NeoServerUpdateModpack 的**导出插件**（Modpack Exporter）：实现 `NeoCore::IModpackExporter` 接口，采用 **HMCL 工作区同步**模式——不在插件内打包压缩包，而是把构建结果**直接写入 HMCL 游戏工作目录**（`sync_to_directory=true` 语义）。在项目中与 `NeoExporter_MCBBS`、`NeoExporter_Modrinth` 并列，被 `NeoBuild::ModpackExporter` 从 `exporters/` 目录扫描加载。

- 读取 Meta：`NeoExporter_HMCL.meta.json` 声明 `format: "hmcl"`、`extension: "工作目录"`（无 `fields`）。
- 与其它导出器不同：**不实现 `export_modpack`**（恒返回 `false`），构建与"导出"合二为一——`build_modpack` 内 `BuildEngine::setTargetDir` 后直接同步。
- 预览走 `NeoBuild::generateUmdStructure`，对目标工作目录做真实比对生成 `U/M/D` 标记。
- 实现类：`HmclExporter`（全局命名空间），导出工厂函数 `CreateExporter`。
- 构建出的目录结构**不包含 Minecraft 游戏和加载器**（HMCL 侧负责游戏本体）。

## 设计目标

- **原地更新**：直接向 HMCL 工作目录写构建产物，启动器读取即用，无 zip 解压环节。
- **差异可视**：`preview_structure` 以目标目录为基准，用 UMD（U=新建/M=修改/D=删除）标记展示同步将造成的变化，配合 `BuildEngine::stepSyncTarget` 的 sync_policies 执行。
- **缓存复用**：中间构建先落 staging（默认 `<cache_dir 父目录>/staging/<branch>`），再同步到目标目录，避免污染工作目录的中间态。
- **构建引擎内置**：`build_modpack` 在插件内构造 `NeoBuild::BuildEngine` 完成真实构建 + 同步两步。

## 模块边界

**做什么**

- `build_modpack`：校验 `target.output_path` 非空 → 计算 staging → `engine.init(wsJson, cache_dir, staging)` → `engine.setTargetDir(target.output_path)` → `engine.build(branch, progress, cancel)`（构建闭环内含 `stepSyncTarget` 同步）。
- `preview_structure`：委托 `NeoBuild::generateUmdStructure(build_dir, target_dir, nullptr, nullptr)` 返回 U/M/D 结构。

**不做什么**

- **不做 zip/mrpack 打包**：`export_modpack` 恒返回 `false`，日志说明「workspace sync mode … zip export is not supported」。
- **不生成清单/索引文件**：工作目录同步无需 `modrinth.index.json` 或 `manifest.json`。
- **不收集额外字段**：meta.json 无 `fields`——`BuildTarget.metadata` 的填充分组不由本插件驱动。
- **不包含游戏本体/加载器**：目标目录仅接收构建出的整合包内容。

## 依赖关系

依赖以 `modules/NeoExporter_HMCL/CMakeLists.txt` 为准（均为 `PRIVATE`）：

| 依赖 | 类型 | 用途 |
|------|------|------|
| `NeoCore` | 静态库 | `IModpackExporter`/`ExportMetadata`/`BuildTarget` 接口；经 PUBLIC 链传递 `CommonLoggerCPP` 的 `CLogger`/`plugin_log_sink.h` |
| `NeoBuild` | 静态库 | `BuildEngine`（`NeoBuild/include/build_engine.h`）构建+同步；`umd_generator.h` 的 `generateUmdStructure` 预览 |
| `spdlog::spdlog` | vcpkg | 日志（源码实际经 `CLogger` 输出） |
| `Qt6::Core` | Qt6 | 已链接（源码未直接引用 Qt 头，经 NeoBuild 传递使用 [保持链接一致性]） |
| `libzippp::libzippp` | vcpkg | 已链接（工作区同步模式未实际使用 zip [与 MCBBS/Modrinth 插件的链接清单保持一致]） |

头文件依赖：`IModpackExporter.h`（NeoCore）、`build_engine.h`/`umd_generator.h`（NeoBuild，经 include 目录 `../NeoBuild/include` 引入）、`logger.h`、`plugin_log_sink.h`（CommonLoggerCPP）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库定义、include 目录（NeoCore/include、NeoBuild/include、src）、链接项 |
| `NeoExporter_HMCL.meta.json` | 插件元数据：名称/版本/DLL 名/`format`/`extension`/描述（无 `fields`） |
| `src/exporter_hmcl.cpp` | `HmclExporter` 实现 + `CreateExporter` 导出 + 日志注册宏 |

## 构建集成

- 目标类型：`add_library(NeoExporter_HMCL SHARED ...)` → 产出 `NeoExporter_HMCL.dll`。
- 导出符号（`extern "C" __declspec(dllexport)`，必须带 `__declspec`，否则 `GetProcAddress` 找不到）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter()
  {
      return new HmclExporter();
  }
  ```

- 日志注册（可选演进约定）：`NEO_DECLARE_PLUGIN_LOG_SINK("NeoExporter_HMCL")` 展开出 `SetPluginLogSink(ILogSink*)` 导出，宿主 `ModpackExporter::loadExporter` 用 `GetProcAddress` 注入；旧插件无此符号时自动跳过。
- `.meta.json` 与 DLL 一同由根 `CMakeLists.txt` 部署：`EXPORTER_TARGETS` 含 `NeoExporter_HMCL`，`neo_deploy` POST_BUILD 将 `$<TARGET_FILE:...>` 与 `NeoExporter_HMCL.meta.json` `copy_if_different` 到 `${DEPLOY_DIR}/exporters/`。
- VERSIONINFO：根 CMakeLists 通过 `nsum_add_version_info` 写入「NSUM 导出插件 (NeoExporter_HMCL)」。

## 公共符号

| 符号 | 签名 | 说明 |
|------|------|------|
| `CreateExporter` | `extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter()` | 工厂：`new HmclExporter()` |
| `SetPluginLogSink` | `extern "C" __declspec(dllexport) void SetPluginLogSink(ILogSink*)` | 宏展开，宿主日志注入点 |
| `HmclExporter` | `class ... : public NeoCore::IModpackExporter`（全局命名空间） | 实现类（与其它导出器不同，未放入匿名命名空间） |

**接口方法实现概览**（摘自 `NeoCore/include/IModpackExporter.h`）：

| 方法 | 实现要点 |
|------|----------|
| `std::string format_name() const` | 返回 `"hmcl"` |
| `std::string file_extension() const` | 返回 `""`（空扩展名 = 目录型导出，调用方据此不做后缀处理） |
| `std::string format_description() const` | 返回 `"HMCL 工作区同步（同步到游戏工作目录）"` |
| `NeoCore::BuildResult build_modpack(const BuildTarget&, IBuildProgress*, CancelToken*)` | 校验 output_path → staging → init → `setTargetDir` → `build(branch, progress, cancel, git_branch)` |
| `bool export_modpack(build_dir, output_path, metadata)` | 恒返回 `false`（工作区同步模式不支持 zip 导出） |
| `nlohmann::json preview_structure(build_dir, metadata, target_dir="")` | 返回 `generateUmdStructure(build_dir, target_dir, nullptr, nullptr)` |

## 同步实现要点（2026-11-03 修订）

工作区同步链路 = **git 分支 checkout → merge 到 staging → L1/L2/mods 镜像同步到目标目录 → L3 存档 serverconfig（`save/[save]/` 路由）**：

| 环节 | 约定 |
|------|------|
| Git 分支 | `BuildTarget` 新增 `git_branch`（空 = 与整合包分支同名），三导出插件统一传给 `engine.build(..., git_branch)`。仓库里整合包分支通常是 `branches/<name>/` 目录，git 分支可能只有 `main`，必须由 CLI `--git-branch` / GUI `gitBranch_` 显式传入 |
| 上游同步 | `fetch` 只更新 remote-tracking 引用，构建前必须同步工作树到 `origin/<branch>`：缓存仓库（`getCacheDir()` 下）用 `reset --hard`，用户仓库用 `merge --ff-only`；否则缓存仓库停在旧提交，会用陈旧内容构建 |
| 仓库内 `save/` | `branches/<branch>/save/[save]/serverconfig/`（含 `.rule/`）是**存档同步目标之一**（`[save]` = 每个存档占位）：构建产物与编辑器/预览中**保留显示**（特意保留，用于编辑要同步进存档的配置），但**绝不按字面路径写入目标**——同步器跳过 `save/**` 的字面拷贝/删除，由 L3 路由到 `<目标>/saves/<存档>/serverconfig/` |
| HMCL 系统文件 | 目标根 `modpack.cfg`、`hmclversion.cfg` 归 HMCL——不生成、不覆盖、不删（`SyncPolicyExecutor::isProtectedRoot`） |
| mods 镜像 | 仓库模组全量写入；多余项清除；`.jar.disabled` 直接删除（判定须用 13 字符 `.jar.disabled`，旧代码写 12 永不命中）；玩家自放模组收割到 `.NSUM/custom/mod/`，按 modId 冲突检测后还原回 mods |
| L2 文件策略 | `files` 为对象（`{ "config/test.json": { "mode": "partial", "tracked_keys": ["b"] } }`）；`partial` 经 `IConfigParser::merge_entries` 只写追踪键，未追踪键与本地独有键保持不变 |
| L3 存档同步 | 扫描 `目标/saves/*/serverconfig/`，每个存档都按仓库规则同步；仓库有而目标缺失的文件会创建（不止覆盖已存在文件） |
| 哈希 | `.NSUM/hashes.json` 记录写入文件的源 SHA-256；分块 64 KiB 计算（1 MiB 栈缓冲会在 CLI 主线程栈溢出 0xC00000FD） |