# NeoExporter_Modrinth 使用文档

## 快速开始

### 构建

`NeoExporter_Modrinth` 是标准插件 DLL，随主工程 `neo_deploy` 目标自动构建并部署：

```powershell
$cmake = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
```

### 部署

产物 `NeoExporter_Modrinth.dll` 与 `NeoExporter_Modrinth.meta.json` 由根 `CMakeLists.txt`（`EXPORTER_TARGETS` → `neo_deploy` POST_BUILD）复制到：

```
<build>/deploy/exporters/NeoExporter_Modrinth.dll
<build>/deploy/exporters/NeoExporter_Modrinth.meta.json
```

运行期由 `NeoBuild::ModpackExporter::scanExporters(exportersDir)` 扫描加载（见「典型用法」）。

## 插件契约

### 接口实现要点（`NeoCore/include/IModpackExporter.h`）

实现类 `ModrinthExporter`（匿名命名空间内）逐字实现：

| 接口方法 | 实现要点 |
|----------|----------|
| `std::string format_name() const override` | `"modrinth"` |
| `std::string file_extension() const override` | `".mrpack"` |
| `std::string format_description() const override` | `"Modrinth Modpack Format (.mrpack)"` |
| `NeoCore::BuildResult build_modpack(const NeoCore::BuildTarget& target, NeoCore::IBuildProgress* progress, NeoCore::CancelToken* cancel) override` | 与 MCBBS 插件同构：`target.workspace_json` 为空拼 `workspace_path/workspace.json`；`engine.init(wsJson, target.cache_dir, target.output_path)`；失败返回 `{success=false, errorMessage="Failed to initialize build engine"}`；成功 `engine.build(target.branch, progress, cancel)` |
| `bool export_modpack(const std::string& build_dir, const std::string& output_path, const NeoCore::ExportMetadata& metadata) override` | 扫描哈希 + 组装 index + 写 zip（详见功能细节） |
| `nlohmann::json preview_structure(const std::string& build_dir, const NeoCore::ExportMetadata& metadata, const std::string& target_dir = "") override` | `(void)target_dir`；返回条目数组 |

`ExportMetadata` 字段（接口原文，本插件消费的键）：`name`、`version`、`author`、`game_version`、`modloader`、`modloader_version`、`summary`、`description`、`language_files`、`extra`。

### meta.json 字段逐字段说明（`NeoExporter_Modrinth.meta.json`）

| 字段 | 值 | 说明 |
|------|-----|------|
| `name` | `"Modrinth Modpack Exporter"` | 显示名 |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoExporter_Modrinth.dll"` | 声明 DLL 文件名（加载器由 `.meta.json` 文件名推断 DLL 路径，不读此字段 [实现为纯描述]） |
| `format` | `"modrinth"` | 格式键（GUI 导出页读取） |
| `extension` | `".mrpack"` | 文件名后缀（GUI 导出页读取） |
| `description` | `"Modrinth Modpack Format (.mrpack)"` | 描述；**加载器会读取此键覆盖实例的 `format_description()`** |
| `fields` | 见下表 | 额外字段声明：GUI 向导与 CLI `flow console` 收集后写入 `ExportMetadata.extra` |

`fields` 数组逐项：

| key | label | required | group | type/placeholder | 说明 |
|-----|-------|----------|-------|------------------|------|
| `name` | `整合包名称` | `true` | `基本信息` | `placeholder: "例如: 我的整合包"` | 映射 `ExportMetadata.name`（mrpack `name`） |
| `version` | `版本号` | `true` | `基本信息` | `placeholder: "例如: 1.0.0"` | 映射 `ExportMetadata.version`（mrpack `versionId`） |
| `summary` | `简介` | `false` | `基本信息` | `placeholder: "Modrinth 展示用简介"` | 映射 `ExportMetadata.summary`（mrpack `summary`） |
| `author` | `作者` | `false` | `可选信息` | `placeholder: "作者名称"` | 映射 `ExportMetadata.author` |
| `description` | `详细描述` | `false` | `可选信息` | `"type": "multiline"`，`placeholder: "详细描述 (可选)"` | 映射 `ExportMetadata.description`（summary 为空时的后备） |

## 功能细节

### 打包结构（`.mrpack` = zip）

```
<output>.mrpack
├── modrinth.index.json    # zip 根：mrpack 索引（addData 写入）
└── overrides/             # 构建目录全部文件（packOverridesToZip 写入）
    ├── <文件/目录> ...
```

注意：**不生成 `client-overrides/` 与 `server-overrides/`**（背景所述可选分隔未实现 [以代码为准，若需支持需扩展 `packOverridesToZip`/index 逻辑]）。

### modrinth.index.json 生成规则（`buildModrinthIndex`，`index.dump(2)` 写入）

```json
{
  "formatVersion": 1,
  "game": "minecraft",
  "versionId": "<meta.version>",
  "name": "<meta.name>",
  "summary": "<meta.summary>，为空则取 <meta.description>",
  "files": [
    {
      "path": "<构建目录相对路径，正斜杠>",
      "hashes": { "sha1": "<小写十六进制>", "sha512": "<小写十六进制>" },
      "downloads": [],
      "fileSize": <字节数>
    }
  ],
  "dependencies": {
    "minecraft": "<meta.game_version>",
    "<loaderId>": "<meta.modloader_version>"
  }
}
```

