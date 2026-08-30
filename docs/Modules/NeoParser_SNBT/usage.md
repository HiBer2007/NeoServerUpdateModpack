# NeoParser_SNBT 使用文档

## 快速开始

1. 构建（含部署）：`cmake --build build --target neo_deploy`（构建前须先 `call vcvars64.bat`，见根 CMakeLists/AGENTS.md）。
2. 产物自动出现在 `deploy/parsers/`：`NeoParser_SNBT.dll` + `NeoParser_SNBT.meta.json`。
3. 加载器扫描 parsers 目录：找到 `NeoParser_SNBT.meta.json` → 剥离 `.meta.json` 拼出 `NeoParser_SNBT.dll`（二者必须同名同目录，否则报 `Plugin meta exists but DLL missing` 并跳过）。
4. 手工部署到任意 parsers/ 目录同样生效（遵循同名约定）。

## 插件契约

### IConfigParser 实现要点

接口定义：`modules/NeoCore/include/IConfigParser.h`。本插件实现：

| 接口方法 | 本插件行为 |
|----------|-----------|
| `ParserCapability capability() const` | name=SNBT，扩展 `.snbt`，FullSync/ConfigMerge/NoSync，priority=70 |
| `bool can_handle(const std::string& filepath) const` | 扩展名字面比较（大小写敏感）== `.snbt` |
| `std::vector<ConfigEntry> parse_entries(const std::string& filepath)` | `nbtcpp::snbt::parse(content)`；根必须为 Compound，否则空列表；Compound 递归平面化 |
| `std::string merge_entries(...)` | 委托 `SnbtPreserving::merge(local_content, remote_content, tracked_keys)`（见「格式细节·merge」） |
| `parse_lines` / `merge_lines` | **未重写**（接口默认空实现）；无行级追踪 |
| `std::vector<std::string> list_keys(const std::string& filepath)` | `parse_entries` 的 `key_path` 收集 |

`ConfigEntry.remote_value` 生成（`tag_value_to_string`，按 `nbtcpp::NbtTagType`）：

| NBT 类型 | 输出格式 | 示例 |
|----------|----------|------|
| String | `"value"`（带双引号） | `"Hello"` |
| Byte | `N` + `b` | `1b` |
| Short | `N` + `s` | `2s` |
| Int | `N` | `3` |
| Long | `N` + `L` | `4L` |
| Float | `N` + `f` | `0.5f` |
| Double | `N` + `d` | `0.5d` |
| 其它（List/ByteArray/IntArray/LongArray 等） | `nbtcpp::snbt::to_snbt(tag, nbtcpp::snbt::Options::default_options())` | — |

### meta.json 字段（逐字段）

文件 `NeoParser_SNBT.meta.json`（内容原样）：

```json
{
  "name": "SNBT Config Parser",
  "version": "1.0.0",
  "dll": "NeoParser_SNBT.dll",
  "capability": {
    "name": "SNBT",
    "extensions": [".snbt"],
    "supported_modes": ["full_sync", "config_merge", "no_sync"],
    "line_tracking": false,
    "priority": 70
  }
}
```

| 字段 | 值 | 含义 |
|------|-----|------|
| `name` | `"SNBT Config Parser"` | 插件展示名 |
| `version` | `"1.0.0"` | 插件版本 |
| `dll` | `"NeoParser_SNBT.dll"` | 对应 DLL 文件名 |
| `capability.name` | `"SNBT"` | 能力名（与 DLL 内 `capability()` 一致） |
| `capability.extensions` | `[".snbt"]` | 声明支持的扩展名 |
| `capability.supported_modes` | `["full_sync", "config_merge", "no_sync"]` | 模式字符串（约定映射 `full_sync`→`TrackingMode::FullSync` 等；真值以 DLL 内 `capability()` 为准） |
| `capability.line_tracking` | `false` | 不支持行级追踪 |
| `capability.priority` | `70` | 声明优先级 |

> 加载器只校验 meta.json 合法性，注册以 DLL 实例 `capability()` 运行期返回值为准。

## 支持的格式细节

**parse 语义（parse_entries）**

