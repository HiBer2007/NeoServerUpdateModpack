# NeoExporter_HMCL 使用文档

## 快速开始

### 构建

`NeoExporter_HMCL` 是标准插件 DLL，随主工程 `neo_deploy` 目标自动构建并部署：

```powershell
$cmake = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
```

### 部署

产物 `NeoExporter_HMCL.dll` 与 `NeoExporter_HMCL.meta.json` 由根 `CMakeLists.txt`（`EXPORTER_TARGETS` → `neo_deploy` POST_BUILD）复制到：

```
<build>/deploy/exporters/NeoExporter_HMCL.dll
<build>/deploy/exporters/NeoExporter_HMCL.meta.json
```

运行期由 `NeoBuild::ModpackExporter::scanExporters(exportersDir)` 扫描加载（见「典型用法」）。

## 插件契约

### 接口实现要点（`NeoCore/include/IModpackExporter.h`）

实现类 `HmclExporter`（全局命名空间）逐字实现：

| 接口方法 | 实现要点 |
|----------|----------|
| `std::string format_name() const override` | `"hmcl"` |
| `std::string file_extension() const override` | `"工作目录"` |
| `std::string format_description() const override` | `"HMCL 工作区同步（同步到游戏工作目录）"` |
| `NeoCore::BuildResult build_modpack(const NeoCore::BuildTarget& target, NeoCore::IBuildProgress* progress, NeoCore::CancelToken* cancel) override` | 见下方流程 |
| `bool export_modpack(const std::string& build_dir, const std::string& output_path, const NeoCore::ExportMetadata& metadata) override` | 参数全部 `(void)` 忽略，记 Error `HMCL is workspace sync mode: build writes directly to the target directory, zip export is not supported`，返回 `false` |
| `nlohmann::json preview_structure(const std::string& build_dir, const NeoCore::ExportMetadata& metadata, const std::string& target_dir = "") override` | `(void)metadata;` 返回 `NeoBuild::generateUmdStructure(build_dir, target_dir, nullptr, nullptr)` |

`build_modpack` 流程（实际代码）：

```cpp
if (target.output_path.empty()) {
    // r.success = false; r.errorMessage = "sync target directory is empty";
    return r;
}
NeoBuild::BuildEngine engine;
std::string wsJson = target.workspace_json;
if (wsJson.empty()) {
    wsJson = (fs::path(target.workspace_path) / "workspace.json").string();
}
std::string staging = target.staging_dir;
if (staging.empty()) {
    staging = (fs::path(target.cache_dir).parent_path() / "staging" / target.branch).string();
}
if (!engine.init(wsJson, target.cache_dir, staging)) {
    // r.success = false; r.errorMessage = "Failed to initialize build engine";
    return r;
}
engine.setTargetDir(target.output_path);
return engine.build(target.branch, progress, cancel);
```

要点：

- **`target.output_path` = 目标工作目录**（HMCL 工作区），非中间构建目录——空则直接失败。
- **staging 默认值** = `target.cache_dir` 的父目录下 `staging/<branch>`（`target.staging_dir` 非空则用显式值）；中间构建落在 staging，最终由 `BuildEngine` 内部 `stepSyncTarget` 按 sync_policies 同步到 `output_path`。
- `BuildTarget` 句柄语义（接口原文，本插件的 `sync_to_directory=true` 场景）：`output_path: sync_to_directory=false: 中间构建目录；true: 目标工作目录（同步目标）`；`staging_dir: sync_to_directory=true: 中间构建目录（source），空 = 插件默认`。

### meta.json 字段逐字段说明（`NeoExporter_HMCL.meta.json`）

| 字段 | 值 | 说明 |
|------|-----|------|
| `name` | `"HMCL 工作区同步"` | 显示名 |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoExporter_HMCL.dll"` | 声明 DLL 文件名（加载器由 `.meta.json` 文件名推断 DLL 路径，不读此字段 [实现为纯描述]） |
| `format` | `"hmcl"` | 格式键（GUI 导出页读取） |
| `extension` | `"工作目录"` | GUI 导出页以此作文件过滤器后缀——**非真实扩展名**，其自动补全逻辑 `path.endsWith(ext)` 对中文后缀不生效 |
| `description` | `"HMCL 工作区同步（同步到游戏工作目录）"` | 描述；**加载器会读取此键覆盖实例的 `format_description()`** |
| `fields` | （无此键） | 本插件不声明额外字段，向导 extra-info 页/CLI flow console 对此格式不收集额外输入 |

## 功能细节

### HMCL 工作区同步（构建即导出）

本插件没有「打包」阶段：`build_modpack` 走完 `BuildEngine::build` 全流程（clone/fetch → checkout → merge → files → configs → custom mods → serverconfig → finalize）后，由引擎内部 `stepSyncTarget` 依据分支 sync_policies 将构建结果同步进目标工作目录。因此：

- 宿主若按打包格式流程（先 `build_modpack` 再 `export_modpack`）调用，**第二步必然失败**——工作区同步模式应只调用 `build_modpack`（或走 `BuildEngine.build` 直通）。
- `export_modpack` 返回 `false` 是有意设计，用于向宿主表明「该格式无 zip 产物」。

