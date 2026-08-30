# NeoParser_TXT 使用文档

## 快速开始

1. 构建（含部署）：`cmake --build build --target neo_deploy`。
2. 产物自动出现在 `deploy/parsers/`：`NeoParser_TXT.dll` + `NeoParser_TXT.meta.json`。
3. 加载器扫描 parsers 目录：找到 `NeoParser_TXT.meta.json` → 剥离 `.meta.json` 拼出 `NeoParser_TXT.dll`（二者必须同名同目录，否则报 `Plugin meta exists but DLL missing` 并跳过）。
4. 手工部署到任意 parsers/ 目录同样生效（遵循同名约定）。

## 插件契约

### IConfigParser 实现要点

接口定义：`modules/NeoCore/include/IConfigParser.h`。本插件实现：

| 接口方法 | 本插件行为 |
|----------|-----------|
| `ParserCapability capability() const` | name=TXT，扩展 `.txt`/`.properties`/`.cfg`，**FullSync/LineByLine/NoSync**，`supports_line_tracking=true`，priority=60 |
| `bool can_handle(const std::string& filepath) const` | 扩展名字面比较（大小写敏感）== `.txt` / `.properties` / `.cfg` |
| `std::vector<ConfigEntry> parse_entries(const std::string& filepath)` | **仅 `.properties` 文件**产出键值条目；`.txt`/`.cfg` 返回空列表 |
| `std::string merge_entries(...)` | 键值行合并（见「格式细节·merge_entries」） |
| `std::vector<LineEntry> parse_lines(const std::string& filepath)` | 逐行产出 `LineEntry`（`line_number` 从 1 开始，`remote_text`=行原文） |
| `std::string merge_lines(...)` | 按行号替换被追踪行（见「格式细节·merge_lines」） |
| `std::vector<std::string> list_keys(const std::string& filepath)` | `parse_entries` 的 `key_path` 收集（仅 properties 有内容） |

### meta.json 字段（逐字段）

文件 `NeoParser_TXT.meta.json`（内容原样）：

```json
{
  "name": "TXT Config Parser",
  "version": "1.0.0",
  "dll": "NeoParser_TXT.dll",
  "capability": {
    "name": "TXT",
    "extensions": [".txt", ".properties", ".cfg"],
    "supported_modes": ["full_sync", "line_by_line", "no_sync"],
    "line_tracking": true,
    "priority": 60
  }
}
```

| 字段 | 值 | 含义 |
|------|-----|------|
| `name` | `"TXT Config Parser"` | 插件展示名 |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoParser_TXT.dll"` | 对应 DLL 文件名 |
| `capability.name` | `"TXT"` | 能力名（与 DLL 内 `capability()` 一致） |
| `capability.extensions` | `[".txt", ".properties", ".cfg"]` | 声明支持的扩展名 |
| `capability.supported_modes` | `["full_sync", "line_by_line", "no_sync"]` | 模式字符串（约定映射 `full_sync`→`TrackingMode::FullSync`、`line_by_line`→`TrackingMode::LineByLine`、`no_sync`→`TrackingMode::NoSync`；真值以 DLL 内 `capability()` 为准） |
| `capability.line_tracking` | `true` | **支持行级追踪**（仅此解析器为 true） |
| `capability.priority` | `60` | 声明优先级（6 个解析器中最低） |

> 加载器只校验 meta.json 合法性，注册以 DLL 实例 `capability()` 运行期返回值为准。

## 支持的格式细节

### 键值行解析（parse_entries / merge_entries，properties 语义）

`parse_property_line` 规则：

- 空行或行首（去空白）为 `#` / `!` → 跳过（注释/空行）。
- 分隔符：取 `=` 与 `:` 中**更靠前者**；键去首尾空白，值去前导空白。
- 行内注释：值中**未转义且不在双引号内**的 `#`（在引号外任一位置出现即截断）→ 截为注释；值再右去空白。
- 无分隔符 → 非属性行（跳过）。

`merge_entries`（键值合并，行级）：

