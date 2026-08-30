# NeoExporter_MCBBS 使用文档

## 快速开始

### 构建

`NeoExporter_MCBBS` 是标准插件 DLL，随主工程 `neo_deploy` 目标自动构建并部署：

```powershell
$cmake = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
```

### 部署

产物 `NeoExporter_MCBBS.dll` 与 `NeoExporter_MCBBS.meta.json` 由根 `CMakeLists.txt`（`EXPORTER_TARGETS` → `neo_deploy` POST_BUILD）复制到：

```
<build>/deploy/exporters/NeoExporter_MCBBS.dll
<build>/deploy/exporters/NeoExporter_MCBBS.meta.json
```

运行期由 `NeoBuild::ModpackExporter::scanExporters(exportersDir)` 扫描加载（见「典型用法」）。

## 插件契约

### 接口实现要点（`NeoCore/include/IModpackExporter.h`）

实现类 `McbbsExporter`（匿名命名空间内）逐字实现：

| 接口方法 | 实现要点 |
|----------|----------|
| `std::string format_name() const override` | `"mcbbs"`（加载器按此键索引，`findExporter(format)` 匹配） |
| `std::string file_extension() const override` | `".zip"` |
| `std::string format_description() const override` | `"MCBBS / PCL / HMCL 通用整合包格式"` |
| `NeoCore::BuildResult build_modpack(const NeoCore::BuildTarget& target, NeoCore::IBuildProgress* progress, NeoCore::CancelToken* cancel) override` | 内部构造 `NeoBuild::BuildEngine`；`target.workspace_json` 为空时拼 `workspace_path/workspace.json`；`engine.init(wsJson, target.cache_dir, target.output_path)` 失败 → 返回 `{success=false, errorMessage="Failed to initialize build engine"}`；成功 → `engine.build(target.branch, progress, cancel)` |
| `bool export_modpack(const std::string& build_dir, const std::string& output_path, const NeoCore::ExportMetadata& metadata) override` | 打包 `.zip`；任何前置校验/打包异常返回 `false` |
| `nlohmann::json preview_structure(const std::string& build_dir, const NeoCore::ExportMetadata& metadata, const std::string& target_dir = "") override` | `(void)target_dir`；返回条目数组 |

`ExportMetadata` 字段（接口原文，插件消费的键）：

```cpp
struct ExportMetadata {
    std::string name;
    std::string version;
    std::string author;
    std::string game_version;
    std::string modloader;
    std::string modloader_version;
    std::string summary;
    std::string description;
    std::vector<std::string> language_files;
    nlohmann::json extra;
};
```

### meta.json 字段逐字段说明（`NeoExporter_MCBBS.meta.json`）

| 字段 | 值 | 说明 |
|------|-----|------|
| `name` | `"MCBBS Modpack Exporter"` | 显示名 |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoExporter_MCBBS.dll"` | 声明 DLL 文件名（加载器由 `.meta.json` 文件名推断 DLL 路径，不读此字段 [实现为纯描述]） |
| `format` | `"mcbbs"` | 格式键（GUI 导出页 `export_page.cpp` 读取用于格式下拉） |
| `extension` | `".zip"` | 文件名后缀（GUI 导出页读取用于文件过滤器与自动补全） |
| `description` | `"MCBBS / PCL / HMCL 通用整合包格式"` | 描述；**加载器 `loadExporter` 会读取此键覆盖实例的 `format_description()`** |
| `fields` | 见下表 | **额外字段声明**：GUI 向导 `extra_info_page.cpp` 与 CLI `flow console` 按此收集用户输入，写入 `ExportMetadata.extra` |

`fields` 数组逐项：

| key | label | required | group | type/placeholder | 说明 |
|-----|-------|----------|-------|------------------|------|
| `name` | `整合包名称` | `true` | `基本信息` | `placeholder: "例如: 我的整合包"` | 映射 `ExportMetadata.name` |
| `version` | `版本号` | `true` | `基本信息` | `placeholder: "例如: 1.0.0"` | 映射 `ExportMetadata.version` |
| `author` | `作者` | `false` | `基本信息` | `placeholder: "作者名称"` | 映射 `ExportMetadata.author` |
| `description` | `整合包描述` | `false` | `可选信息` | `"type": "multiline"`，`placeholder: "整合包描述 (可选)"` | 映射 `ExportMetadata.description` |

> 注意：`game_version`/`modloader`/`modloader_version`/`summary` 不在 `fields` 中，由构建流程/分支配置提供。

## 功能细节

### 打包结构（文件清单树）

`export_modpack` 生成的 `.zip` 顶层结构：

```
<output>.zip
├── manifest.json          # zip 根：整合包清单（zip 内 addData 写入）
├── mcbbs.packmeta         # zip 根：MCBBS 元数据（zip 内 addData 写入）
└── overrides/             # 构建目录全部内容（递归扫描装入）
    ├── <构建目录内任意层级文件/目录> ...
```

**`manifest.json` 生成规则**（`buildManifest`，`json::dump(2)` 写入）：

```json
{
  "manifestType": "minecraftModpack",
  "manifestVersion": 1,
  "name": "<meta.name>",
  "version": "<meta.version>",
  "author": "<meta.author>",
  "overrides": "overrides",
  "minecraft": {
    "version": "<meta.game_version>",
    "modLoaders": [
      { "id": "<meta.modloader>-<meta.modloader_version>", "primary": true }
    ]
  },
  "files": [],
  "description": "<meta.summary> 或 <meta.description>（非空时覆盖 summary）"
}
```

