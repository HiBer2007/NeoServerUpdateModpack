# NeoParser_Properties 使用文档

## 快速开始

1. 构建（含部署）：`cmake --build build --target neo_deploy`。
2. 产物自动出现在 `deploy/parsers/`：`NeoParser_Properties.dll` + `NeoParser_Properties.meta.json`。
3. 加载器扫描 parsers 目录：找到 `NeoParser_Properties.meta.json` → 剥离 `.meta.json` 拼出 `NeoParser_Properties.dll`（二者必须同名同目录，否则报 `Plugin meta exists but DLL missing` 并跳过）。
4. 手工部署到任意 parsers/ 目录同样生效（遵循同名约定）。

## 插件契约

### IConfigParser 实现要点

接口定义：`modules/NeoCore/include/IConfigParser.h`。本插件实现：

| 接口方法 | 本插件行为 |
|----------|-----------|
| `ParserCapability capability() const` | name=Properties，扩展 `.properties`，FullSync/ConfigMerge/NoSync，priority=80 |
| `bool can_handle(const std::string& filepath) const` | 扩展名**小写化后**比较 == `.properties`（唯一做 tolower 的解析器） |
| `std::vector<ConfigEntry> parse_entries(const std::string& filepath)` | 逐行解析属性行→条目；空文件返回空列表 |
| `std::string merge_entries(...)` | 键值行合并（见「格式细节」） |
| `parse_lines` / `merge_lines` | **未重写**（接口默认空实现）；无行级追踪 |
| `std::vector<std::string> list_keys(const std::string& filepath)` | `parse_entries` 的 `key_path` 收集 |

`ConfigEntry`：`key_path`=属性键（trim 后），`remote_value`=属性值（去前导空白、去行内注释、右 trim），`local_value` 空、`is_tracked=false`。

### meta.json 字段（逐字段）

文件 `NeoParser_Properties.meta.json`（内容原样）：

```json
{
  "name": "Properties Config Parser",
  "version": "1.0.0",
  "dll": "NeoParser_Properties.dll",
  "capability": {
    "name": "Properties",
    "extensions": [".properties"],
    "supported_modes": ["full_sync", "config_merge", "no_sync"],
    "line_tracking": false,
    "priority": 80
  }
}
```

| 字段 | 值 | 含义 |
|------|-----|------|
| `name` | `"Properties Config Parser"` | 插件展示名 |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoParser_Properties.dll"` | 对应 DLL 文件名 |
| `capability.name` | `"Properties"` | 能力名（与 DLL 内 `capability()` 一致） |
| `capability.extensions` | `[".properties"]` | 声明支持的扩展名 |
| `capability.supported_modes` | `["full_sync", "config_merge", "no_sync"]` | 模式字符串（约定映射 `full_sync`→`TrackingMode::FullSync` 等；真值以 DLL 内 `capability()` 为准） |
| `capability.line_tracking` | `false` | 不支持行级追踪 |
| `capability.priority` | `80` | 声明优先级 |

> 加载器只校验 meta.json 合法性，注册以 DLL 实例 `capability()` 运行期返回值为准。

## 支持的格式细节

**属性行解析（parse_property_line）**

- 空行或行首（去空白）为 `#` / `!` → 跳过（注释/空行）。
- 分隔符，**三种形式**：
  1. 同时存在 `=` 与 `:` → 取更靠前者；
  2. 只有其一 → 取该者；
  3. 都没有 → **空白分隔**：首个空白前为键（trim 左侧），首个空白后的首个非空白起为值。
- 键：分隔符前部分 trim（`find_first_not_of(" \t")` / `find_last_not_of(" \t")`）。
- 值：去掉前导空白；行内注释处理——扫描未转义、不在双引号内的 `#`，**仅当 `#` 为值首字符或前一个字符是空格/制表符**时截断为注释；其余 `#` 保留为值的一部分；再右 trim 空白。
- 无分隔符且无空白 → 非属性行（跳过）。

**merge 语义（merge_entries）**

- 空处理：都空→`{}`；仅 remote 空→`local_content`；仅 local 空→`remote_content`。
- 遍历 local 每行：若该行可解析为属性行、键在 `tracked_keys` 中：
  - remote 存在同名键 → 整行替换为 remote 对应行（`remote_lines[remote_key_line[key]]`）；
  - remote 无同名键 → 保留 local 原行；
- 非 tracked 或非属性行 → 保留 local 原行。
- 输出保留 local 行顺序/注释/空行；local 以 `\n` 结尾则补一个 `\n`。

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

- **`__declspec(dllexport)` 必须保留**：缺失则 `GetProcAddress("CreateParser")` 失败；用 `dumpbin /exports NeoParser_Properties.dll` 确认 `CreateParser`/`SetPluginLogSink` 存在。
- **meta/DLL 命名配对**：`NeoParser_Properties.meta.json` ↔ `NeoParser_Properties.dll`（去 `.meta.json` 拼 `.dll`）。
- **✅ 已修复（2026-08-30）：`.properties` 归属已按 priority 仲裁**：`NeoParser_Properties` 与 `NeoParser_TXT` 都声明 `.properties`。`PluginLoader::RegisterParser` 现按 `capability().priority` 仲裁扩展名冲突——高于/等于则覆盖，低于则保留原注册者并打 Info 日志。本插件 `priority=80` > `NeoParser_TXT` 的 60，`.properties` **稳定归本插件**（不再依赖 `ScanDirectory` 目录迭代顺序）。
- **行内注释规则差异**：本插件要求 `#` 前为空白（或值首字符）才作为注释；`NeoParser_TXT` 对值中引号外任意位置 `#` 都截断。同一文件两插件解析结果可能不同。
- **空白分隔形式**：`key value`（无 `=`/`:`）也被解析为属性行——注意含空格值需依赖此形式的定义边界（首个空白后全部为值，但行内 `#` 注释规则仍生效）。
- **无行级追踪**：需要逐行追踪 `.properties` 请使用 `NeoParser_TXT`（`line_tracking=true`）。
- **扩展名大小写**：`can_handle` 会小写化扩展名再比较，`.PROPERTIES` 也能识别（与 `FindParser` 小写化行为一致）。
- **插件日志**：依赖宿主 `SetPluginLogSink` 注入（找不到符号自动跳过）；NeoCore 静态库，插件侧勿假定 `CLogger` 已初始化。
- **部署**：改源码/meta 后需重新构建部署；EXE 运行中 DLL 可能被占用阻塞复制。

## 相关文档

- NeoCore（接口 + PluginLoader）：`docs/Modules/NeoCore/README.md`、`usage.md`
- 编辑器扩展：`docs/Modules/NeoEditorExtension_Parser_Properties/README.md`、`usage.md`
- 同类插件（.properties 语义差异 + 行级能力）：`docs/Modules/NeoParser_TXT/README.md`、`usage.md`
- 插件日志机制：`docs/Modules/CommonLoggerCPP/README.md`、`usage.md`
- 模块总索引：`docs/Modules/README.md`