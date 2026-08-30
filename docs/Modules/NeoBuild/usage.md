# NeoBuild 使用文档

> 本文所有签名逐字摘自 `modules/NeoBuild/include/` 头文件；行为说明以 `src/` 实现为准。进度/结果/取消/插件接口类型（`NeoCore::*`）定义于 `modules/NeoCore/include/`。

## 快速开始

NeoBuild 是 STATIC 库，include 目录经 `target_include_directories(PUBLIC include)` 传递：

```cmake
add_subdirectory(modules/NeoBuild)          # 根 CMakeLists 已挂载；自建工程按需添加

target_link_libraries(my_target PRIVATE NeoBuild)
# PUBLIC 依赖（NeoCore/NeoWorkspace/Qt6::Core/Qt6::Network/cpr/libzippp）随目标传递
```

代码中直接包含头文件并处于 `NeoBuild` 命名空间：

```cpp
#include <build_engine.h>
#include <pointer_downloader.h>
#include <modpack_exporter.h>
#include <serverconfig_sync.h>
// ... 均位于 namespace NeoBuild，类型别名 BuildProgress/BuildResult 同命名空间
```

## 公共 API

### 1. build_engine.h — 构建引擎

类型别名：

```cpp
using BuildProgress = NeoCore::BuildProgress;
using BuildResult    = NeoCore::BuildResult;
```

`class BuildEngine`：

| 方法 | 签名（逐字） | 说明 |
|------|------|------|
| `init` | `bool init(const std::string& workspacePath, const std::string& cacheDir = "", const std::string& outputBaseDir = "", const std::string& exportersDir = "")` | 初始化。`workspacePath` 可传目录或 `workspace.json` 文件路径（自动取所在目录）；`cacheDir`/`outputBaseDir` 为空则默认 `<workspace>/.cache`、`<workspace>/output`；`exportersDir` 非空才调用 `scanExporters` 扫描导出插件（预览/导出必需） |
| `build` | `BuildResult build(const std::string& branchName, NeoCore::IBuildProgress* progress = nullptr, NeoCore::CancelToken* cancelToken = nullptr, const std::string& gitBranch = "")` | 完整构建。`gitBranch` 空则用 `branchName` 做 checkout；`progress`/`cancelToken` 可为空（静默降级为无 UI 构建） |
| `stepCloneOrFetch` | `bool stepCloneOrFetch()` | clone/fetch；本地仓库（无 remote）跳过 fetch，fetch 失败仅告警不误报 |
| `stepCheckout` | `bool stepCheckout(const std::string& branch)` | git checkout 指定分支 |
| `stepMergeBranches` | `bool stepMergeBranches(const std::string& targetBranch)` | 按继承链合并到 outputDir |
| `stepProcessFiles` | `bool stepProcessFiles(const std::string& branchName)` | 扫描并处理 `.pointer` 文件与 branch_config 指针清单（缓存命中→复制，未命中→下载→落位） |
| `stepMergeCustomMods` | `bool stepMergeCustomMods(const std::string& branchName)` | `<workspace>/custom/mods` → `outputDir/mods`（覆盖写） |
| `stepSyncServerConfigs` | `bool stepSyncServerConfigs()` | 目标工作目录 `saves/` 存在且分支有规则时执行 serverconfig 同步（见第 9 节） |
| `stepFinalize` | `bool stepFinalize()` | 生成 `version.json` 与 `hmclversion.cfg` |
| `stepSyncTarget` | `bool stepSyncTarget(NeoCore::IBuildProgress* progress, NeoCore::CancelToken* cancelToken, NeoCore::BuildResult& result)` | 按 `sync_policies` 将构建目录同步到目标工作目录（`setTargetDir` 后自动调用） |
| `exportModpack` | `bool exportModpack(const std::string& format, const std::string& outputPath, const NeoCore::ExportMetadata& metadata)` | 将 `outputDir` 按指定格式打包导出 |
| `previewStructure` | `nlohmann::json previewStructure(const std::string& format, const NeoCore::ExportMetadata& metadata, const std::string& targetDir = "")` | 模拟最终归档结构（不写任何文件）；`targetDir` 为 HMCL 真实比对基准，空 = 全部未更改 |
| `cacheDir` | `std::string cacheDir() const` | 缓存目录访问器 |
| `outputDir` | `std::string outputDir() const` | 输出目录访问器 |
| `workspace` | `const NeoWorkspace::WorkspaceManager* workspace() const` | 工作区管理器（只读） |
| `setGitPath` | `void setGitPath(const std::string& path)` | 注入 git 可执行路径（默认走环境探测） |
| `setTargetDir` | `void setTargetDir(const std::string& dir)` | HMCL 工作区同步目标目录；空 = 跳过同步，走打包格式 |
| `targetDir` | `const std::string& targetDir() const` | 同步目标目录访问器 |
| `mergedFileManifest` | `const std::unordered_map<std::string, std::string>& mergedFileManifest() const` | 分支级文件清单（file_manifest/pointer_files 顶层 + 分支合并结果，分支优先） |
| `mergedPointerFiles` | `const std::unordered_map<std::string, NeoCore::PointerInfo>& mergedPointerFiles() const` | 分支级指针文件集（key = sha256） |