要点：`files` **恒为空数组**（不写文件哈希清单）；`description` 先取 `meta.summary`，若 `meta.description` 非空则覆盖。

**`mcbbs.packmeta` 生成规则**（`buildPackMeta`）：

```json
{
  "pack": "<meta.name>",
  "version": "<meta.version>",
  "author": "<meta.author>",
  "export_time": "<QDateTime::currentDateTime().toString(Qt::ISODate)>"
}
```

**overrides 打包**（`scanAndPackDirectory`, entry 前缀 `"overrides"`）：递归 `QDirIterator`（`QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot`，`Subdirectories`）；相对路径由构建目录前缀剥离 + `'\\'→'/'` 规范化；空目录仅在其**确无子项**时以 `entry + "/"` 写入 zip 目录条目。

### 前置校验与清理

`export_modpack` 依次校验：`build_dir`/`output_path` 非空 → `build_dir` 存在且为目录 → 输出父目录不存在则以 `outDir.mkpath(".")` 创建（失败返回 false）→ 输出文件已存在则 `QFile::remove` 删除。归档以 `libzippp::ZipArchive::OpenMode::New` 打开，`zf.close() != LIBZIPPP_OK` 判定关闭失败。

### preview_structure

返回条目数组：先是 `{"path":"manifest.json","dir":false,"umd":""}`、`{"path":"mcbbs.packmeta","dir":false,"umd":""}`，随后遍历构建目录追加 `{"path":"overrides/<rel>","dir":<isDir>,"umd":""}`（`rel` 做 `'\\'→'/'` 规范化）。

## 典型用法

### 宿主加载（`NeoBuild::ModpackExporter`）

```cpp
ModpackExporter exporter;
exporter.scanExporters(exportersDir);   // 扫 *.meta.json → 拼 *.dll → LoadLibrary → CreateExporter
std::vector<std::string> formats = exporter.availableFormats();   // 含 "mcbbs"

// 方式一：完整"构建+打包"走宿主 exportModpack
NeoCore::ExportMetadata meta;                                 // 填 name/version/... 字段
bool ok = exporter.exportModpack("mcbbs", buildDir, outPath, meta);

// 方式二：先取插件实例做预览/构建（BuildEngine 的构建入口）
NeoCore::IModpackExporter* inst = exporter.exporterForFormat("mcbbs");
nlohmann::json tree = inst->preview_structure(buildDir, meta);   // 不落盘预览
auto result = inst->build_modpack(target, progress, cancel);     // 插件内构建引擎
```

`scanExporters` 扫描规则（`modpack_exporter.cpp`，**命名陷阱**）：仅处理 `*.meta.json` 后缀文件，DLL 路径 = 文件名的 `.meta.json` 后缀替换为 `.dll`（**不** `replace_extension`，否则拼成 `XXXX.meta.dll`）；DLL 缺失记 Warn `DLL not found for meta`。加载后 `LoadedExporter.format/extension/description` 默认取实例方法返回值，若 meta 含 `format_name`/`file_extension`/`description` 键则覆盖——实际 meta 只提供 `format`/`extension`/`description`，故只有 `description` 覆盖生效。

## 注意事项

- **`__declspec(dllexport)` 必须保留**：`CreateExporter` 缺它则 DLL 零导出，`GetProcAddress` 报 `CreateExporter not found`（2026-08-06 全局修复教训）。修改后 `dumpbin /exports NeoExporter_MCBBS.dll` 验证 `CreateExporter`/`SetPluginLogSink`。
- **构建与打包是两阶段**：`build_modpack` 输出中间构建目录（`target.output_path`），`export_modpack` 再据此打包；宿主若只调 `export_modpack` 需要先有构建产物。
- **`files` 恒为空**：某些平台按 `files` 列表做增量/校验，本格式依赖 overrides 文件本体冗余——这是当前实现的事实行为，非缺陷。
- **meta 键名不一致**：加载器读 `format_name`/`file_extension`，meta.json 提供的是 `format`/`extension`——改格式键时两者都要同步（加载器兜底用实例返回值，GUI 用 meta 值）。
- **`preview_structure` 忽略 `target_dir`**：MCBBS 为打包格式，不做 U/M/D 比对，所有条目 `umd` 为 `""`。
- **日志通道**：源文件用 `CLogger::Error/Info`；宿主注入 `SetPluginLogSink` 后带 `[NeoExporter_MCBBS]` 前缀。
- **构建目录末尾分隔符**：`scanAndPackDirectory` 会先剥除 `build_dir` 尾部 `/`、`\`，相对路径剥离后可安全得出 `overrides/<rel>`。

## 相关文档

- 接口契约：`docs/Modules/NeoCore/README.md`、`docs/Modules/NeoCore/usage.md`
- 构建引擎/宿主加载：`docs/Modules/NeoBuild/README.md`、`docs/Modules/NeoBuild/usage.md`（`ModpackExporter`/`BuildEngine`）
- 同族插件：`docs/Modules/NeoExporter_Modrinth/README.md`、`docs/Modules/NeoExporter_HMCL/README.md`（及各自 usage.md）
- 格式参考：`docs/deploy/main/formats.md`、根 `CMakeLists.txt` 的 `EXPORTER_TARGETS` 部署段落