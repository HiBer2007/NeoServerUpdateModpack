# NeoCore 使用文档

> 适用对象：NeoCore 的消费方（工作区/构建/GUI/CLI/编辑器）与插件作者（`NeoParser_*` / `NeoPointer_*` / `NeoExporter_*` / `NeoEditorExtension_*`）。
> 本文全部 API 名称、签名、枚举值逐字摘自 `modules/NeoCore/include/` 头文件；行为描述依据 `modules/NeoCore/src/` 实现。

## 快速开始

NeoCore 为 STATIC 库，消费方在 CMake 中链接即自动获得 `include/` 头文件路径与日志依赖（PUBLIC 传递 CommonLoggerCPP）：

```cmake
# 消费方（插件 DLL / 可执行一般用 PRIVATE；自身头文件要暴露 NeoCore 类型的库用 PUBLIC）
target_link_libraries(<your_target>
    PRIVATE NeoCore
)
```

按需包含头文件，全部符号位于 `NeoCore` 命名空间：

```cpp
#include <PluginLoader.h>
#include <cancel_token.h>
#include <IBuildProgress.h>
// ...
NeoCore::CancelToken token;
```

> 注意：`IModpackExporter.h` / `IPluginPointer.h` / `IConfigEditorExtension.h` 头文件本身 include `<nlohmann/json.hpp>`，`IPointerEditorExtension.h` / `IConfigEditorExtension.h` include `<QWidget>` / `<QJsonObject>`。NeoCore 的 CMakeLists 不链接 Qt 与 nlohmann，消费方须自行链接（如插件 DLL 链接 `spdlog::spdlog`、`nlohmann_json::nlohmann_json`、`Qt6::Widgets`）。

## 公共 API

### IBuildProgress.h — 构建进度回调（宿主实现）

| 符号 | 签名（逐字摘自头文件） | 说明 |
|---|---|---|
| struct `BuildProgress` | `std::string stage; int percent = 0; std::string message;` | 构建进度数据结构，由构建引擎产出 |
| struct `BuildResult` | `bool success = false; std::string outputDir; std::string errorMessage; std::vector<std::string> warnings; int totalFiles = 0; int syncedFiles = 0; int failedFiles = 0;` | 构建结果 |
| class `IBuildProgress` | 纯虚接口（主程序实现，注入导出插件内的构建引擎） | 构建进度接口 |
| 主进度 | `virtual void set_main_stage(const std::string& stage) = 0;` `virtual void set_main_progress(int percent) = 0;` `virtual void set_main_message(const std::string& message) = 0;` | 主进度条三件套 |
| 子进度（句柄化） | `virtual int add_sub_bar(const std::string& label) = 0;` `virtual void remove_sub_bar(int handle) = 0;` `virtual void set_sub_progress(int handle, int percent) = 0;` `virtual void set_sub_info(int handle, const std::string& info) = 0;` | 返回新句柄（>0），无效输入静默忽略 |
| 日志 | `virtual void log(const std::string& line) = 0;` | 构建页日志面板 / CLI 终端 |
| 取消检查 | `virtual bool is_cancelled() const = 0;` | 返回 true 表示应中止构建 |

头文件注释中的容错约定（实现方与调用方都须遵守）：调用方允许传空指针，实现方必须静默降级为无 UI 构建；子进度句柄 `handle <= 0` 或已移除的句柄必须静默忽略；实现方不得因任何输入抛异常或崩溃。

### IConfigParser.h — 配置解析器契约（插件实现）