**构建阶段进度**（`reportProgress` 双写日志 + `IBuildProgress`）：`init 0%` → `clone 5%` → `checkout 15%` → `merge 25%` → `download 40%` → `sync_config 60%` → `custom_mods 75%` → `server_config 85%` → `finalize 95%` → `sync_target 97%`（仅 `setTargetDir` 后）→ `done 100%`。

**被引用类型（NeoCore）**：

| 类型 | 定义（逐字） |
|------|------|
| `struct BuildProgress` | `std::string stage; int percent = 0; std::string message;` |
| `struct BuildResult` | `bool success = false; std::string outputDir; std::string errorMessage; std::vector<std::string> warnings; int totalFiles = 0; int syncedFiles = 0; int failedFiles = 0;` |
| `class IBuildProgress` | 纯虚接口：`set_main_stage/progress/message`、`add_sub_bar`(返回句柄 `>0`)/`remove_sub_bar`/`set_sub_progress`/`set_sub_info`、`log`、`is_cancelled`；容错约定：允许空指针、无效句柄静默忽略、实现方不得抛异常 |
| `class CancelToken` | `void request_cancel()` / `bool is_cancelled() const` / `void reset()` |

### 2. branch_merger.h — 分支合并

```cpp
struct BranchLayer {
    std::string name;
    std::string baseDir;
    std::string overridesDir;               // .overrides/ subdirectory
    NeoWorkspace::BranchManifest manifest;  // branch_manifest.json
};

struct MergeResult {
    bool success = false;
    std::vector<std::string> mergedFiles;
    std::vector<std::string> overriddenFiles;
    std::vector<std::string> deletedFiles;
    std::string message;
};
```

| 方法 | 签名（逐字） | 说明 |
|------|------|------|
| `merge` | `MergeResult merge(const std::vector<BranchLayer>& layers, const std::string& outputDir, bool overwriteChild = true, NeoCore::CancelToken* cancelToken = nullptr)` | 按 layers 顺序层叠合并（后层覆盖前层；manifest delete 标记删除目标文件，override 标记改用 `.overrides/` 文件） |
| `mergeDirectories` | `MergeResult mergeDirectories(const std::string& parentDir, const std::string& childDir, const std::string& outputDir, const std::string& manifestPath = "", NeoCore::CancelToken* cancelToken = nullptr)` | parent + child 两目录合并 |
| `loadManifest` | `static NeoWorkspace::BranchManifest loadManifest(const std::string& dir)` | 读 `<dir>/branch_manifest.json`（缺失/解析失败返回空 manifest） |
| `saveManifest` | `static void saveManifest(const std::string& dir, const NeoWorkspace::BranchManifest& manifest)` | 写 `<dir>/branch_manifest.json`（缩进 2） |

### 3. mod_metadata.h — JAR modId 提取

```cpp
// 从 JAR 包提取 modId（加载器优先级: NeoForge > Forge mods.toml >
// Fabric fabric.mod.json > Forge 旧版 mcmod.info），失败返回空列表。
std::vector<std::string> extractModIds(const std::string& jarPath);
```

