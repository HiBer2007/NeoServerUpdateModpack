# NeoEditorExtension_Parser_SNBT 使用文档

## 快速开始

```powershell
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
# 产物: build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_SNBT.dll
#       build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_SNBT.meta.json
```

打开 NeoWorkspaceEditor 中的 `.snbt` 配置文件即可看到本扩展界面。

## 插件契约

### 1. `NeoEditorExtension_Parser_SNBT.meta.json`（逐字段，单行 JSON）

| 字段 | 类型 | 值 | 说明 |
|------|------|-----|------|
| `name` | string | `"SNBT Config Editor Extension"` | 显示名 |
| `version` | string | `"1.0.0"` | 版本号 |
| `dll` | string | `"NeoEditorExtension_Parser_SNBT.dll"` | 同目录 DLL 文件名 |
| `editor_type` | string | `"parser"` | 扩展族 |
| `extensions` | array[string] | `[".snbt"]` | 关联文件后缀 |
| `description` | string | `"SNBT配置文件编辑器，支持NBT键追踪"` | 描述 |

### 2. 接口实现要点

- `fileExtension()` 返回 `".snbt"`。
- `loadConfig()`：内容远端优先；`policy = syncRules.value("policy", "full_sync")`；键提取 = `extractSnbtKeys`：
  1. 先 `remove(QRegularExpression("#[^\n]*"))` 剥离 `#` 行注释；
  2. `nlohmann::json::parse(cleaned, nullptr, false)` 试探解析，非 `is_discarded()` 时取**顶层键**；
  3. 解析失败（`catch(...)` 或 discarded）回退：逐行正则 `^\s*([a-zA-Z_][a-zA-Z0-9_-]*)\s*:` 提取行首裸键。
- `saveSyncRules()`：写 `policy`；**仅** `config_merge` 时写 `tracked_keys`（数组）。
- `trackedLines(content, trackedKeys)`：每行先试引号键 `^\s*"([^"]+)"\s*:`，失败再试裸键 `^\s*([a-zA-Z_][a-zA-Z0-9_-]*)\s*:`；命中条件 = 行内键 == 追踪键、或 == 追踪键点分路径末段。1-based。
- `mergePreview()` 返回 `""`。

## 功能细节

### 编辑界面（共享 `ConfigEditor`）

| 区域 | 控件 | 内容 |
|------|------|------|
| 同步模式 | `QComboBox` | `full_sync - 完整同步` / `config_merge - 配置合并` / `line_by_line - 逐行同步` / `no_sync - 不同步` |
| 文件内容 (只读) | `QTextEdit`（只读，Consolas 9pt） | 远端或本地内容 |
| 追踪项 | `QTreeWidget`（表头 `键` / `追踪`）+ `全选/全不选` 按钮 | 键列表 |

- SNBT 写法兼容：键提取与行定位同时接受 JSON 风格引号键（`"Enable": 1b`）与 SNBT 裸键（`Enable: 1b`）。
- 与 `NeoParser_SNBT` 的协作：本扩展产出 `policy` + `tracked_keys`；保序写入（`SnbtPreservingWriter`）等解析/合并能力在配对解析器侧。

## 典型用法

同 JSON 族（见 [NeoEditorExtension_Parser_JSON/usage.md](NeoEditorExtension_Parser_JSON/usage.md) 的「典型用法」）：`EditorExtensionRegistry::scan` → `resolve("CreateConfigEditor")` → `configMap_[".snbt"]` 注册；`configEditorFor(".snbt")` 路由。编辑器菜单展示 `SNBT Config Editor Extension  v1.0.0`。

## 注意事项

- **导出符号必须完整**：`extern "C" __declspec(dllexport) ... CreateConfigEditor`。
- **仅顶层键（设计如此，当前未实现嵌套键追踪）**：`extractSnbtKeys` 取 JSON 解析的顶层键；嵌套 NBT 复合标签不展开为点分路径（与 JSON 扩展不同，SNBT 无点分合并），嵌套键追踪目前不在实现范围内。
- **裸键正则不含引号/特殊字符**：SNBT 值如字符串 `"value"` 含引号不影响键匹配（正则锚定行首键名冒号前）。
- **无 `line_by_line` 序列化**：策略选 `line_by_line` 时不写 `tracked_lines`。
- **配对关系**：界面 ≠ 解析；`NeoParser_SNBT` 需部署到 `parsers/`。
- 加载 API 为 `QLibrary::resolve`；meta 必须与 DLL 同目录。

## 相关文档

- [README.md](README.md) — 模块说明、依赖、构建集成
- 配对解析器：[NeoParser_SNBT](../NeoParser_SNBT/README.md)（含 `SnbtPreservingWriter`）
- 接口定义：[NeoCore](../NeoCore/README.md)（`IConfigEditorExtension`）
- 注册与加载：[GUIWorker](../GUIWorker/usage.md)、[NeoWorkspaceEditor](../NeoWorkspaceEditor/README.md)
- 总索引：`docs/Modules/README.md`