| 符号 | 签名 | 说明 |
|---|---|---|
| enum class `TrackingMode` | `FullSync, ConfigMerge, LineByLine, NoSync` | 同步模式 |
| struct `ParserCapability` | `std::string name; std::vector<std::string> extensions; std::vector<TrackingMode> supported_modes; bool supports_line_tracking; int priority;` | 解析器能力声明 |
| struct `ConfigEntry` | `std::string key_path; std::string remote_value; std::string local_value; bool is_tracked;` | 键值配置条目 |
| struct `LineEntry` | `int line_number; std::string remote_text; std::string local_text; bool is_tracked;` | 行级条目 |
| class `IConfigParser` | 纯虚接口 | 配置解析器 |
| 能力 | `virtual ParserCapability capability() const = 0;` | |
| 匹配 | `virtual bool can_handle(const std::string& filepath) const = 0;` | |
| 键值解析 | `virtual std::vector<ConfigEntry> parse_entries(const std::string& filepath) = 0;` | |
| 键值合并 | `virtual std::string merge_entries(const std::string& filepath, const std::vector<std::string>& tracked_keys, const std::string& remote_content, const std::string& local_content) = 0;` | 按 tracked_keys 将 remote 值合入 local |
| 行级解析（可选） | `virtual std::vector<LineEntry> parse_lines(const std::string& filepath) { return {}; }` | 不实现则用默认空实现 |
| 行级合并（可选） | `virtual std::string merge_lines(const std::string& filepath, const std::vector<int>& tracked_lines, const std::string& remote_content, const std::string& local_content) { return ""; }` | |
| 键列表 | `virtual std::vector<std::string> list_keys(const std::string& filepath) = 0;` | |
| 工厂类型 | `using CreateParserFunc = IConfigParser* (*)();` | 插件导出函数类型 |

**插件作者如何实现**（参考 `modules/NeoParser_JSON/src/parser_json.cpp`）：
1. 派生 `IConfigParser` 实现全部纯虚方法；`capability().extensions` 填**小写**扩展名（如 `.json`, `.json5`, `.jsonc`）——`PluginLoader` 按小写扩展名注册/匹配（plugin_loader.cpp:238-244）。
2. 导出 `extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser()`（必须带 `__declspec(dllexport)`，见注意事项）。
3. 可选：`NEO_DECLARE_PLUGIN_LOG_SINK("插件名")`（宏来自 CommonLoggerCPP 的 `plugin_log_sink.h`）导出 `SetPluginLogSink` 供宿主注入日志。
4. 配套 `<插件名>.meta.json` 与 DLL 同放 `parsers/` 目录（meta 格式见「典型用法」示例 2）。

### IPluginPointer.h — 指针解析器契约（插件实现）

| 符号 | 签名 | 说明 |
|---|---|---|
| struct `PointerInfo` | `std::string resolver; std::string sha256; nlohmann::json metadata;` | 单个解析器信息 |
| struct `PointerFileData` | `std::string sha256; std::vector<std::string> original_names; std::vector<PointerInfo> resolvers; std::vector<std::string> download_methods;` 另有 `nlohmann::json toJson() const` 与 `static PointerFileData fromJson(const nlohmann::json& j)` | 指针文件数据（含 JSON 序列化/反序列化） |
| class `IPluginPointer` | 纯虚接口 | 指针解析器 |
| 身份 | `virtual std::string name() const = 0;` | |
| 匹配 | `virtual bool can_handle(const PointerInfo& ptr) const = 0;` | 通常按 `ptr.resolver` 匹配 |
| 解析 | `virtual std::string resolve_url(const PointerInfo& ptr) = 0;` | 返回下载 URL，失败返回空串 |
| 校验 | `virtual bool validate(const std::string& filepath, const std::string& expected_sha256) = 0;` | 重新计算文件 SHA-256 与期望值比对（零信任原则） |
| 下载方式（可选） | `virtual std::vector<std::string> supported_download_methods() const { return {}; }` | |
| 批量搜索（可选） | `virtual bool can_batch_search() const { return false; }` `virtual std::vector<PointerInfo> batch_search(const std::string& modId, const std::string& version, const std::string& sha256) { return {}; }` | |
| class `IDownloadMethod` | `virtual std::string name() const = 0;` `virtual bool download(const std::string& url, const std::string& destPath, const std::string& expectedSha256) = 0;` | 下载方式接口 |
| 工厂类型 | `using CreatePointerFunc = IPluginPointer* (*)();` `using CreateDownloaderFunc = IDownloadMethod* (*)();` | |

**插件作者如何实现**（参考 `modules/NeoPointer_DirectURL/src/pointer_directurl.cpp`）：导出 `extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer()`；`resolve_url` 返回空串表示解析失败；`validate` 小写 hex 比对 SHA-256；配套 meta.json 与 DLL 放 `pointers/` 目录。

### IModpackExporter.h — 导出器契约（插件实现）