### preview_structure 与 UMD

`preview_structure(build_dir, metadata, target_dir)` 委托：

```cpp
return NeoBuild::generateUmdStructure(build_dir, target_dir, nullptr, nullptr);
```

签名（`NeoBuild/include/umd_generator.h` 原文）：

```cpp
nlohmann::json generateUmdStructure(
    const std::string& buildDir,
    const std::string& targetDir,
    NeoCore::IBuildProgress* progress = nullptr,
    NeoCore::CancelToken* cancel = nullptr);
```

返回条目 `[{"path","dir","umd"}]`，`umd ∈ {"", "U", "M", "D"}`：

| umd | 含义（头文件注释原文） |
|-----|------------------------|
| `""` | 未更改 |
| `"U"` | 目标目录不存在该文件（新建） |
| `"M"` | 内容不同（修改） |
| `"D"` | 目标有而构建无（删除） |

`targetDir`（即 `BuildTarget.output_path`）非空时做**真实比对**；为空则不比对、全部未更改。与此相对，MCBBS/Modrinth 插件 `preview_structure` 的 `umd` 恒为 `""`。

## 典型用法

### 宿主加载（`NeoBuild::ModpackExporter`）

```cpp
ModpackExporter exporter;
exporter.scanExporters(exportersDir);   // 扫 *.meta.json → 拼 *.dll → LoadLibrary → CreateExporter
std::vector<std::string> formats = exporter.availableFormats();   // 含 "hmcl"

// HMCL 工作区同步：只走 build_modpack（构建+同步一体），不要调 export_modpack
NeoCore::IModpackExporter* inst = exporter.exporterForFormat("hmcl");
NeoCore::BuildTarget target;
target.output_path = "D:/HMCL/.minecraft/versions/我的整合包";   // 目标工作目录
target.cache_dir   = "D:/NSUM/workspaces/<repo>/.NSUM/cache";
target.staging_dir = "";                                          // 空 → 默认 cache 父目录/staging/<branch>
target.branch      = "release";

NeoCore::BuildResult r = inst->build_modpack(target, progress, cancel);

// 预览同步差异（需先有 buildDir 产物）
nlohmann::json tree = inst->preview_structure(buildDir, meta, target.output_path);
```

### 导出页/向导接入

GUI 导出页（`export_page.cpp`）按 meta 读取 `format`/`extension`/`description` 展示「HMCL 工作目录」选项；由于无 `fields`，extra-info 页对该格式不出现额外字段输入；flow console 的 hmcl 导出目录保持原样（不追加扩展名）。

## 注意事项

- **`__declspec(dllexport)` 必须保留**：`CreateExporter` 缺它则 DLL 零导出，`GetProcAddress` 报 `CreateExporter not found`（2026-08-06 全局修复教训）。修改后 `dumpbin /exports NeoExporter_HMCL.dll` 验证。
- **`export_modpack` 恒失败是特性不是 bug**：任何「先构建后打包」的宿主流程要按格式分流——`hmcl` 只调 `build_modpack`；`mcbbs`/`modrinth` 才走 `export_modpack`。
- **`file_extension() == "工作目录"`**：非扩展名面值；若宿主依赖 `file_extension()` 拼文件名/过滤器需特判 `hmcl`（GUI 导出页的 `.endsWith(ext)` 自动补全对此值不适用）。
- **staging 目录默认位置**：`<cache_dir 父目录>/staging/<branch>`——确认 `cache_dir` 存在父目录语义；显式给 `target.staging_dir` 可覆盖。
- **output_path 必须非空**：空串直接失败（`sync target directory is empty`），与打包插件（output_path 为中间目录可默认）行为不同。
- **冗余依赖**：链接 `Qt6::Core`/`libzippp::libzippp` 但源码未直接使用 zip（与同族插件链接清单对齐）；保留或精简均可，改动后 clean-first 重建。
- **实现类不在匿名命名空间**：`HmclExporter` 定义在全局命名空间（`namespace` 未包裹），与其它导出器（匿名命名空间）不同——不影响功能，但链接可见性略宽。
- **日志通道**：`build_modpack` 内部由 `BuildEngine` 经 `reportProgress`/`CLogger` 输出；`export_modpack` 的拒绝日志带 `[NeoExporter_HMCL]` 前缀（注入后）。
- **虚拟构建预览**：走 `preview_structure` 前需先有 buildDir 产物（`engine.build(...)` 生成）；UMD 判定以目标目录真实比对为准，目标目录不存在时全部归 `"U"`。

## 相关文档

- 接口契约：`docs/Modules/NeoCore/README.md`、`docs/Modules/NeoCore/usage.md`
- 构建引擎/同步：`docs/Modules/NeoBuild/README.md`、`docs/Modules/NeoBuild/usage.md`（`ModpackExporter`/`BuildEngine::setTargetDir`/`stepSyncTarget`/`generateUmdStructure`）
- 同族插件：`docs/Modules/NeoExporter_MCBBS/README.md`、`docs/Modules/NeoExporter_Modrinth/README.md`（及各自 usage.md）
- 部署：`docs/Modules/README.md`（总索引）、根 `CMakeLists.txt` 的 `EXPORTER_TARGETS` 部署段落