# NeoExporter_NSUM 说明文档

## 概述

`NeoExporter_NSUM` 是 NeoServerUpdateModpack 的导出插件（`NeoCore::IModpackExporter`），产出 **NSUM 整合包**（`<name>.nsumpack`）。它是「服务端仓库 → 客户端 HMCL」的入口：包体很小，核心是清单 `nsum.index.json`，内容由客户端安装的 **NSUM Runtime** 在启动/手动同步时从仓库拉取。

- 读取 Meta：`NeoExporter_NSUM.meta.json`（`format: "nsum"`，`extension: ".nsumpack"`）。
- 实现类：`NsumExporter`（匿名命名空间），导出工厂 `CreateExporter`。
- 与 HMCL 侧的配套模组：`FLFHMCL` 仓库的 `extensions/nsum-bridge`（注册整合包类型 `nsum`、启动方式 `nsum-runtime`、NSUM GUI 页）。

## 包格式（pack_format 2）

```
<name>.nsumpack
  ├── nsum.index.json     ← 清单（仓库/整合包/文件结构版本/来源提交/文件索引）
  └── overrides/**         ← 仅在显式声明 embed_patterns 时出现（可选附带文件）
```

默认**不内嵌任何文件**：包体只有清单（约 1 KB），客户端由 NSUM Runtime 按清单里的仓库/分支/提交同步。
`save/`（L3 存档规则与源）永不进入包体。

清单字段（由 `buildManifest()` 生成）：

| 字段 | 来源 | 说明 |
|------|------|------|
| `pack_format` / `schema` | 常量 | `2`（文件结构版本，不是整合包版本） |
| `type` | 常量 | `"nsum"` |
| `name` / `author` / `version` | `ExportMetadata` | 展示与整合包自身版本 |
| `gameVersion` / `modloader` / `modloaderVersion` / `core` | `ExportMetadata` / `extra.core` | 游戏与加载器/核心 |
| `description` | `ExportMetadata` | 描述 |
| `repository.url` | `extra.repo` | 仓库地址（CLI 记录用户给定的原始地址） |
| `repository.default_branch` | `extra.default_branch` | 仓库默认分支 |
| `repository.branch` | `extra.branch` | 整合包分支 |
| `repository.git_branch` | `extra.git_branch` | 构建时 Git 分支 |
| `repository.commit` | `extra.git_commit` | 构建时 Git 提交 ID（完整 SHA） |
| `files[]` | 构建产物扫描 | `path` / `size` / `sha256` / `commit` / `git_branch`（只登记，不含字节） |
| `embedded[]` | 正则命中文件 | `path` / `size` / `sha256` / `commit` / `git_branch` / `pattern`，同时写入 `overrides/` |
| `embed_patterns[]` | `extra.embed_patterns` | 显式声明的正则列表（每行一个） |
| `runtime.minVersion` | `extra.runtime_min_version` | Runtime 最低版本 |

### 内嵌文件（可选）

只有显式声明 `embed_patterns` 时才把命中文件写入 `overrides/`，并在 `embedded[]` 记录来源提交与分支：

```powershell
NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack base --git-branch main `
    --format nsum --embed-patterns '^config/.*\.json$' --export <输出.nsumpack>
```

- 正则按 ECMAScript 语法、对**仓库相对路径**做 `search`；多个模式可用换行或 `;` 分隔；
- 未命中的文件仍会出现在 `files[]` 中（只登记）；
- 客户端（bridge）把命中文件按 SHA-256 存入 `.hmcl/nsum/cache/files/`，跨整合包/实例复用，避免重复携带与传输。
## 模块边界

**做什么**

- `build_modpack`：委托 `NeoBuild::BuildEngine::build(branch, progress, cancel, git_branch)` 完成构建（含 L1/L2/mods 镜像/L3 存档规则处理）。
- `export_modpack`：写 `nsum.index.json` + 按 `embed_patterns` 把命中文件打进 `overrides/` 并登记 `embedded[]`（默认不内嵌；`save/` 永不进入包体）。
- `preview_structure`：列出 `nsum.index.json`（有 `embed_patterns` 时加 `overrides/**` 命中文件）。

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