### 4. modpack_exporter.h — 导出插件管理

```cpp
class ModpackExporter {
public:
    void scanExporters(const std::string& exportersDir);
    std::vector<std::string> availableFormats() const;
    std::string formatDescription(const std::string& format) const;
    bool exportModpack(const std::string& format,
        const std::string& buildDir,
        const std::string& outputPath,
        const NeoCore::ExportMetadata& metadata,
        NeoCore::CancelToken* cancelToken = nullptr);
    NeoCore::IModpackExporter* exporterForFormat(const std::string& format);
    const NeoCore::IModpackExporter* exporterForFormat(const std::string& format) const;
    nlohmann::json previewStructure(const std::string& format,
        const std::string& buildDir,
        const NeoCore::ExportMetadata& metadata,
        const std::string& targetDir = "");
};
```

- `scanExporters`：遍历目录中 `*.meta.json`，**剥离 `.meta.json` 后缀后拼同名 `.dll`**（Linux/macOS 回退 `.so`/`.dylib`），`LoadLibrary` → `GetProcAddress("CreateExporter")` → 可选 `GetProcAddress("SetPluginLogSink")` 注入宿主日志 sink；meta JSON 的 `format_name`/`file_extension`/`description` 覆盖插件默认值。
- `exporterForFormat`：返回裸指针，生命周期归 `ModpackExporter` 所有，`scanExporters` 之后稳定。
- `previewStructure` 返回 `[{"path","dir","umd"}]` JSON 数组，不写文件。

### 5. platform_api.h — 平台 API

| 函数（逐字） | 说明 |
|------|------|
| `std::string getAppDataDir();` | AppData 目录 |
| `std::string getCacheDir();` | 缓存目录 |
| `std::string getConfigDir();` | 配置目录 |
| `std::string getTempDir();` | 系统临时目录 |
| `std::string getDefaultWorkspaceDir();` | 默认工作区目录 |
| `bool isWindows(); bool isLinux(); bool isMacOS();` | 平台判定 |
| `std::string platformName();` | 平台名 |
| `std::string findGitExecutable();` | 定位 git 可执行文件 |
| `bool isGitAvailable();` | git 是否可用 |
| `std::string getFreeDiskSpace(const std::string& path);` | 磁盘剩余空间（字符串） |
| `uint64_t getFreeDiskBytes(const std::string& path);` | 磁盘剩余空间（字节） |

### 6. pointer_downloader.h — 指针解析与下载

```cpp
struct DownloadProgress {
    std::string sha256;
    int64_t bytesDownloaded = 0;
    int64_t totalBytes = -1;
    bool completed = false;
    std::string error;
};

struct DownloadResult {
    bool success = false;
    std::string cachedPath;
    std::string sha256;
    std::string errorMessage;
};

struct ResolveResult {
    bool success = false;
    std::string url;
    std::string resolver;
    std::string errorMessage;
};
```

> ⚠️ `ResolveResult` 是**命名空间级** struct（`NeoBuild::ResolveResult`），不是 `PointerDownloader` 的嵌套类型（见注意事项 4）。

`class PointerDownloader`：

| 方法 | 签名（逐字） | 说明 |
|------|------|------|
| `registerResolver` | `void registerResolver(std::unique_ptr<NeoCore::IPluginPointer> resolver)` | 注册解析器（同名去重） |
| `scanResolvers` | `void scanResolvers(const std::string& pointersDir)` | 扫描目录中 `.dll`/`.so`/`.dylib` 加载 `CreatePointer` 插件 |
| `download` | `DownloadResult download(const NeoCore::PointerInfo& pointer, const std::string& cacheDir, std::function<void(const DownloadProgress&)> progressCallback = nullptr, NeoCore::CancelToken* cancelToken = nullptr)` | 单文件下载（含缓存命中与零信任校验） |
| `downloadAll` | `std::vector<DownloadResult> downloadAll(const std::vector<NeoCore::PointerInfo>& pointers, const std::string& cacheDir, std::function<void(const DownloadProgress&)> progressCallback = nullptr, NeoCore::CancelToken* cancelToken = nullptr)` | 串行批量下载 |
| `isCached` | `bool isCached(const std::string& cacheDir, const std::string& sha256) const` | 缓存存在**且校验通过**才返回 true |
| `cachePath` | `std::string cachePath(const std::string& cacheDir, const std::string& sha256) const` | `<cacheDir>/<sha256>` |
| `resolveUrl` | `ResolveResult resolveUrl(const NeoCore::PointerInfo& pointer) const` | 仅解析 URL，不下载 |