- `files`：`scanBuildDirForIndex(build_dir, "overrides")` 遍历构建目录所有**文件**（跳过目录），`computeHashes` 流式计算 SHA-1+SHA-512（`kHashBufferSize = 65536`，十六进制小写）；哈希失败记 Warn `cannot hash file ... skipping` 并跳过该文件（此时文件本体仍会被打入 overrides——[潜在不一致点，见注意事项]）。
- `dependencies`：`minecraft` 恒写入 `meta.game_version`；加载器键 = `mapModloaderId(meta.modloader)`，仅当 `modloader_version` 非空才写入。

### mapModloaderId 映射

| 输入（meta.modloader） | 输出（dependencies 键） |
|------------------------|------------------------|
| `"fabric"` | `"fabric-loader"` |
| `"quilt"` | `"quilt-loader"` |
| `"forge"` | `"forge"` |
| `"neoforge"` | `"neoforge"` |
| 其它 | 原样透传 |

### overrides 写入（`packOverridesToZip`）

遍历构建目录所有文件，entry 名 = `"overrides/" + rel`（`rel` 做了 `'\\'→'/'` 规范化）；**不写空目录条目**（与 MCBBS 插件的 `addEmptyDirToZip` 行为不同）。

### 前置校验与清理

与 MCBBS 插件一致：`build_dir`/`output_path` 非空 → `build_dir` 存在且为目录 → 输出父目录 `mkpath` → 已存在则 `remove` → `libzippp::ZipArchive::OpenMode::New` 打开 → 写 index → 写 overrides → `zf.close() != LIBZIPPP_OK` 判失败。

### preview_structure

返回 `{"path":"modrinth.index.json","dir":false,"umd":""}` + 每个文件/目录 `{"path":"overrides/<rel>","dir":<isDir>,"umd":""}`。

## 典型用法

### 宿主加载（`NeoBuild::ModpackExporter`）

```cpp
ModpackExporter exporter;
exporter.scanExporters(exportersDir);   // 扫 *.meta.json → 拼 *.dll → LoadLibrary → CreateExporter
std::vector<std::string> formats = exporter.availableFormats();   // 含 "modrinth"

NeoCore::ExportMetadata meta;                                 // 填 name/version/summary/...
bool ok = exporter.exportModpack("modrinth", buildDir, outPath, meta);

// 预览/构建入口
NeoCore::IModpackExporter* inst = exporter.exporterForFormat("modrinth");
nlohmann::json tree = inst->preview_structure(buildDir, meta);   // 不落盘预览
auto result = inst->build_modpack(target, progress, cancel);     // 插件内构建引擎
```

`scanExporters` 扫描规则与命名陷阱见 `docs/Modules/NeoExporter_MCBBS/usage.md`（两插件同构）；`LoadedExporter` 覆盖逻辑相同（meta 提供的 `format`/`extension` 键不参与加载器覆盖，仅 `description` 生效）。

## 注意事项

- **`__declspec(dllexport)` 必须保留**：`CreateExporter` 缺它则 DLL 零导出，`GetProcAddress` 报 `CreateExporter not found`（2026-08-06 全局修复教训）。修改后 `dumpbin /exports NeoExporter_Modrinth.dll` 验证。
- **✅ 已修复（2026-08-30）：哈希失败的文件不入包**：某个文件 `computeHashes` 失败（如权限/占用）时，该文件**既不进 index 也不进 overrides**——`packOverridesToZip` 只打包已成功入 index 的文件（validRels），并打 Warn 日志；mrpack 不再出现「文件在包内但 index 无记录」的不一致。
- **✅ 已修复（2026-08-30）：`downloads` 空数组不再写出**：`downloads` 为空时该字段不写入 mrpack（规格中的可选字段）；发布到 Modrinth 平台前由发布方回填上传后的下载 URL（当前实现不承担回填）。
- **只有 `overrides/`**：不支持 `client-overrides/`/`server-overrides/` 区分——客户端/服务端差异文件会全部进 `overrides/`。 [与 mrpack 规格的可选能力存在差距]
- **构建与打包是两阶段**：`build_modpack` 输出中间构建目录（`target.output_path`），`export_modpack` 再据此打包；宿主若只调 `export_modpack` 需要先有构建产物。
- **`preview_structure` 忽略 `target_dir`**：打包格式不做 U/M/D 比对，所有条目 `umd` 为 `""`。
- **日志通道**：源文件用 `CLogger::Error/Warn/Info`；宿主注入 `SetPluginLogSink` 后带 `[NeoExporter_Modrinth]` 前缀。

## 相关文档

- 接口契约：`docs/Modules/NeoCore/README.md`、`docs/Modules/NeoCore/usage.md`
- 构建引擎/宿主加载：`docs/Modules/NeoBuild/README.md`、`docs/Modules/NeoBuild/usage.md`（`ModpackExporter`/`BuildEngine`）
- 同族插件：`docs/Modules/NeoExporter_MCBBS/README.md`、`docs/Modules/NeoExporter_HMCL/README.md`（及各自 usage.md）
- 格式参考：https://support.modrinth.com/en/articles/8802351-modrinth-modpack-format-mrpack 、`docs/deploy/main/formats.md`、根 `CMakeLists.txt` 的 `EXPORTER_TARGETS` 部署段落