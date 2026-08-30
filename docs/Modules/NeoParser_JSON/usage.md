# NeoParser_JSON 使用文档

## 快速开始

1. 构建项目（含部署）：`cmake --build build --target neo_deploy`。
2. 产物自动出现在 `deploy/parsers/`：
   - `NeoParser_JSON.dll`
   - `NeoParser_JSON.meta.json`
3. 主程序运行时把该目录作为 parsers 目录交给 `PluginLoader::ScanDirectory`（如 `<exe>/parsers`）。加载器找到 `NeoParser_JSON.meta.json` 后，**剥离 `.meta.json` 后缀再拼 `.dll`**（得到 `NeoParser_JSON.dll`），二者必须同名同目录，否则报 `Plugin meta exists but DLL missing` 并跳过。
4. 也可以把这两个文件手工放入任意 parsers/ 目录（同命名约定）实现"即插即用"。

## 插件契约

### IConfigParser 实现要点

接口定义：`modules/NeoCore/include/IConfigParser.h`（命名空间 `NeoCore`）。本插件实现：

| 接口方法 | 本插件行为 |
|----------|-----------|
| `ParserCapability capability() const` | 返回上表能力（name=JSON，3 扩展名，FullSync/ConfigMerge/NoSync，priority=100） |
| `bool can_handle(const std::string& filepath) const` | 扩展名（`.rfind('.')` 子串，大小写敏感）== `.json` / `.json5` / `.jsonc` |
| `std::vector<ConfigEntry> parse_entries(const std::string& filepath)` | 读文件→解析→平面化；解析失败/空文件返回空列表 |
| `std::string merge_entries(const std::string&, const std::vector<std::string>& tracked_keys, const std::string& remote_content, const std::string& local_content)` | 见「格式细节·merge」 |
| `std::vector<LineEntry> parse_lines(const std::string&)` | **未重写**（接口默认返回空）；JSON 无行级追踪 |
| `std::string merge_lines(...)` | **未重写**（接口默认返回空串） |
| `std::vector<std::string> list_keys(const std::string& filepath)` | 等价于 `parse_entries` 的 `key_path` 收集 |

`ConfigEntry` 结构：`{ key_path, remote_value, local_value, is_tracked }`；本插件产出条目 `key_path`=点/下标路径，`remote_value`=叶子值（字符串原样、其余类型 `dump()`），`local_value` 为空、`is_tracked=false`。

### meta.json 字段（逐字段）

文件 `NeoParser_JSON.meta.json`（内容原样）：

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

| 字段 | 值 | 含义 |
|------|-----|------|
| `name` | `"JSON Config Parser"` | 插件展示名（CLI `info plugins` 会输出） |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoParser_JSON.dll"` | 对应 DLL 文件名（声明用） |
| `capability.name` | `"JSON"` | 能力名（与 DLL 内 `capability()` 一致） |
| `capability.extensions` | `[".json", ".json5", ".jsonc"]` | 声明支持的扩展名 |
| `capability.supported_modes` | `["full_sync", "config_merge", "no_sync"]` | 支持的追踪模式字符串（约定映射：`full_sync`→`TrackingMode::FullSync`，`config_merge`→`TrackingMode::ConfigMerge`，`no_sync`→`TrackingMode::NoSync`；枚举真值以 DLL 内 `capability()` 为准） |
| `capability.line_tracking` | `false` | 是否支持行级追踪 |
| `capability.priority` | `100` | 声明优先级 |

> 重要：`PluginLoader::LoadPlugin` 解析 meta.json 仅做 JSON 合法性校验，**注册以 DLL 实例的 `capability()` 运行期返回值为准**（`plugin_loader.cpp` 中 `RegisterParser` 用 `parser->capability().extensions` 建注册表）。

## 支持的格式细节

**parse 语义（parse_entries）**

- 空文件 → 空列表；读取/解析 `std::exception` → 空列表。
- `.json5` / `.jsonc`：先 `strip_json_comments`（剥离字符串外的 `//` 行注释与 `/* */` 块注释，块注释替换为空格）再 `json::parse(stripped, nullptr, true, false)`。
- `.json`：先尝试 `json::parse(content, nullptr, true, true)`（`ignore_comments=true`），失败回退 `json::parse(content, nullptr, true, false)`。
- 平面化规则（`flatten_json`）：
  - 对象：`a.b.c` 点路径；嵌套对象递归展开，叶子（非对象）成条目。
  - 数组：`list[0]` 下标路径；数组内嵌套对象/数组继续展开，标量叶子成条目。
  - 叶子 `remote_value`：字符串取原文，其余类型用 `dump()`。