**下载 / 缓存 / 零信任校验行为**（实现依据 pointer_downloader.cpp）：

1. 缓存路径 = `<cacheDir>/<sha256>`；`sha256` 为空直接失败。
2. 缓存命中后**仍重新计算文件 SHA-256**（`validateFile`，64KB 分块），匹配即复用；不匹配删除缓存并重新下载（零信任原则：每次使用缓存前必须重算校验）。
3. 未命中 → 按 `can_handle` 找到解析器 → `resolve_url` 得到 URL → 下载到缓存路径（Qt `QNetworkAccessManager`，UA `NeoServerUpdateModpack/1.0 (NeoServer)`，重定向 `NoLessSafeRedirectPolicy`，传输/强制超时均 300s，取消经 100ms 轮询 `abort`）。
4. 下载完成后**再次校验 SHA-256**，失败删除临时文件；下载失败/取消同样清理。
5. 进度回调在下载过程中推送 `DownloadProgress`（含 `completed` 标记）。

### 7. sync_policy_executor.h — 同步策略执行器

```cpp
class SyncPolicyExecutor {
public:
    struct Result {
        bool success = true;
        int copiedFiles = 0;
        int mergedFiles = 0;
        int skippedFiles = 0;
        int deletedFiles = 0;
        int customHarvested = 0;
        int customRestored = 0;
        std::vector<std::string> warnings;
    };

    Result execute(
        const std::string& sourceDir,
        const std::string& targetDir,
        const NeoWorkspace::SyncPolicy& policy,
        NeoCore::IBuildProgress* progress = nullptr,
        NeoCore::CancelToken* cancel = nullptr);
};
```

层次语义（头文件注释）：

- **L1 文件夹策略**（`sync_policies.folders`，最长前缀匹配，递归含子文件夹）：`skip` / `mirror`（严格镜像：清空重写 + 删多余）/ `incremental_add`（只补缺失）/ `incremental_overwrite`（保留多余项，被改过也写入）/ `default`（兜底）。
- **L2 配置文件特化**（`sync_policies.files`）：`full`（仅覆盖）/ `partial`（半同步 merge，经 `IConfigParser`，`tracked_keys` 为空时取全部键）/ `ignore`。
- **mods 目录为 mirror 时特殊处理**：`.disabled` 清除、custom 模组收集到 `.NSUM/custom/mod`（只增不覆盖）、同步后复制回 mods、modId 冲突检测（保留镜像模组，冲突模组留在库中 + 警告）。
- `.NSUM/hashes.json` 记录每次写入文件的源 SHA-256，供增量策略比对。

### 8. umd_generator.h — U/M/D 虚拟构建预览

```cpp
// 生成 U/M/D 虚拟构建预览结构（供构建清单页文件树展示）。
//   buildDir:  虚拟构建目录（已生成文件的中间目录）
//   targetDir: 目标工作目录（hmcl 真实比对基准；为空 = 不做比对，全部未更改）
//   progress/cancel: 可空指针，静默降级
// 返回 JSON 数组: [{"path","dir","umd"}]，umd ∈ {"", "U", "M", "D"}
//   U = 目标目录不存在该文件（新建）；M = 内容不同（修改）；D = 目标有而构建无（删除）；"" = 未更改
nlohmann::json generateUmdStructure(
    const std::string& buildDir,
    const std::string& targetDir,
    NeoCore::IBuildProgress* progress = nullptr,
    NeoCore::CancelToken* cancel = nullptr);

// 分支继承层叠合并版：buildSet 由 BranchLayer 链在内存中虚拟合并
// （层叠覆盖 + delete/override 标记应用，不落盘），供 IDE 输出树展示最终结果
nlohmann::json generateUmdStructureFromLayers(
    const std::vector<BranchLayer>& layers,
    const std::string& targetDir,
    NeoCore::IBuildProgress* progress = nullptr,
    NeoCore::CancelToken* cancel = nullptr);
```

