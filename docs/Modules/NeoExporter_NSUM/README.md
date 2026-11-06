# NeoExporter_NSUM 说明文档

## 概述

`NeoExporter_NSUM` 是 NeoServerUpdateModpack 的导出插件（`NeoCore::IModpackExporter`），产出 **NSUM 整合包**（`<name>.zip`）。它是「服务端仓库 → 客户端 HMCL」的入口：包体很小，核心是清单 `nsum.index.json`，内容由客户端安装的 **NSUM Runtime** 在启动/手动同步时从仓库拉取。

- 读取 Meta：`NeoExporter_NSUM.meta.json`（`format: "nsum"`，`extension: ".zip"`）。
- 实现类：`NsumExporter`（匿名命名空间），导出工厂 `CreateExporter`。
- 与 HMCL 侧的配套模组：`FLFHMCL` 仓库的 `extensions/nsum-bridge`（注册整合包类型 `nsum`、启动方式 `nsum-runtime`、NSUM GUI 页）。

## 包格式

```
<name>.zip
  ├── nsum.index.json   ← 清单（连接信息 + 版本信息 + `files[]` 文件索引）
  └── overrides/          ← 构建产物（首装兜底内容；不含 save/）
```

清单字段（由 `buildManifest()` 生成，`meta.extra` 提供连接信息）：

| 字段 | 来源 | 说明 |
|------|------|------|
| `schema` / `type` | 常量 | `1` / `"nsum"` |
| `name` / `author` / `version` | `ExportMetadata` | 展示与版本 |
| `gameVersion` / `modloader` / `modloaderVersion` | `ExportMetadata` | 游戏与加载器 |
| `description` | `ExportMetadata.description/summary` | 描述 |
| `instance.repo` | `extra.repo` | 仓库地址（CLI 记录**用户给定的原始仓库地址**，避免客户端同步到本地缓存路径） |
| `instance.branch` | `extra.branch` | 整合包分支 |
| `instance.gitBranch` | `extra.git_branch` | git 分支（checkout 用） |
| `runtime.minVersion` | `extra.runtime_min_version`（默认 `1.0.0`） | Runtime 最低版本要求 |

## 模块边界

**做什么**

- `build_modpack`：委托 `NeoBuild::BuildEngine::build(branch, progress, cancel, git_branch)` 完成构建（含 L1/L2/mods 镜像/L3 存档规则处理）。
- `export_modpack`：写 `nsum.index.json` + 把构建产物打进 `overrides/`（**排除 `save/`**——它是 L3 规则与源，不属于包内容）。
- `preview_structure`：列出 `nsum.index.json` 与 `overrides/**`。

**不做什么**

- 不打包游戏本体/加载器。
- 不写 HMCL 的 `modpack.cfg` / `hmclversion.cfg`（客户端侧由 HMCL 自己管理）。
- 不实现客户端同步（那是 NSUM Runtime + nsum-bridge 的职责）。

## 依赖

| 依赖 | 类型 | 用途 |
|------|------|------|
| `NeoCore` | 静态库 | `IModpackExporter` / `ExportMetadata` / `BuildTarget` |
| `NeoBuild` | 静态库 | `BuildEngine` |
| `libzippp::libzippp` | vcpkg | zip 打包 |
| `Qt6::Core` | Qt6 | 路径/文件遍历 |

## 构建集成

根 `CMakeLists.txt` 已登记：`add_subdirectory(modules/NeoExporter_NSUM)`、`EXPORTER_TARGETS`、部署与版本资源循环；`neo_deploy` 会把 `NeoExporter_NSUM.dll` 与 `.meta.json` 复制到 `deploy/exporters/`。

## 调用方

- CLI：`exec build --format nsum --export <zip 路径>`（`fillExportMetadata` 会写入 `repo`，CLI 再补 `branch` / `git_branch`）。
- GUI：导出类型页读取 exporter meta 自动出现「NSUM 整合包 (.zip)」；构建页填充同样的 `extra`。
