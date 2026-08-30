# NeoEditorExtension_Parser_Properties 使用文档

## 快速开始

```powershell
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
# 产物: build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_Properties.dll
#       build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_Properties.meta.json
```

打开 NeoWorkspaceEditor 中的 `.properties` 配置文件即可看到本扩展界面。

## 插件契约

### 1. `NeoEditorExtension_Parser_Properties.meta.json`（逐字段，单行 JSON）

| 字段 | 类型 | 值 | 说明 |
|------|------|-----|------|
| `name` | string | `"Properties Config Editor Extension"` | 显示名 |
| `version` | string | `"1.0.0"` | 版本号 |
| `dll` | string | `"NeoEditorExtension_Parser_Properties.dll"` | 同目录 DLL 文件名 |
| `editor_type` | string | `"parser"` | 扩展族 |
| `extensions` | array[string] | `[".properties"]` | 关联文件后缀 |
| `description` | string | `"properties 配置文件编辑器，支持键追踪"` | 描述 |

> ✅ 已修复（2026-08-30）：`NeoEditorExtension_Parser_TXT` 的 meta 也声明了 `".properties"`（连同 `".txt"`/`".cfg"`），但注册表（EditorExtensionRegistry）新增 `priority` 字段（meta.json 可选，默认 0）在 `configMap_` 冲突时**高者胜**——本扩展 meta `priority=20` > TXT 扩展 `priority=10`，`configMap_[".properties"]` **稳定归本扩展**，不再取决于扫描顺序。

### 2. 接口实现要点

- `fileExtension()` 返回 `".properties"`。
- `loadConfig()`：内容远端优先；`policy = syncRules.value("policy", "full_sync")`；`std::getline` 逐行走 `parse_property_line` 提取键列表，`syncRules["tracked_keys"]`（字符串数组）预置勾选。
- `saveSyncRules()`：写 `policy`；**仅** `config_merge` 时写 `tracked_keys`（数组）。
- `mergePreview()` 返回 `""`。
- `trackedLines(content, trackedKeys)`：逐行解析键，与追踪键**精确相等**才计入（无末段/前缀模糊匹配）。1-based。

### 3. 键解析规则（`parse_property_line`）

| 规则 | 行为 |
|------|------|
| 分隔符 | `=` 与 `:` 均可；两者同时存在取**位置靠前**者 |
| 注释 | 行首（去空白后）为 `#` 或 `!` → 跳过 |
| 空行 | 全空白行 → 跳过 |
| 键裁剪 | 键前后空白（` \t`）剥离 |
| 无分隔符行 | 跳过（不成键值对） |

## 功能细节

### 编辑界面（共享 `ConfigEditor`）

| 区域 | 控件 | 内容 |
|------|------|------|
| 同步模式 | `QComboBox` | `full_sync - 完整同步` / `config_merge - 配置合并` / `line_by_line - 逐行同步` / `no_sync - 不同步` |
| 文件内容 (只读) | `QTextEdit`（只读，Consolas 9pt） | 远端或本地内容 |
| 追踪项 | `QTreeWidget`（表头 `键` / `追踪`）+ `全选/全不选` 按钮 | 键列表（去空白后的键名） |

- 键追踪为**精确匹配**：`trackedLines` 不做点分/末段匹配，追踪键必须与文件中的键字符串完全一致。
- 与 `NeoParser_Properties` 的协作：本扩展产出 `policy` + `tracked_keys`；配对解析器按追踪键执行合并。

## 典型用法

同 JSON 族（见 [NeoEditorExtension_Parser_JSON/usage.md](NeoEditorExtension_Parser_JSON/usage.md) 的「典型用法」）：`EditorExtensionRegistry::scan` → `resolve("CreateConfigEditor")` → `configMap_[".properties"]` 注册；`configEditorFor(".properties")` 路由。编辑器菜单展示 `Properties Config Editor Extension  v1.0.0`。

## 注意事项

- **导出符号必须完整**：`extern "C" __declspec(dllexport) ... CreateConfigEditor`。
- **`.properties` 键冲突已按 priority 仲裁（✅ 已修复，2026-08-30）**：与 TXT 扩展注册同名键（见上）——本扩展 meta `priority=20` 高于 TXT 扩展的 `10`，`configMap_[".properties"]` 稳定归本扩展，无需再通过避免混用扩展名声明或调整扫描顺序来保证接管。
- **精确匹配语义**：`trackedLines` 不做嵌套/模糊匹配；追踪键拼写不一致即不命中。
- **无 `line_by_line` 序列化**：策略选 `line_by_line` 时不写 `tracked_lines`。
- **配对关系**：界面 ≠ 解析；`NeoParser_Properties` 需部署到 `parsers/`。
- 加载 API 为 `QLibrary::resolve`；meta 必须与 DLL 同目录。

## 相关文档

- [README.md](README.md) — 模块说明、依赖、构建集成
- 配对解析器：[NeoParser_Properties](../NeoParser_Properties/README.md)
- 接口定义：[NeoCore](../NeoCore/README.md)（`IConfigEditorExtension`）
- 注册与加载：[GUIWorker](../GUIWorker/usage.md)、[NeoWorkspaceEditor](../NeoWorkspaceEditor/README.md)
- 总索引：`docs/Modules/README.md`