| 符号 | 签名 | 说明 |
|---|---|---|
| struct `ExportMetadata` | `std::string name; std::string version; std::string author; std::string game_version; std::string modloader; std::string modloader_version; std::string summary; std::string description; std::vector<std::string> language_files; nlohmann::json extra;` | 导出元数据 |
| struct `BuildTarget` | `std::string workspace_path; std::string workspace_json; std::string cache_dir; std::string output_path; std::string staging_dir; std::string branch; bool sync_to_directory = false; ExportMetadata metadata;` | 构建目标（字段语义见头文件注释：sync_to_directory=false 时 output_path 为中间构建目录，true 时为同步目标工作目录） |
| class `IModpackExporter` | 接口 | 导出器 |
| 身份 | `virtual std::string format_name() const = 0;` `virtual std::string file_extension() const = 0;` `virtual std::string format_description() const = 0;` | |
| 构建入口 | `virtual BuildResult build_modpack(const BuildTarget& target, IBuildProgress* progress, CancelToken* cancel)`（默认实现返回 `errorMessage = "build_modpack not implemented"`） | 完整构建流程入口；`progress`/`cancel` 可为空指针，静默降级为无 UI 构建 |
| 导出 | `virtual bool export_modpack(const std::string& build_dir, const std::string& output_path, const ExportMetadata& metadata) = 0;` | 打包为最终格式 |
| 预览 | `virtual nlohmann::json preview_structure(const std::string& build_dir, const ExportMetadata& metadata, const std::string& target_dir = "")`（默认返回空数组） | 模拟最终归档结构而不写文件；条目 `[{"path": "...", "dir": true\|false, "umd": ""\|"U"\|"M"\|"D"}, ...]`，`target_dir` 非空则比对生成 U/M/D |
| 工厂类型 | `using CreateExporterFunc = IModpackExporter* (*)();` | |

**插件作者如何实现**（参考 `modules/NeoExporter_HMCL/src/exporter_hmcl.cpp`）：导出 `extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter()`；`build_modpack` 内部可驱动 `NeoBuild::BuildEngine`（`engine.init(wsJson, cache_dir, staging)` → `engine.setTargetDir(output_path)` → `engine.build(branch, progress, cancel)`）；配套 meta.json 与 DLL 放 `exporters/` 目录（meta 含 `format` / `extension` / `description` 字段，见 `modules/NeoExporter_HMCL/NeoExporter_HMCL.meta.json`）。

### IPointerEditorExtension.h — 指针编辑器扩展（Qt 插件）

| 符号 | 签名 | 说明 |
|---|---|---|
| class `IPointerEditorExtension` | 纯虚接口 | 按 resolver 类型维度提供指针元数据编辑器 |
| 方法 | `virtual std::string resolverType() const = 0;` `virtual QWidget* createEditor(QWidget* parent) = 0;` `virtual void loadMetadata(QWidget* editor, const QJsonObject& metadata) = 0;` `virtual QJsonObject saveMetadata(QWidget* editor) const = 0;` | 创建编辑器控件、装载/保存元数据 |
| 工厂类型 | `using CreateEditorExtensionFunc = IPointerEditorExtension* (*)();` | |

**插件作者如何实现**（参考 `modules/NeoEditorExtension_Pointer_DirectURL/src/editor_directurl.cpp:64`）：导出 `extern "C" __declspec(dllexport) NeoCore::IPointerEditorExtension* CreateEditorExtension()`；宿主 `GUIWorker::EditorExtensionRegistry` 经 `entry.lib->resolve("CreateEditorExtension")` 加载（editor_extension_registry.cpp:165-166）。

### IConfigEditorExtension.h — 配置编辑器扩展（Qt 插件）

| 符号 | 签名 | 说明 |
|---|---|---|
| class `IConfigEditorExtension` | 纯虚接口 | 按文件扩展名维度提供配置编辑器 |
| 方法 | `virtual std::string fileExtension() const = 0;` `virtual QWidget* createEditor(QWidget* parent) = 0;` `virtual void loadConfig(QWidget* editor, const std::string& remoteContent, const std::string& localContent, const nlohmann::json& syncRules) = 0;` `virtual nlohmann::json saveSyncRules(QWidget* editor) const = 0;` `virtual std::string mergePreview(QWidget* editor) const = 0;` | 装载内容与同步规则、保存规则、生成合并预览 |
| 行定位（可选） | `virtual std::vector<int> trackedLines(const std::string& content, const std::vector<std::string>& trackedKeys) const`（默认返回空数组） | 追踪键在内容中的 1-based 行列表，供 merge 预览标记；未支持时返回空数组（宿主回退整行标记） |
| 工厂类型 | `using CreateConfigEditorFunc = IConfigEditorExtension* (*)();` | |