**merge 语义（merge_entries）**

- 空处理：都空→`{}`；仅 remote 空→返回 `local_content`；仅 local 空→返回 `remote_content`。
- 解析失败：local 解析失败→返回 `remote_content`；remote 解析失败→返回 `local_content`。
- 正常：`result = local`（本地为基线）；遍历 `tracked_keys`，按 `.` 分段（`split_path`）在 result 与 remote 中同时导航（`navigate_json_path`，仅对象路径），双向命中则 `*dst = *src`；最终 `return result.dump(2)`（2 空格缩进输出）。
- **局限（当前实现）**：`navigate_json_path` 只支持对象路径（每段按 key `find`），数组下标路径（如 `arr[0]`，`split_path` 不会拆分下标）的 tracked key 无法命中而被跳过——数组元素仅可展示、暂不可 partial 合并。

## 典型用法

通过 `PluginLoader` 加载并使用（API 签名摘自 `modules/NeoCore/include/PluginLoader.h`）：

```cpp
#include <PluginLoader.h>
#include <IConfigParser.h>
#include <QCoreApplication>

NeoCore::PluginLoader loader;
// 扫描 parsers 目录（含本插件）
loader.ScanDirectory(
    (QCoreApplication::applicationDirPath() + "/parsers").toStdString());

// 按文件扩展名（小写）取解析器；拿不到返回 nullptr
NeoCore::IConfigParser* parser = loader.FindParser(filepath);
if (!parser) { /* 未找到对应解析器 */ }

const auto cap = parser->capability();              // 能力（名称/扩展名/模式/优先级）
auto entries = parser->parse_entries(filepath);     // 平面化条目
auto keys = parser->list_keys(filepath);            // 键列表
std::string merged = parser->merge_entries(filepath,
    trackedKeys, remoteContent, localContent);      // partial 合并
```

- `PluginLoader` 公共 API：`void ScanDirectory(const std::string& parsersDir);`、`IConfigParser* FindParser(const std::string& filepath) const;`、`std::vector<ParserCapability> ListParsers() const;`、`size_t ParserCount() const;`。
- 参考实现：`modules/GUIWorker/src/config_file_editor.cpp`（`ScanDirectory(applicationDirPath()/parsers)` + `FindParser(absRepoPath)`）；加载细节见 `modules/NeoCore/src/plugin_loader.cpp`。

## 注意事项

- **`__declspec(dllexport)` 必须保留**：缺失时 DLL 零导出，`GetProcAddress("CreateParser")` 失败。排查用 `dumpbin /exports NeoParser_JSON.dll`，确认存在 `CreateParser` 与 `SetPluginLogSink`。
- **meta/DLL 命名配对**：`NeoParser_JSON.meta.json` ↔ `NeoParser_JSON.dll`（加载器按「去 `.meta.json` 拼 `.dll`」解析，2026-08-11 曾因拼出 `.meta.dll` 全部加载失败，已修复）。
- **扩展名大小写**：`can_handle` 按小写字面比较（`Config.JSON` 判 false）；但 `FindParser` 会先小写化文件扩展名再查注册表，注册表键为 `capability().extensions` 原值（小写），两条路径一致。
- **插件日志**：NeoCore 是静态库，插件内 `CLogger`/`PluginLog` 依赖宿主注入——宿主 `GetProcAddress("SetPluginLogSink")` 注入 `LoggerLogSink`（带 `[NeoParser_JSON]` 前缀）；找不到符号自动跳过，插件日志回退 `CLogger`（spdlog default_logger 跨模块共享）。
- **部署失效**：改 meta.json/源码后需重新构建部署（`copy_if_different` 按内容差异复制）；若 EXE 正在运行，DLL 可能被占用阻塞复制。
- **`.properties` 归属**：本插件不声明 `.properties`；该扩展名由 `NeoParser_TXT` 与 `NeoParser_Properties` 同时声明，`PluginLoader::RegisterParser` 已按 `capability().priority` 仲裁（Properties 80 > TXT 60，✅ 2026-08-30），`.properties` 稳定归 `NeoParser_Properties`（见两者文档注意事项）。

## 相关文档

- NeoCore（接口 + PluginLoader）：`docs/Modules/NeoCore/README.md`、`docs/Modules/NeoCore/usage.md`
- 编辑器扩展：`docs/Modules/NeoEditorExtension_Parser_JSON/README.md`、`usage.md`
- 插件日志机制：`docs/Modules/CommonLoggerCPP/README.md`、`usage.md`
- 模块总索引：`docs/Modules/README.md`