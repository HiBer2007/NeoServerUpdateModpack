# NeoParser_YAML 使用文档

## 快速开始

1. 构建（含部署）：`cmake --build build --target neo_deploy`。
2. 产物自动出现在 `deploy/parsers/`：`NeoParser_YAML.dll` + `NeoParser_YAML.meta.json`。
3. 加载器扫描 parsers 目录：找到 `NeoParser_YAML.meta.json` → 剥离 `.meta.json` 拼出 `NeoParser_YAML.dll`（二者必须同名同目录，否则报 `Plugin meta exists but DLL missing` 并跳过）。
4. 手工部署到任意 parsers/ 目录同样生效（遵循同名约定）。

## 插件契约

### IConfigParser 实现要点

接口定义：`modules/NeoCore/include/IConfigParser.h`。本插件实现：

| 接口方法 | 本插件行为 |
|----------|-----------|
| `ParserCapability capability() const` | name=YAML，扩展 `.yml`/`.yaml`，FullSync/ConfigMerge/NoSync，priority=90 |
| `bool can_handle(const std::string& filepath) const` | 扩展名字面比较（大小写敏感）== `.yml` 或 `.yaml` |
| `std::vector<ConfigEntry> parse_entries(const std::string& filepath)` | `YAML::Load` 全量解析→平面化；异常/空节点返回空列表 |
| `std::string merge_entries(...)` | 见「格式细节·merge」 |
| `parse_lines` / `merge_lines` | **未重写**（接口默认空实现）；无行级追踪 |
| `std::vector<std::string> list_keys(const std::string& filepath)` | `parse_entries` 的 `key_path` 收集 |

`ConfigEntry.remote_value`：标量取 `as<std::string>()`；序列/映射经 `YAML::Emitter` 并以 `SetStringFormat(YAML::EMITTER_MANIP::Flow)` 输出为 Flow 风格字符串。

### meta.json 字段（逐字段）

文件 `NeoParser_YAML.meta.json`（内容原样）：

```json
{
  "name": "YAML Config Parser",
  "version": "1.0.0",
  "dll": "NeoParser_YAML.dll",
  "capability": {
    "name": "YAML",
    "extensions": [".yml", ".yaml"],
    "supported_modes": ["full_sync", "config_merge", "no_sync"],
    "line_tracking": false,
    "priority": 90
  }
}
```

| 字段 | 值 | 含义 |
|------|-----|------|
| `name` | `"YAML Config Parser"` | 插件展示名 |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoParser_YAML.dll"` | 对应 DLL 文件名 |
| `capability.name` | `"YAML"` | 能力名（与 DLL 内 `capability()` 一致） |
| `capability.extensions` | `[".yml", ".yaml"]` | 声明支持的扩展名 |
| `capability.supported_modes` | `["full_sync", "config_merge", "no_sync"]` | 模式字符串（约定映射 `full_sync`→`TrackingMode::FullSync` 等；真值以 DLL 内 `capability()` 为准） |
| `capability.line_tracking` | `false` | 不支持行级追踪 |
| `capability.priority` | `90` | 声明优先级 |

> 加载器只校验 meta.json 合法性，注册以 DLL 实例 `capability()` 运行期返回值为准。

## 支持的格式细节

**parse 语义（parse_entries）**

- 空文件/空节点/未定义节点 → 空列表；`YAML::Load` 抛异常 → 空列表。
- 平面化规则（`flatten_yaml`）：
  - 映射（Map）：`a.b` 点路径；值为嵌套映射 → 递归展开；值为序列 → **单条目**（`key_path`=该路径，`remote_value`=Flow 风格序列字符串，不逐下标展开）；值为标量/null → 叶子条目。
  - 顶层序列（Sequence）：`[0]`、`[1]` 下标路径；元素为映射 → 继续递归，其余为叶子条目。

**merge 语义（merge_entries）**

- 空处理：都空→`{}`；仅 remote 空→`local_content`；仅 local 空→`remote_content`。
- 解析失败：local 失败→`remote_content`；remote 失败→`local_content`。
- 正常：`result = local`；对每个 tracked key 按 `.` 分段（`split_path`）在 remote 导航（`navigate_yaml`），命中且已定义则 `set_yaml_at_path` 写入 result；最终经 `YAML::Emitter` 输出（默认块风格）。
- **注意**：合并输出为 yaml-cpp 重序列化结果——原注释、引号风格、顺序（映射键序）不保证保留。

## 典型用法

```cpp
#include <PluginLoader.h>
#include <IConfigParser.h>
#include <QCoreApplication>

NeoCore::PluginLoader loader;
loader.ScanDirectory(
    (QCoreApplication::applicationDirPath() + "/parsers").toStdString());

NeoCore::IConfigParser* parser = loader.FindParser(filepath); // 小写扩展名匹配
if (!parser) { /* 未找到对应解析器 */ }

const auto cap = parser->capability();
auto entries = parser->parse_entries(filepath);
auto keys = parser->list_keys(filepath);
std::string merged = parser->merge_entries(filepath,
    trackedKeys, remoteContent, localContent);
```

参考实现：`modules/GUIWorker/src/config_file_editor.cpp`（`ScanDirectory(applicationDirPath()/parsers)` + `FindParser`）；加载细节见 `modules/NeoCore/src/plugin_loader.cpp`。

## 注意事项

- **`__declspec(dllexport)` 必须保留**：缺失则 `GetProcAddress("CreateParser")` 失败；用 `dumpbin /exports NeoParser_YAML.dll` 确认 `CreateParser`/`SetPluginLogSink` 存在。
- **meta/DLL 命名配对**：`NeoParser_YAML.meta.json` ↔ `NeoParser_YAML.dll`（去 `.meta.json` 拼 `.dll`）。
- **扩展名大小写**：`can_handle` 按小写字面比较；`FindParser` 小写化后查注册表，行为一致。
- **合并丢排版**：merge 走 yaml-cpp 重序列化，注释与原始格式不保留——若需保序/保注释请评估 `NeoParser_JSON`（JSONC 兼容注释）或行级解析器（TXT）。
- **插件日志**：依赖宿主 `SetPluginLogSink` 注入（找不到符号自动跳过）；NeoCore 静态库，插件侧勿假定 `CLogger` 已初始化。
- **部署**：改源码/meta 后需重新构建部署；EXE 运行中 DLL 可能被占用阻塞复制。

## 相关文档

- NeoCore（接口 + PluginLoader）：`docs/Modules/NeoCore/README.md`、`usage.md`
- 编辑器扩展：`docs/Modules/NeoEditorExtension_Parser_YAML/README.md`、`usage.md`
- 插件日志机制：`docs/Modules/CommonLoggerCPP/README.md`、`usage.md`
- 模块总索引：`docs/Modules/README.md`