**插件作者如何实现**（参考 `modules/NeoEditorExtension_Parser_JSON/src/editor_parser_json.cpp:86`）：导出 `extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor()`；宿主经 `resolve("CreateConfigEditor")` 加载（editor_extension_registry.cpp:141-142）。

### PluginLoader.h — 解析器插件加载器

| 符号 | 签名 | 说明 |
|---|---|---|
| 构造/析构 | `PluginLoader();` `~PluginLoader();` | 析构时释放全部已加载 DLL（`FreeLibrary`），实例随之失效 |
| 扫描 | `void ScanDirectory(const std::string& parsersDir);` | 扫描目录下 `*.meta.json`，由文件名剥离 `.meta.json` 后缀拼 `.dll` 加载（plugin_loader.cpp:141-149）；DLL 缺失记 Error 并跳过 |
| 查找 | `IConfigParser* FindParser(const std::string& filepath) const;` | 按文件扩展名（转小写）查注册表，未命中返回 `nullptr` |
| 列表 | `std::vector<ParserCapability> ListParsers() const;` | 已加载解析器的能力列表 |
| 数量 | `size_t ParserCount() const { return owned_parsers_.size(); }` | 已加载解析器数量 |

**扫描/加载语义**（plugin_loader.cpp 实现）：
- 只处理扩展名为 `.json` 且命名形如 `NeoParser_XXX.meta.json` 的文件（其他 json 跳过）；DLL 名由文件名推导为 `NeoParser_XXX.dll`（与 `ModpackExporter::scanExporters` 同款剥离逻辑，modpack_exporter.cpp:56）。
- 加载流程：解析 meta（nlohmann::json）→ 加载 DLL（Windows `LoadLibraryW`，UTF-8 路径）→ `GetProcAddress("CreateParser")` →（可选）`GetProcAddress("SetPluginLogSink")` 注入宿主 `LoggerLogSink` → `createFunc()` 建实例 → 取 `capability()` 按 extensions 注册 → 记录 DLL 句柄。
- 生命周期：`FindParser` 返回的指针归 `PluginLoader` 所有，loader 析构（FreeLibrary）后旧指针全部失效。

### cancel_token.h — 取消令牌（header-only）

| 符号 | 签名 | 说明 |
|---|---|---|
| 构造 | `CancelToken() : cancelled_(false) {}` | 默认未取消 |
| 发起取消 | `void request_cancel() { cancelled_.store(true); }` | 线程安全，可跨线程调用 |
| 查询 | `bool is_cancelled() const { return cancelled_.load(); }` | 原子读，需在循环内主动轮询 |
| 重置 | `void reset() { cancelled_.store(false); }` | 复用令牌；须确保无并发查询后进行 |

底层为 `std::atomic<bool>`；`src/cancel_token.cpp` 为空占位（header-only）。

### error_codes.h — 错误码

| 区间 | 枚举值（逐字摘自头文件） | 值 |
|---|---|---|
| 成功 | `Success` | 0 |
| Git | `GitTimeout, GitNotFound, GitCrash, GitWriteError, GitReadError` | 1001–1005 |
| 配置 | `ConfigParseFailed, ConfigFormatUnknown, ConfigMergeFailed` | 2001–2003 |
| 工作区 | `WorkspaceNotInitialized, WorkspaceSyncFailed, WorkspaceBranchNotFound` | 3001–3003 |
| 构建 | `BuildFailed, BuildExportFailed` | 4001–4002 |
| 网络 | `NetworkError, DownloadFailed, HashMismatch` | 5001–5003 |
| 其他 | `Cancelled, Unknown` | 9000, 9999 |

