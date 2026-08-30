# NeoExporter_Modrinth 说明文档

## 概述

`NeoExporter_Modrinth` 是 NeoServerUpdateModpack 的**导出插件**（Modpack Exporter）：实现 `NeoCore::IModpackExporter` 接口，将构建完成的整合包打包为 **Modrinth Modpack Format**（`.mrpack`）。在项目中与 `NeoExporter_MCBBS`、`NeoExporter_HMCL` 并列，被 `NeoBuild::ModpackExporter` 从 `exporters/` 目录扫描加载，面向 Modrinth 平台发布场景。

- 读取 Meta：`NeoExporter_Modrinth.meta.json` 声明 `format: "modrinth"`、`extension: ".mrpack"`。
- 打包结构：`modrinth.index.json` + `overrides/` 目录（每个文件附带 SHA-1/SHA-512 哈希）。
- 实现类：`ModrinthExporter`（匿名命名空间内），导出工厂函数 `CreateExporter`。
- 构建出的整合包**不包含 Minecraft 游戏和加载器**（依赖在 index 的 `dependencies` 声明）。

## 设计目标

- **符合 mrpack 规范**：`modrinth.index.json` 提供 `formatVersion`/`game`/`versionId`/`name`/`summary`/`files`（含 `hashes.sha1`+`hashes.sha512`、`downloads`、`fileSize`）/`dependencies`，客户端/服务端按 index 还原文件。
- **哈希完整**：打包时对每个文件流式计算 SHA-1 与 SHA-512（64 KiB 缓冲），写入 index，支持平台校验与增量下载。
- **加载器 ID 映射**：将内部 `modloader` 简称映射为 mrpack 依赖键（fabric→`fabric-loader`、quilt→`quilt-loader`、forge→`forge`、neoforge→`neoforge`）。
- **插件内自带构建引擎**：与 MCBBS 插件同构，`build_modpack` 在插件内用 `NeoBuild::BuildEngine` 完成真实构建。

## 模块边界

**做什么**

- `build_modpack`：`BuildEngine` 构建流程，产物落 `target.output_path`。
- `export_modpack`：扫描构建目录逐文件计算 SHA-1/SHA-512/大小 → 组装 `modrinth.index.json` → 与文件本体一并写入 `.mrpack`（zip）。
- `preview_structure`：返回 `modrinth.index.json` + `overrides/<rel>` 的模拟结构。

**不做什么**

- **不生成 `client-overrides/` / `server-overrides/`**：当前实现只以 `overrides/` 前缀打包全部构建产物，index `files` 也统一记录——背景所述「可选 client/server-overrides」尚未落地 [与规格描述的差异，以实际代码为准]。
- **不包含游戏本体/加载器文件**：加载器在 `dependencies` 中声明（含版本），文件本体不进包。
- **不做 HMCL 工作区同步**：`sync_to_directory=true` 属 `NeoExporter_HMCL`。
- **不校验现有文件是否来自 Modrinth CDN**：`downloads` 恒为空数组（每个文件 `fileEntry["downloads"] = json::array()`）。

## 依赖关系

依赖以 `modules/NeoExporter_Modrinth/CMakeLists.txt` 为准（均为 `PRIVATE`）：

| 依赖 | 类型 | 用途 |
|------|------|------|
| `NeoCore` | 静态库 | `IModpackExporter`/`ExportMetadata`/`BuildTarget` 接口；经 PUBLIC 链传递 `CommonLoggerCPP` 的 `CLogger`/`plugin_log_sink.h` |
| `NeoBuild` | 静态库 | `BuildEngine`（`NeoBuild/include/build_engine.h`）执行真实构建 |
| `spdlog::spdlog` | vcpkg | 日志（源码实际经 `CLogger` 输出） |
| `Qt6::Core` | Qt6 | `QFile`/`QFileInfo`/`QDir`/`QDirIterator`/`QCryptographicHash`/`QIODevice`/`QDateTime` |
| `libzippp::libzippp` | vcpkg | zip 归档 |

头文件依赖：`IModpackExporter.h`（NeoCore）、`build_engine.h`（NeoBuild，经 include 目录 `../NeoBuild/include` 引入）、`logger.h`、`plugin_log_sink.h`（CommonLoggerCPP）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库定义、include 目录（NeoCore/include、NeoBuild/include、src）、链接项 |
| `NeoExporter_Modrinth.meta.json` | 插件元数据：名称/版本/DLL 名/`format`/`extension`/描述/`fields` |
| `src/exporter_modrinth.cpp` | `ModrinthExporter` 实现 + `CreateExporter` 导出 + 日志注册宏 |

## 构建集成

- 目标类型：`add_library(NeoExporter_Modrinth SHARED ...)` → 产出 `NeoExporter_Modrinth.dll`。
- 导出符号（`extern "C" __declspec(dllexport)`，必须带 `__declspec`，否则 `GetProcAddress` 找不到）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter() {
      return new ModrinthExporter();
  }
  ```

- 日志注册（可选演进约定）：`NEO_DECLARE_PLUGIN_LOG_SINK("NeoExporter_Modrinth")` 展开出 `SetPluginLogSink(ILogSink*)` 导出，宿主 `ModpackExporter::loadExporter` 用 `GetProcAddress` 注入；旧插件无此符号时自动跳过。
- `.meta.json` 与 DLL 一同由根 `CMakeLists.txt` 部署：`EXPORTER_TARGETS` 含 `NeoExporter_Modrinth`，`neo_deploy` POST_BUILD 将 `$<TARGET_FILE:...>` 与 `NeoExporter_Modrinth.meta.json` `copy_if_different` 到 `${DEPLOY_DIR}/exporters/`。
- VERSIONINFO：根 CMakeLists 通过 `nsum_add_version_info` 写入「NSUM 导出插件 (NeoExporter_Modrinth)」。

## 公共符号

| 符号 | 签名 | 说明 |
|------|------|------|
| `CreateExporter` | `extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter()` | 工厂：`new ModrinthExporter()` |
| `SetPluginLogSink` | `extern "C" __declspec(dllexport) void SetPluginLogSink(ILogSink*)` | 宏展开，宿主日志注入点 |
| `ModrinthExporter` | `class ... : public NeoCore::IModpackExporter`（匿名命名空间） | 实现类 |

**接口方法实现概览**（摘自 `NeoCore/include/IModpackExporter.h`）：

| 方法 | 实现要点 |
|------|----------|
| `std::string format_name() const` | 返回 `"modrinth"` |
| `std::string file_extension() const` | 返回 `".mrpack"` |
| `std::string format_description() const` | 返回 `"Modrinth Modpack Format (.mrpack)"` |
| `NeoCore::BuildResult build_modpack(const BuildTarget&, IBuildProgress*, CancelToken*)` | 插件内 `BuildEngine` 构建，输出到 `target.output_path` |
| `bool export_modpack(build_dir, output_path, metadata)` | 打包 `.mrpack`（详见功能细节） |
| `nlohmann::json preview_structure(build_dir, metadata, target_dir="")` | 返回 `modrinth.index.json`/`overrides/...` 条目数组 |