# NeoExporter_MCBBS 说明文档

## 概述

`NeoExporter_MCBBS` 是 NeoServerUpdateModpack 的**导出插件**（Modpack Exporter）：实现 `NeoCore::IModpackExporter` 接口，将构建完成的整合包打包为 **MCBBS / PCL / HMCL 通用整合包格式**（`.zip`）。在项目中与 `NeoExporter_Modrinth`、`NeoExporter_HMCL` 并列，被 `NeoBuild::ModpackExporter` 从 `exporters/` 目录扫描加载，承担「通用 zip 整合包」这一默认导出格式。

- 读取 Meta：`NeoExporter_MCBBS.meta.json` 声明 `format: "mcbbs"`、`extension: ".zip"`。
- 打包结构：`manifest.json` + `mcbbs.packmeta` + `overrides/` 目录。
- 实现类：`McbbsExporter`（匿名命名空间内），导出工厂函数 `CreateExporter`。
- 构建出的整合包**不包含 Minecraft 游戏和加载器**（只含版本/加载器声明与 overrides 文件）。

## 设计目标

- **格式兼容最广**：产物为祖传 MCBBS 整合包约定（`manifest.json` + `mcbbs.packmeta` + `overrides/`），可被 MCBBS、PCL、HMCL 等多款启动器/平台识别。
- **插件内自带构建引擎**：`build_modpack` 在插件内构造 `NeoBuild::BuildEngine` 执行完整构建流程，导出插件对宿主屏蔽构建细节。
- **纯净打包**：`export_modpack` 只产出格式规定的三个组成部分，不掺入 git 元数据、构建缓存等杂项。
- **结构可预览**：`preview_structure` 返回不落盘的目录树 JSON，供构建清单页展示目标 zip 结构。

## 模块边界

**做什么**

- `build_modpack`：用 `BuildTarget` 驱动 `NeoBuild::BuildEngine` 完成 clone/fetch → checkout → merge → files → configs → custom mods → serverconfig → finalize 全流程，产物落 `target.output_path` 作为中间构建目录。
- `export_modpack`：把构建目录打包为 `.zip`——写入 `manifest.json`、`mcbbs.packmeta`，并把构建目录全部内容递归装入 `overrides/`。
- `preview_structure`：模拟最终 zip 结构（`manifest.json` + `mcbbs.packmeta` + `overrides/<rel>` 条目），不写任何文件。

**不做什么**

- **不包含游戏本体/加载器文件**：只写 `minecraft` 版本与 `modLoaders` 声明（`manifest.json` 内）。
- **不做 HMCL 工作区同步**：那是 `NeoExporter_HMCL` 的职责（`sync_to_directory=true` 分支）；本插件 `build_modpack` 不调用 `setTargetDir`。
- **不管理文件哈希/`files` 清单内容**：`manifest.json` 的 `files` 恒为空数组（见功能细节）——本格式的校验/下载依赖 overrides 目录内文件本体。
- **不做 client/server 分离**：所有构建产物统一进 `overrides/`。

## 依赖关系

依赖以 `modules/NeoExporter_MCBBS/CMakeLists.txt` 为准（均为 `PRIVATE`）：

| 依赖 | 类型 | 用途 |
|------|------|------|
| `NeoCore` | 静态库 | `IModpackExporter`/`ExportMetadata`/`BuildTarget` 接口（`NeoCore/include/IModpackExporter.h`）；经 PUBLIC 链传递 `CommonLoggerCPP` 的 `CLogger`/`plugin_log_sink.h` |
| `NeoBuild` | 静态库 | `BuildEngine`（`NeoBuild/include/build_engine.h`）执行真实构建 |
| `spdlog::spdlog` | vcpkg | 日志（源码实际经 `CLogger` 输出） |
| `Qt6::Core` | Qt6 | `QFile`/`QFileInfo`/`QDir`/`QDirIterator`/`QDateTime` |
| `libzippp::libzippp` | vcpkg | zip 归档（`libzippp::ZipArchive`） |

头文件依赖：`IModpackExporter.h`（NeoCore）、`build_engine.h`（NeoBuild，经 include 目录 `../NeoBuild/include` 引入，NeoBuild 又链 `nlohmann` 等）、`logger.h`、`plugin_log_sink.h`（CommonLoggerCPP）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | SHARED 库定义、include 目录（NeoCore/include、NeoBuild/include、src）、链接项 |
| `NeoExporter_MCBBS.meta.json` | 插件元数据：名称/版本/DLL 名/`format`/`extension`/描述/`fields` |
| `src/exporter_mcbbs.cpp` | `McbbsExporter` 实现 + `CreateExporter` 导出 + 日志注册宏 |

## 构建集成

- 目标类型：`add_library(NeoExporter_MCBBS SHARED ...)` → 产出 `NeoExporter_MCBBS.dll`。
- 导出符号（`extern "C" __declspec(dllexport)`，必须带 `__declspec`，否则 `GetProcAddress` 找不到）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter() {
      return new McbbsExporter();
  }
  ```

- 日志注册（可选演进约定）：`NEO_DECLARE_PLUGIN_LOG_SINK("NeoExporter_MCBBS")` 展开出 `SetPluginLogSink(ILogSink*)` 导出，宿主 `ModpackExporter::loadExporter` 用 `GetProcAddress` 注入；旧插件无此符号时自动跳过。
- `.meta.json` 与 DLL 一同由根 `CMakeLists.txt` 部署：`EXPORTER_TARGETS` 含 `NeoExporter_MCBBS`，`neo_deploy` POST_BUILD 将 `$<TARGET_FILE:...>` 与 `NeoExporter_MCBBS.meta.json` `copy_if_different` 到 `${DEPLOY_DIR}/exporters/`。
- VERSIONINFO：根 CMakeLists 通过 `nsum_add_version_info` 写入「NSUM 导出插件 (NeoExporter_MCBBS)」。

## 公共符号

| 符号 | 签名 | 说明 |
|------|------|------|
| `CreateExporter` | `extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter()` | 工厂：`new McbbsExporter()` |
| `SetPluginLogSink` | `extern "C" __declspec(dllexport) void SetPluginLogSink(ILogSink*)` | 宏展开，宿主日志注入点 |
| `McbbsExporter` | `class ... : public NeoCore::IModpackExporter`（匿名命名空间） | 实现类 |

**接口方法实现概览**（摘自 `NeoCore/include/IModpackExporter.h`）：

| 方法 | 实现要点 |
|------|----------|
| `std::string format_name() const` | 返回 `"mcbbs"` |
| `std::string file_extension() const` | 返回 `".zip"` |
| `std::string format_description() const` | 返回 `"MCBBS / PCL / HMCL 通用整合包格式"` |
| `NeoCore::BuildResult build_modpack(const BuildTarget&, IBuildProgress*, CancelToken*)` | 插件内 `BuildEngine` 构建，输出到 `target.output_path` |
| `bool export_modpack(build_dir, output_path, metadata)` | 打包 `.zip`（详见功能细节） |
| `nlohmann::json preview_structure(build_dir, metadata, target_dir="")` | 返回 `manifest.json`/`mcbbs.packmeta`/`overrides/...` 条目数组 |