`const char* ErrorCodeToString(ErrorCode code);` — 返回中文文案（error_codes.cpp 实现，如 `Success` → `"操作成功"`、`HashMismatch` → `"哈希校验不匹配"`）。

### git_analyzer.h — Git 错误诊断

`std::string AnalyzeGitError(const std::string& stderrOutput);` — 按关键字命中返回中文诊断（git_analyzer.cpp）：网络失败（`unable to access` / `Could not resolve host` / `Failed to connect` / `Connection refused` / `Connection timed out` / `Could not read from remote repository`）→ 认证失败（`Permission denied` / `could not read Username` / `Authentication failed`）→ 仓库未初始化（`not a git repository` / `does not have any commits yet`）→ 远程分支不存在 → 合并冲突（`merge conflict` / `CONFLICT` / `Automatic merge failed`）→ 所有权可疑（`detected dubious ownership`）→ 引用无效（`fatal: not a valid object name`）→ 兜底 `"Git 操作失败：" + stderrOutput`；空输出返回 `"Git 操作成功"`。

## 典型用法

### 1. 加载插件目录并查找解析器（参考 `modules/NeoBuild/src/serverconfig_sync.cpp:341-362`）

```cpp
#include <PluginLoader.h>
#include <QCoreApplication>
#include <filesystem>

NeoCore::PluginLoader loader;
std::string parsersDir = (std::filesystem::path(
    QCoreApplication::applicationDirPath().toStdString()) / "parsers").string();
if (std::filesystem::exists(parsersDir)) {
    loader.ScanDirectory(parsersDir);
}

NeoCore::IConfigParser* parser = loader.FindParser(sourcePath);
if (!parser) {
    /* 无解析器：回退整文件覆盖 */
}
std::vector<std::string> keys = parser->list_keys(sourcePath);
std::string merged = parser->merge_entries(
    sourcePath, keys, remoteContent, localContent);
```

### 2. 实现一个配置解析器插件（参考 `modules/NeoParser_JSON/src/parser_json.cpp`）

```cpp
#include <IConfigParser.h>

class JsonParser : public NeoCore::IConfigParser {
public:
    NeoCore::ParserCapability capability() const override {
        NeoCore::ParserCapability cap;
        cap.name = "JSON";
        cap.extensions = {".json", ".json5", ".jsonc"};
        cap.supported_modes = {NeoCore::TrackingMode::FullSync,
            NeoCore::TrackingMode::ConfigMerge, NeoCore::TrackingMode::NoSync};
        cap.supports_line_tracking = false;
        cap.priority = 100;
        return cap;
    }
    bool can_handle(const std::string& filepath) const override { /* 扩展名判断 */ }
    std::vector<NeoCore::ConfigEntry> parse_entries(const std::string& filepath) override { /* ... */ }
    std::string merge_entries(const std::string& filepath,
        const std::vector<std::string>& tracked_keys,
        const std::string& remote_content,
        const std::string& local_content) override { /* ... */ }
    std::vector<std::string> list_keys(const std::string& filepath) override { /* ... */ }
};

extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser() {
    return new JsonParser();
}
```

配套 meta 文件（参考 `modules/NeoParser_JSON/NeoParser_JSON.meta.json`，与 DLL 同放 `parsers/`）：

```json
{
  "name": "JSON Config Parser",
  "version": "1.0.0",
  "dll": "NeoParser_JSON.dll",
  "capability": {
    "name": "JSON",
    "extensions": [".json", ".json5", ".jsonc"],
    "supported_modes": ["full_sync", "config_merge", "no_sync"],
    "line_tracking": false,
    "priority": 100
  }
}
```

### 3. 取消检查（参考 `modules/NeoCLI/src/cli_dispatcher.cpp`）

```cpp
#include <cancel_token.h>

NeoCore::CancelToken cancelToken;          // 主线程创建
// 工作线程 / 长循环内主动轮询：
while (!cancelToken.is_cancelled()) {
    /* 逐步处理 */
}
// 任意线程发起取消：
cancelToken.request_cancel();
```

### 4. 实现 IBuildProgress 并注入导出插件构建（参考 `modules/GUIWorker/include/build_page.h`、`modules/NeoExporter_HMCL/src/exporter_hmcl.cpp:45-53`）

