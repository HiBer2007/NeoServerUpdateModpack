# NeoParser_TOML 使用文档

## 快速开始

1. 构建（含部署）：`cmake --build build --target neo_deploy`。
2. 产物自动出现在 `deploy/parsers/`：`NeoParser_TOML.dll` + `NeoParser_TOML.meta.json`。
3. 加载器扫描 parsers 目录：找到 `NeoParser_TOML.meta.json` → 剥离 `.meta.json` 拼出 `NeoParser_TOML.dll`（二者必须同名同目录，否则报 `Plugin meta exists but DLL missing` 并跳过）。
4. 手工部署到任意 parsers/ 目录同样生效（遵循同名约定）。

## 插件契约

### IConfigParser 实现要点

接口定义：`modules/NeoCore/include/IConfigParser.h`。本插件实现：

| 接口方法 | 本插件行为 |
|----------|-----------|
| `ParserCapability capability() const` | name=TOML，扩展 `.toml`，FullSync/ConfigMerge/NoSync，priority=80 |
| `bool can_handle(const std::string& filepath) const` | 扩展名字面比较（大小写敏感）== `.toml` |
| `std::vector<ConfigEntry> parse_entries(const std::string& filepath)` | `toml::parse_file(filepath)` 解析→平面化；`toml::parse_error`/异常返回空列表 |
| `std::string merge_entries(...)` | 见「格式细节·merge」 |
| `parse_lines` / `merge_lines` | **未重写**（接口默认空实现）；无行级追踪 |
| `std::vector<std::string> list_keys(const std::string& filepath)` | `parse_entries` 的 `key_path` 收集 |

`ConfigEntry.remote_value` 生成（`node_to_string`）：string/integer/floating_point/boolean 直接输出文本；**其余类型（含数组）输出字面量 `"<complex>"`**。

### meta.json 字段（逐字段）

文件 `NeoParser_TOML.meta.json`（内容原样）：

```json
{
  "name": "TOML Config Parser",
  "version": "1.0.0",
  "dll": "NeoParser_TOML.dll",
  "capability": {
    "name": "TOML",
    "extensions": [".toml"],
    "supported_modes": ["full_sync", "config_merge", "no_sync"],
    "line_tracking": false,
    "priority": 80
  }
}
```

| 字段 | 值 | 含义 |
|------|-----|------|
| `name` | `"TOML Config Parser"` | 插件展示名 |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoParser_TOML.dll"` | 对应 DLL 文件名 |
| `capability.name` | `"TOML"` | 能力名（与 DLL 内 `capability()` 一致） |
| `capability.extensions` | `[".toml"]` | 声明支持的扩展名 |
| `capability.supported_modes` | `["full_sync", "config_merge", "no_sync"]` | 模式字符串（约定映射 `full_sync`→`TrackingMode::FullSync` 等；真值以 DLL 内 `capability()` 为准） |
| `capability.line_tracking` | `false` | 不支持行级追踪 |
| `capability.priority` | `80` | 声明优先级 |

> 加载器只校验 meta.json 合法性，注册以 DLL 实例 `capability()` 运行期返回值为准。

## 支持的格式细节

**parse 语义（parse_entries）**

- `toml::parse_file(filepath)` 失败（`toml::parse_error` 或其它 `std::exception`）→ 空列表。
- 平面化规则（`flatten_toml`）：
  - table：`a.b` 点路径，递归展开。
  - array of tables：`servers[0]` 下标路径，逐元素递归展开。
  - 标量（string/integer/floating_point/boolean）：叶子条目，值如上述 `node_to_string`。
- 数组（非 table 数组）作为叶子时值为 `"<complex>"`（当前实现占位，表示"复杂值，未展开"）。

**merge 语义（merge_entries）**

- 空处理：都空→`{}`；仅 remote 空→`local_content`；仅 local 空→`remote_content`。
- 解析失败：local 失败→`remote_content`；remote 失败→`local_content`。
- 正常：`result = local`；对每个 tracked key 按 `.` 分段（`split_path`）在 remote 导航（`navigate_toml`，每段必须是 table），在 result 中定位父 table；按 remote 值类型执行 `parent->insert_or_assign(last, value)`——支持 string/integer/floating_point/boolean/table/array 六类；最终 `oss << result`（tomlplusplus 默认序列化）。
- **注意**：合并输出为 tomlplusplus 重序列化结果，原注释不保留。

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

- **`__declspec(dllexport)` 必须保留**：缺失则 `GetProcAddress("CreateParser")` 失败；用 `dumpbin /exports NeoParser_TOML.dll` 确认 `CreateParser`/`SetPluginLogSink` 存在。
- **meta/DLL 命名配对**：`NeoParser_TOML.meta.json` ↔ `NeoParser_TOML.dll`（去 `.meta.json` 拼 `.dll`）。
- **数组值展示为 `<complex>`**：TOML 数组作为叶子条目时 `remote_value` 是占位文本，GUI 键列表看到的是占位符而非展开内容；partial 合并仍可按数组类型写回（insert_or_assign 支持 array）。
- **扩展名大小写**：`can_handle` 按小写字面比较；`FindParser` 小写化后查注册表，行为一致。
- **插件日志**：依赖宿主 `SetPluginLogSink` 注入（找不到符号自动跳过）；NeoCore 静态库，插件侧勿假定 `CLogger` 已初始化。
- **部署**：改源码/meta 后需重新构建部署；EXE 运行中 DLL 可能被占用阻塞复制。

## 相关文档

- NeoCore（接口 + PluginLoader）：`docs/Modules/NeoCore/README.md`、`usage.md`
- 编辑器扩展：`docs/Modules/NeoEditorExtension_Parser_TOML/README.md`、`usage.md`
- 插件日志机制：`docs/Modules/CommonLoggerCPP/README.md`、`usage.md`
- 模块总索引：`docs/Modules/README.md`