### 9. serverconfig_sync.h — serverconfig 规则同步

```cpp
struct ServerConfigEntry {
    std::string worldName;
    std::string configPath;
    std::string relativePath;
    std::string content;
};

enum class ServerConfigMode {
    Full,      // full: 应用本层设置 (遵守 save/[save]/serverconfig 文件夹同步模式 folder_mode)
    Force,     // force: 强制覆盖 (与 config 同步逻辑统一)
    Partial,   // partial: 半同步 merge (IConfigParser 追踪键, tracked_keys)
    Ignore     // ignore: 不碰
};

enum class ServerConfigFolderMode {
    Skip,                // skip: 不处理
    Mirror,              // mirror: 严格覆盖
    IncrementalAdd,      // incremental_add: 只补缺失, 已存在不动
    IncrementalOverwrite // incremental_overwrite: 保留多余项, 被改过也写入
};
```

| 方法 | 签名（逐字） | 说明 |
|------|------|------|
| `init` | `bool init(const std::string& savesDir, const std::string& repoRoot, const std::string& branchName)` | 初始化；规则目录 = `<repoRoot>/branches/<branchName>/save/[save]/serverconfig/.rule` |
| `hasRules` | `bool hasRules() const` | 规则目录存在 |
| `scanServerConfigs` | `std::vector<ServerConfigEntry> scanServerConfigs()` | 扫描目标 `savesDir` 下各存档目录的 `serverconfig/`（递归） |
| `syncConfig` | `bool syncConfig(const ServerConfigEntry& entry, NeoCore::CancelToken* cancelToken = nullptr)` | 同步单个条目（内容相同跳过；Ignore 跳过） |
| `syncAll` | `bool syncAll(NeoCore::CancelToken* cancelToken = nullptr)` | 同步全部条目 |
| `syncedCount` / `skippedCount` / `failedCount` | `int ...() const` | 统计访问器 |

**规则文件格式**（规则存储于 `branches/<branch>/save/[save]/serverconfig/`，`save` = 存档文件夹、`[save]` = 单个存档目录占位、均为字面目录名）：

| 文件 | 格式（逐字摘录头文件注释） | 说明 |
|------|------|------|
| `<源文件本体>` | — | 镜像内容，同步到目标每个存档的 `serverconfig/` |
| `.rule/globle.json` | `{ default_mode, folder_mode, version, description }` | `folder_mode`: 本层文件夹同步模式（full 模式应用，`skip\|mirror\|incremental_add\|incremental_overwrite`，默认 mirror）。实现实际读取 `default_mode`（`full`/`overwrite`/`force`→Full，`partial`/`merge`→Partial，`ignore`→Ignore，默认 Full）与 `folder_mode`；`version`/`description` 为预留字段 |
| `.rule/list.json` | `{ files: { <rel>: {mode, tracked_keys} } }` | 逐文件同步模式；兼容旧字符串格式 `{ <rel>: "mode" }`；路径反斜杠自动规整为正斜杠 |
| `.rule/<其他文件>` | — | 规则文件组（预留，同步时忽略） |

模式语义与 `sync_policies.files` 统一：`full`/`force`/`partial`/`ignore`；`full` = 应用本层 `folder_mode`；`partial` 的 `tracked_keys` 与 config 同步逻辑一致（为空时取 `list_keys` 全部键）。`Force` 直接覆盖（写前备份 `.backup`）；`full`+`IncrementalAdd` 目标已存在则不动。

## 典型用法

### 1. 初始化引擎并执行完整构建

```cpp
#include <build_engine.h>

NeoBuild::BuildEngine engine;
if (!engine.init("K:/repos/mypack",     // workspacePath（仓库目录；亦可传 workspace.json 路径）
                 "",                    // cacheDir   默认 <workspace>/.cache
                 "",                    // outputBaseDir 默认 <workspace>/output
                 "exporters")) {        // exportersDir 导出插件目录（需先部署插件 DLL + .meta.json）
    return 1;
}
engine.setGitPath("C:/tools/git/bin/git.exe");   // 可选：注入 git 路径

NeoCore::BuildResult r = engine.build("dev", nullptr, nullptr, "main");
// branchName="dev"：branch_config 清单 + 分支合并；gitBranch="main"：checkout 用 git 分支
if (!r.success) {
    // r.errorMessage / r.warnings
}
```

