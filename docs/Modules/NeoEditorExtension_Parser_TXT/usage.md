# NeoEditorExtension_Parser_TXT 使用文档

## 快速开始

```powershell
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
# 产物: build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_TXT.dll
#       build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_TXT.meta.json
```

打开 NeoWorkspaceEditor 中的 `.txt` / `.properties` / `.cfg` 配置文件即可看到本扩展界面（追踪项表头为 `行号`）。

## 插件契约

### 1. `NeoEditorExtension_Parser_TXT.meta.json`（逐字段，单行 JSON）

| 字段 | 类型 | 值 | 说明 |
|------|------|-----|------|
| `name` | string | `"TXT Config Editor Extension"` | 显示名 |
| `version` | string | `"1.0.0"` | 版本号 |
| `dll` | string | `"NeoEditorExtension_Parser_TXT.dll"` | 同目录 DLL 文件名 |
| `editor_type` | string | `"parser"` | 扩展族 |
| `extensions` | array[string] | `[".txt", ".properties", ".cfg"]` | 关联文件后缀 |
| `description` | string | `"TXT/properties/cfg配置文件编辑器，支持行号追踪"` | 描述 |

> ✅ 已修复（2026-08-30）：`".properties"` 与 `NeoEditorExtension_Parser_Properties`（meta `extensions: [".properties"]`）重叠已按 `priority` 仲裁——注册表（EditorExtensionRegistry）新增 `priority` 字段（meta.json 可选，默认 0），`configMap_` 冲突时高者胜：本扩展 meta `priority=10`、Properties 扩展 `priority=20`，故 `.properties` **稳定归 Properties 扩展**，本扩展经 `".txt"`/`".cfg"` 路由。

### 2. 接口实现要点

- `fileExtension()` 返回 `".txt"`；`createEditor()` 立即 `setLineMode(true)`（键树切为 `行号` / `追踪` 双列表头）。
- `loadConfig()`：内容远端优先；`policy = syncRules.value("policy", "full_sync")`；从 `syncRules["tracked_lines"]`（仅接受 `is_number_integer()` 元素）读行号数组 → `ce->setTrackedLines(lines)`（按当前内容行数建行号列表，已追踪行预勾选）。
- `saveSyncRules()`：写 `policy`；**仅** `line_by_line` 时收集 `ce->trackedLines()`（勾选行号）写入 `tracked_lines`。
- `mergePreview()` 返回 `""`；`trackedLines(content, trackedKeys)`（键定位版）未重写，返回空 → 宿主回退整行标记。
- 行模式与「逐行同步」策略严格配对：本扩展粒度是行，不是键。

## 功能细节

### 编辑界面（共享 `ConfigEditor`，行模式）

| 区域 | 控件 | 内容 |
|------|------|------|
| 同步模式 | `QComboBox` | `full_sync - 完整同步` / `config_merge - 配置合并` / `line_by_line - 逐行同步` / `no_sync - 不同步` |
| 文件内容 (只读) | `QTextEdit`（只读，Consolas 9pt） | 远端或本地内容 |
| 追踪项 | `QTreeWidget`（表头 `行号` / `追踪`）+ `全选/全不选` 按钮 | 按内容行数列出 1..N 行号，勾选需追踪的行 |

- `setTrackedLines` / `trackedLines` 以 `contentView_->toPlainText().split('\n')` 的 1-based 行号为追踪单元。
- 与 `NeoParser_TXT` 的协作：本扩展产出 `line_by_line` + `tracked_lines`；配对解析器的 `parse_lines` / `merge_lines` 按行号执行同步/合并。

## 典型用法

同 JSON 族（见 [NeoEditorExtension_Parser_JSON/usage.md](NeoEditorExtension_Parser_JSON/usage.md) 的「典型用法」）：`EditorExtensionRegistry::scan` → `resolve("CreateConfigEditor")` → 以 `".txt"` / `".properties"` / `".cfg"` 小写注册 `configMap_`；`configEditorFor(".txt")` 路由。编辑器「配置文件编辑器扩展(&P)」菜单展示 `TXT Config Editor Extension  v1.0.0`。

## 注意事项

- **导出符号必须完整**：`extern "C" __declspec(dllexport) ... CreateConfigEditor`。
- **`.properties` 键冲突已按 priority 仲裁（✅ 已修复，2026-08-30）**：本扩展（meta `priority=10`）与 Properties 扩展（meta `priority=20`）同时部署时 `configMap_[".properties"]` 归高者——Properties 扩展稳定接管；本扩展的 `.properties` 声明不再生效（`.txt`/`.cfg` 不受影响），无需调整扫描/注册顺序。
- **仅行追踪**：本扩展无键概念；`config_merge` 策略下保存的规则只有 `policy`，无 `tracked_keys`。
- **配对关系**：界面 ≠ 解析；`NeoParser_TXT` 需部署到 `parsers/`。
- 加载 API 为 `QLibrary::resolve`；meta 必须与 DLL 同目录。

## 相关文档

- [README.md](README.md) — 模块说明、依赖、构建集成
- 配对解析器：[NeoParser_TXT](../NeoParser_TXT/README.md)（`parse_lines` / `merge_lines`）
- 接口定义：[NeoCore](../NeoCore/README.md)（`IConfigEditorExtension`）
- 注册与加载：[GUIWorker](../GUIWorker/usage.md)、[NeoWorkspaceEditor](../NeoWorkspaceEditor/README.md)
- 总索引：`docs/Modules/README.md`