```cpp
#include <IBuildProgress.h>
#include <IModpackExporter.h>
#include <cancel_token.h>
#include <cstdio>

class CliBuildProgress : public NeoCore::IBuildProgress {
public:
    explicit CliBuildProgress(NeoCore::CancelToken* token) : token_(token) {}
    void set_main_stage(const std::string& s) override { std::printf("[%s]\n", s.c_str()); }
    void set_main_progress(int) override {}
    void set_main_message(const std::string&) override {}
    int  add_sub_bar(const std::string&) override { return 1; }
    void remove_sub_bar(int) override {}
    void set_sub_progress(int, int) override {}
    void set_sub_info(int, const std::string&) override {}
    void log(const std::string& line) override { std::printf("%s\n", line.c_str()); }
    bool is_cancelled() const override { return token_ && token_->is_cancelled(); }
private:
    NeoCore::CancelToken* token_;
};

// 调用方：
NeoCore::CancelToken cancelToken;
CliBuildProgress progress(&cancelToken);
NeoCore::IModpackExporter* exporter = /* 经 ModpackExporter 扫描加载 */;
NeoCore::BuildTarget target;   // 填充 workspace_path / branch / metadata 等
NeoCore::BuildResult result = exporter->build_modpack(target, &progress, &cancelToken);
// 容错约定：progress / cancel 均可传 nullptr（静默降级为无 UI 构建）
```

### 5. 实现配置编辑器扩展（参考 `modules/NeoEditorExtension_Parser_JSON/src/editor_parser_json.cpp`）

```cpp
#include <IConfigEditorExtension.h>

class JsonConfigEditorExtension : public NeoCore::IConfigEditorExtension {
public:
    std::string fileExtension() const override { return ".json"; }
    QWidget* createEditor(QWidget* parent) override { /* 返回格式专属编辑器控件 */ }
    void loadConfig(QWidget* editor, const std::string& remoteContent,
        const std::string& localContent,
        const nlohmann::json& syncRules) override { /* 装载内容与规则 */ }
    nlohmann::json saveSyncRules(QWidget* editor) const override { /* 保存规则 */ }
    std::string mergePreview(QWidget*) const override { return ""; }
};

extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
    return new JsonConfigEditorExtension();
}
```

（指针编辑器扩展同理：实现 `IPointerEditorExtension` 并导出 `CreateEditorExtension()`。）

### 6. Git 错误诊断（参考 `modules/NeoWorkspace/src/git_operations.cpp`）

```cpp
#include <git_analyzer.h>

// git 命令失败后，把 stderr 交给诊断函数：
std::string diag = NeoCore::AnalyzeGitError(stderrText);
CLogger::Warn("git failed: {}", diag);
// 例："Git 远程仓库连接失败，请检查网络与仓库地址"
```

## 注意事项

来源标注：`[AGENTS.md]` = 项目 AGENTS.md 关键陷阱章节；`[src]` = NeoCore 源码实现；`[H]` = 头文件注释。