### 2. 虚拟构建预览（三要素必需）

```cpp
NeoBuild::BuildEngine engine;
engine.init("K:/repos/mypack", "", "", "exporters");  // ① workspace = 真实仓库
                                                       // ② init 第 4 参扫描导出插件
engine.build("dev", nullptr, nullptr, "main");         // ③ 先执行虚拟构建，生成 outputDir 产物

NeoCore::ExportMetadata meta;
meta.name = "My Modpack";
meta.version = "1.2.0";
meta.author = "me";
meta.game_version = "1.21";
meta.modloader = "forge";

nlohmann::json tree = engine.previewStructure("mcbbs", meta);   // 或 ModpackExporter::previewStructure
// tree: [{"path","dir","umd"}]，umd ∈ {"","U","M","D"}
```

只 `init` + 扫描空目录必然得到空树：预览必须真实仓库 + 扫描导出插件 + 先执行虚拟构建。

### 3. 下载指针文件（含缓存与零信任校验）

```cpp
#include <pointer_downloader.h>

NeoBuild::PointerDownloader downloader;
downloader.scanResolvers("pointers");   // 加载 NeoPointer_*.dll（CreatePointer）

NeoCore::PointerInfo ptr;
ptr.sha256  = "a3f5...";                // 目标文件 SHA-256
ptr.resolver = "directurl";             // 解析器名
ptr.metadata = {{"url", "https://example.com/mods/foo.jar"}};

auto r = downloader.download(ptr, "cache",
    [](const NeoBuild::DownloadProgress& p) {
        // p.bytesDownloaded / p.totalBytes / p.completed / p.error
    });
if (r.success) {
    // r.cachedPath = cache/<sha256>，已通过 SHA-256 校验
}
bool cached = downloader.isCached("cache", ptr.sha256);  // 含零信任重算
```

### 4. 导出整合包

```cpp
bool ok = engine.exportModpack("mcbbs", "out/my_pack.zip", meta);

// 或独立使用 ModpackExporter
NeoBuild::ModpackExporter exp;
exp.scanExporters("exporters");
for (const auto& f : exp.availableFormats()) { /* mcbbs / modrinth / hmcl ... */ }
bool ok2 = exp.exportModpack("modrinth", "build_dir", "out/pack.mrpack", meta);
nlohmann::json preview = exp.previewStructure("modrinth", "build_dir", meta);
```

### 5. serverconfig 规则同步

```cpp
#include <serverconfig_sync.h>

NeoBuild::ServerConfigSync scs;
scs.init("K:/hmcl/versions/1.21/saves",   // 目标各存档所在目录
         "K:/repos/mypack",               // 仓库根
         "dev");                          // 整合包分支
if (scs.hasRules()) {
    auto entries = scs.scanServerConfigs();  // 扫描各存档 serverconfig/
    scs.syncAll();  // 按 .rule/globle.json + .rule/list.json 逐文件同步
    // scs.syncedCount() / scs.skippedCount() / scs.failedCount()
}
```

## 注意事项