- 空文件 → 空列表；`nbtcpp::snbt::parse` 抛异常或返回空 → 空列表。
- 根节点类型必须为 `nbtcpp::NbtTagType::Compound`，否则返回空列表（不解析裸 List/标量根）。
- 平面化规则（`flatten_nbt`）：Compound 逐子节点递归；嵌套 Compound 以 `a.b` 点路径展开；非 Compound 子节点为叶子条目（值见上表）。

**merge 语义（merge_entries → `SnbtPreserving::merge`）**

`snbt_preserving_writer.cpp` 实现的行级保序合并：

1. **空处理**：都空→`{}`；仅 remote 空→`local_content`；仅 local 空→`remote_content`（与其它解析器一致，在 `merge_entries` 内先判空）。
2. **词法化（tokenize）**：逐行切分，行分类 `TokenType { CommentLine, KeyValueLine, StructuralLine, BlankLine, Unknown }`；`TokenLine { type, line_number, raw_text, indent, key, value, trailing_comment }`：
   - 首字符 `#` → CommentLine；
   - 仅含括号 `{ } [ ]` 与空白 → StructuralLine；
   - 顶层（括号深度 0）出现 `:` → KeyValueLine（键去首尾空白及两侧引号）；
   - 其余 → Unknown。
3. **远端值表**：`build_remote_map` 收集 remote 所有 KeyValueLine 的 `key → value`（value 为 `:` 后原始段）。
4. **逐本地行输出**：
   - 若该行是 KeyValueLine、键在 `trackedKeys` 中、且 remote 存在同名键 → 输出 `indent + key + ": " + remote_val`（远端值去前导空白）——**注意：被替换行不保留行尾注释（`trailing_comment` 字段当前未参与输出）**；
   - 否则 → 原样输出 `raw_text`（注释、结构行、空行、未追踪键值行全部保留）；
   - 若 localContent 以 `\n` 结尾，末尾补一个 `\n`。

**保序特性**：本地文件的顺序、缩进、注释、结构行逐字保留，仅被追踪键的值被远端覆盖；不重排、不重序列化。

> 注意：保序合并按**行**匹配（原始 `key: value` 对），键匹配前提是本地/远端该键都以「顶层冒号键值行」形式书写；单行内联复合值（如 `{a:1,b:2}` 写在一行）不会被当作可替换目标。

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
    trackedKeys, remoteContent, localContent); // 保序合并
```

参考实现：`modules/GUIWorker/src/config_file_editor.cpp`（`ScanDirectory(applicationDirPath()/parsers)` + `FindParser`）；加载细节见 `modules/NeoCore/src/plugin_loader.cpp`。

## 注意事项

- **`__declspec(dllexport)` 必须保留**：缺失则 `GetProcAddress("CreateParser")` 失败；用 `dumpbin /exports NeoParser_SNBT.dll` 确认 `CreateParser`/`SetPluginLogSink` 存在。
- **meta/DLL 命名配对**：`NeoParser_SNBT.meta.json` ↔ `NeoParser_SNBT.dll`（去 `.meta.json` 拼 `.dll`）。
- **构建前置**：nbtcpp 依赖 MSVC 头文件环境，必须先 `vcvars64.bat` 再 cmake，否则 C1083（缺 stddef.h/stdio.h）。
- **保序合并的匹配前提**：可替换目标必须是「顶层冒号键值行」；被替换行不保留行尾注释（当前实现）；合并是行级的，键重复（同名多次出现）时 remote 同名键取**最后一个**（`build_remote_map` 覆盖写）。
- **解析范围**：根非 Compound 的 `.snbt`（如纯 List/标量）parse_entries 返回空；二进制 NBT（.dat）不适用。
- **扩展名大小写**：`can_handle` 按小写字面比较；`FindParser` 小写化后查注册表，行为一致。
- **插件日志**：依赖宿主 `SetPluginLogSink` 注入（找不到符号自动跳过）；NeoCore 静态库，插件侧勿假定 `CLogger` 已初始化。
- **部署**：改源码/meta 后需重新构建部署；EXE 运行中 DLL 可能被占用阻塞复制。

## 相关文档

- NeoCore（接口 + PluginLoader）：`docs/Modules/NeoCore/README.md`、`usage.md`
- 编辑器扩展：`docs/Modules/NeoEditorExtension_Parser_SNBT/README.md`、`usage.md`
- 插件日志机制：`docs/Modules/CommonLoggerCPP/README.md`、`usage.md`
- 模块总索引：`docs/Modules/README.md`