1. **插件 DLL 必须导出 `extern "C" __declspec(dllexport) CreateXxx()`**（`[AGENTS.md]`）：缺 `__declspec(dllexport)` 时 DLL 零导出，`PluginLoader` / `PointerDownloader` / `ModpackExporter` 的 `GetProcAddress` 全部失败且不易察觉。新增/修改插件 create 函数后，用 `dumpbin /exports <dll>` 确认存在 `CreateXxx` 导出。
2. **`.meta.json` 命名陷阱**（`[AGENTS.md]`）：meta 文件名必须形如 `NeoParser_XXX.meta.json`，插件加载按「剥离 `.meta.json` 后缀再拼 `.dll`」推导 DLL 名（plugin_loader.cpp:141-149）。曾发生由 `NeoParser_JSON.meta.json` 拼出 `NeoParser_JSON.meta.dll` 导致全部解析器 `DLL missing` 的回归；新增/改动插件加载逻辑先对照 `ModpackExporter::scanExporters`（modpack_exporter.cpp:56）。`ScanDirectory` 只处理 `.meta.json`，其他 json 跳过；meta 存在但 DLL 缺失时记 Error `"Plugin meta exists but DLL missing ..."`。
3. **CancelToken 使用纪律**（`[H]` / `[src]`）：`is_cancelled()` 是原子读，须在循环内主动轮询；`request_cancel()` 线程安全；`reset()` 复用令牌前须确认无并发访问（`cancelled_` 为 `std::atomic<bool>`，reset 仅 store(false)）。构建入口的 `progress` / `cancel` 参数允许传 `nullptr`（IBuildProgress.h:29-32 容错约定）。
4. **IBuildProgress 容错约定**（`[H]`）：实现方不得因任何输入抛异常或崩溃；子进度句柄 `handle <= 0` 或已移除的句柄必须静默忽略；调用方允许传空指针（无 UI 构建）。
5. **插件内不要调用 `CLogger::Init`（跨 DLL 日志共享）**（`[AGENTS.md]`）：NeoCore 与 CommonLoggerCPP 均为 STATIC → 每个插件 DLL 各持有一份日志静态实例且从不 Init。应通过 `NEO_DECLARE_PLUGIN_LOG_SINK("插件名")`（宏来自 CommonLoggerCPP 的 `plugin_log_sink.h`，**不是 NeoCore 头**）导出 `SetPluginLogSink(ILogSink*)`；`PluginLoader::LoadPlugin` 会经 `GetProcAddress("SetPluginLogSink")` 注入宿主 `LoggerLogSink`（plugin_loader.cpp:195-199）；插件未导出该符号时自动跳过，`PluginLog` 回退 spdlog default_logger。
6. **生命周期**（`[src]`）：`FindParser` 返回的 `IConfigParser*` 归 `PluginLoader` 所有；loader 析构对全部已加载 DLL 执行 `FreeLibrary`，之后旧指针失效。扩展名匹配按小写（plugin_loader.cpp:238-244），`capability().extensions` 应填小写扩展名。
7. **契约变更必须 clean-first 全量重建**（`[AGENTS.md]`）：NeoCore 头文件（接口签名/类布局）被宿主与全部插件共享；改动后未同步重建会导致插件 DLL LNK2019（链接旧符号），涉及类布局时核对引用它的每个 target 的 .obj 时间戳。
8. **源码与日志编码约定**（`[AGENTS.md]`）：NeoCore 源码保持纯 ASCII（不写中文字面量）；日志消息用英文；`ErrorCodeToString` 返回的中文文案仅供 UI 展示。
9. **路径编码**（`[AGENTS.md]`）：Windows 下 `fs::path ↔ std::string` 一律用 `u8string()` / `fs::u8path()`（UTF-8 全链路）；`PluginLoader` 内部已用 `MultiByteToWideChar(CP_UTF8, ...)` + `LoadLibraryW` 处理 DLL 路径。

## 相关文档

- **插件体系总览**：AGENTS.md「插件系统」章节（三套插件：`parsers/`、`pointers/`、`exporters/`）——本文档的 meta.json 字段与 CreateXxx 约定均与其对应。
- **日志模块**：`modules/CommonLoggerCPP`（`CLogger`、`ILogSink`、`LoggerLogSink`、`plugin_log_sink.h`）——NeoCore 唯一显式依赖。
- **CLI 文档**：`docs/deploy/CLI/CLI.md`（总览）、`CLI-usage.md`、`CLI-info.md`、`CLI-flow.md`、`CLI-exec.md`、`CLI-errors.md`（CLI 经 NeoCore 契约驱动：`CancelToken`、`PluginLoader`、`IModpackExporter`、`IPluginPointer`）。
- **模块文档**：`docs/Modules/README.md` 总索引（按核心库/GUI 库/可执行/插件登记全部模块）；本模块文档 = 本文件的姊妹篇 `README.md`（说明文档）。
- **部署文档**：`docs/deploy/main/formats.md`（导出格式：zip / mrpack / HMCL 目录）、`docs/deploy/main/operation-guide.md`、`docs/deploy/main/troubleshooting.md`。
- **相邻模块**：NeoWorkspace（工作区引擎）、NeoBuild（构建引擎）、GUIWorker（向导/编辑器 UI 宿主）。
- **相关插件模块**：`NeoParser_*`（配置解析器）、`NeoPointer_*`（指针解析器）、`NeoExporter_*`（导出器）、`NeoEditorExtension_*`（编辑器扩展）。