1. **插件 DLL 必须 `__declspec(dllexport)`**：`extern "C" __declspec(dllexport) IModpackExporter* CreateExporter()`（指针解析器为 `CreatePointer`，解析器为 `CreateParser`）缺导出时 DLL 零导出，`GetProcAddress` 全部失败且运行期不易察觉。新增/修改插件后用 `dumpbin /exports <dll>` 确认符号存在。
2. **`.meta.json` → `.dll` 命名陷阱**：`ModpackExporter::scanExporters` 剥离 `.meta.json` 后缀再拼同名 `.dll`（modpack_exporter.cpp:56）；若用 `path.replace_extension(".dll")` 会把 `X.meta.json` 拼成 `X.meta.dll` 导致全部加载失败。新增/改动插件加载逻辑先对照该处。
3. **`std::move` 后不得读取被移对象**：`modpack_exporter.cpp` 曾 `std::move(loaded)` 后再读 `loaded.format`（std::string 移后为空），日志信息丢失但容器内容正确，易被忽略。
4. **`NeoBuild::ResolveResult` 是命名空间级 struct**（pointer_downloader.h），非 `PointerDownloader` 嵌套类型：成员函数定义返回类型要写 `NeoBuild::ResolveResult`，写 `PointerDownloader::ResolveResult` 报 C2039（定义文件内主体实现可只用 `ResolveResult`，已处于 NeoBuild 命名空间）。
5. **虚拟构建预览三要素**：① workspace = 真实仓库（workspace.json 自然存在）；② `init` 时传 `exportersDir`（第 4 参）触发 `scanExporters`；③ 必须执行虚拟构建 `engine.build(branch, nullptr, nullptr, gitBranch)` 生成 build_dir 产物后再 `previewStructure`。`build` 第 4 参 `gitBranch`：checkout 用 git 分支、merge 用整合包分支（workspace.json 的整合包分支名通常不是 git 分支，直接 checkout 会「分支切换失败」）。预览 cache/output 放系统临时目录，勿放 exe 目录（会残留在发布包）。
6. **空指针容错约定**：`progress`/`cancelToken` 允许传空，引擎静默降级为无 UI 构建；`IBuildProgress` 实现方不得因任何输入抛异常或崩溃，无效子进度条句柄（`<= 0` 或已移除）必须静默忽略。
7. **插件日志注入走导出符号**：`NEO_DECLARE_PLUGIN_LOG_SINK("插件名")` 展开出 `SetPluginLogSink(ILogSink*)` 导出 + `PluginLog(...)`；`ModpackExporter::loadExporter` 与 `PointerDownloader::loadResolverDLL` 均 `GetProcAddress("SetPluginLogSink")` 注入宿主 `LoggerLogSink`（带 `[插件名]` 前缀）。新插件类型走导出符号而非固定接口，旧插件找不到符号自动跳过。
8. **改共享头文件签名后必须 clean-first 全量重建**：如 `BuildEngine::init/build` 加参，插件 DLL（NeoExporter_*）与消费方若链接旧符号报 LNK2019；改动类成员尺寸后确认引用它的每个 target 都重编（`new Xxx()` 的 TU 最易漏，核对 .obj 时间戳晚于头文件）。
9. **serverconfig 同步目录路径铁律**：`branches/<branch>/save/[save]/serverconfig/`（`save` 与 `[save]` 均为字面目录名）；改此路径必须全链路同步 `serverconfig_sync.cpp`（init ruleBase + sourcePathFor）等 7 处（见 AGENTS.md）。
10. **`std::filesystem` 路径编码**：Windows 上对外展示/JSON 用 `generic_u8string()`（正斜杠），纯磁盘访问可用 `u8string()`，Qt 侧 `fromUtf8`；umd_generator 即以 `fs::u8path` + `generic_u8string` 为正确范例。混用 `u8string()`（反斜杠）会导致按 `/` split 的树结构全部平铺。
11. **✅ 已修复（2026-08-30）：`version.json`/`hmclversion.cfg` 不再硬编码**：`BuildEngine::generateVersionJson`/`generateHMCLVersionCfg` 现从 `workspace_->findBranch(currentBranch_)` 取真实 `gameVersion`/`modloader`/`modloaderVersion` 写入，旧固定值 `mc_version=1.21`/`modloader=forge` 已移除。

## 相关文档

- **docs/deploy/main/**：主程序部署与操作（`formats.md` 整合包格式、`operation-guide.md` 操作指南、`troubleshooting.md` 排查）。
- **docs/Modules/**：模块文档（本文所在目录 `README.md` + `usage.md`；其他模块文档同构）。
- **docs/deploy/CLI/**：CLI 子系统（`CLI.md` 总览、`CLI-usage.md` 参数/JSON 协议、`CLI-info.md` 信息命令、`CLI-flow.md` flow 向导、`CLI-exec.md` 执行命令、`CLI-errors.md` 退出码）。