- 空处理：都空→`{}`；仅 remote 空→`local_content`；仅 local 空→`remote_content`。
- 遍历 local 每行：若该行可解析为属性行、键在 `tracked_keys` 中、且 remote 存在同名键 → **整行替换为 remote 对应行**（`remote_lines[remote_key_line[key]]`）；否则保留 local 原行。
- 输出保留 local 的行顺序、注释、空行；local 以 `\n` 结尾则补一个 `\n`。

### 行级解析/合并（parse_lines / merge_lines）

- `parse_lines`：`split_lines` 逐行（`std::getline`），`line_number = i + 1`（1 起始），`remote_text`=行原文，`local_text` 空、`is_tracked=false`。
- `merge_lines`：
  - tracked 行号集（忽略 ≤0 的行号）；
  - 逐行（`max(remote_lines, local_lines)` 行数内）：tracked 且 remote 有该行 → 取 remote 行；否则取 local 行（local 不足时补 remote 行）；
  - 跳过「不需要输出」的空行（`!output_line.empty() || i < local_lines.size() || is_tracked` 才输出）；local 以 `\n` 结尾则补一个 `\n`。

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

// 键值路径（仅 .properties 有内容）
auto entries = parser->parse_entries(filepath);
std::string merged = parser->merge_entries(filepath, trackedKeys,
    remoteContent, localContent);

// 行级路径（.txt/.properties/.cfg 均可用）
auto lines = parser->parse_lines(filepath);
std::string mergedLines = parser->merge_lines(filepath, trackedLineNums,
    remoteContent, localContent);
```

参考实现：`modules/GUIWorker/src/config_file_editor.cpp`（`ScanDirectory(applicationDirPath()/parsers)` + `FindParser`）；加载细节见 `modules/NeoCore/src/plugin_loader.cpp`。

## 注意事项

- **`__declspec(dllexport)` 必须保留**：缺失则 `GetProcAddress("CreateParser")` 失败；用 `dumpbin /exports NeoParser_TXT.dll` 确认 `CreateParser`/`SetPluginLogSink` 存在。
- **meta/DLL 命名配对**：`NeoParser_TXT.meta.json` ↔ `NeoParser_TXT.dll`（去 `.meta.json` 拼 `.dll`）。
- **✅ 已修复（2026-08-30）：`.properties` 归属已按 priority 仲裁**：`NeoParser_TXT` 与 `NeoParser_Properties` 都声明 `.properties`。`PluginLoader::RegisterParser` 现按 `capability().priority` 仲裁扩展名冲突——高于/等于则覆盖，低于则保留原注册者并打 Info 日志；`NeoParser_Properties` `priority=80` 高于本插件 `60`，故 `.properties` **稳定归 Properties 插件**（本插件仍经 `.txt`/`.cfg` 提供行级能力）。
- **parse_entries 仅限 .properties**：`.txt`/`.cfg` 走 `parse_entries` 会得到空条目，但其行级能力（`parse_lines`/`merge_lines`）不受影响——使用方需根据文件类型选择 key 或 line 模式。
- **行内注释规则差异**：本插件（TXT）对值中**任意位置**（引号外、未转义）的 `#` 都截断为注释；`NeoParser_Properties` 要求 `#` 前为空白才截断——同一文件被两个插件解析结果可能不同。
- **扩展名大小写**：`can_handle` 按小写字面比较；`FindParser` 小写化后查注册表，行为一致。
- **插件日志**：依赖宿主 `SetPluginLogSink` 注入（找不到符号自动跳过）；NeoCore 静态库，插件侧勿假定 `CLogger` 已初始化。
- **部署**：改源码/meta 后需重新构建部署；EXE 运行中 DLL 可能被占用阻塞复制。

## 相关文档

- NeoCore（接口 + PluginLoader）：`docs/Modules/NeoCore/README.md`、`usage.md`
- 编辑器扩展：`docs/Modules/NeoEditorExtension_Parser_TXT/README.md`、`usage.md`
- 同类插件（.properties 语义差异）：`docs/Modules/NeoParser_Properties/README.md`、`usage.md`
- 插件日志机制：`docs/Modules/CommonLoggerCPP/README.md`、`usage.md`
- 模块总索引：`docs/Modules/README.md`