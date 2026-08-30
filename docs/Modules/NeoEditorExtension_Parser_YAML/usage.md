# NeoEditorExtension_Parser_YAML 使用文档

## 快速开始

```powershell
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
# 产物: build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_YAML.dll
#       build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_YAML.meta.json
```

打开 NeoWorkspaceEditor 中的 `.yml` / `.yaml` 配置文件即可看到本扩展界面。

## 插件契约

### 1. `NeoEditorExtension_Parser_YAML.meta.json`（逐字段，文件为单行 JSON）

| 字段 | 类型 | 值 | 说明 |
|------|------|-----|------|
| `name` | string | `"YAML Config Editor Extension"` | 显示名 |
| `version` | string | `"1.0.0"` | 版本号 |
| `dll` | string | `"NeoEditorExtension_Parser_YAML.dll"` | 同目录 DLL 文件名 |
| `editor_type` | string | `"parser"` | 扩展族 |
| `extensions` | array[string] | `[".yml", ".yaml"]` | 关联文件后缀 |
| `description` | string | `"YAML配置文件编辑器，支持嵌套键追踪"` | 描述 |

### 2. 接口实现要点

- `fileExtension()` 返回 `".yaml"`（meta 已带 `extensions`，注册表不回退该值）。
- `loadConfig()`：内容远端优先；`policy = syncRules.value("policy", "full_sync")`；键 = `extractYamlKeys`（正则 `^\s*([a-zA-Z_][a-zA-Z0-9_.-]*)\s*:` 逐行提取，**不做缩进层级归并**——键列展示的是所有匹配行的行首键名）。
- `saveSyncRules()`：写 `policy`；**仅** `config_merge` 时写 `tracked_keys`。YAML 扩展无 `line_by_line` 分支（`line_by_line` 策略下不写 `tracked_lines`）。
- `trackedLines(content, trackedKeys)`：以缩进栈构造点分完整路径——层数按 `indent / 2` 估算（假设 2 空格一级，如 `a:\n  b:` → `a.b`）。命中条件：追踪键 == 完整路径、== 行内局部键、或完整路径末段 == 行内局部键。1-based。
- `mergePreview()` 返回 `""`。

## 功能细节

### 编辑界面（共享 `ConfigEditor`）

| 区域 | 控件 | 内容 |
|------|------|------|
| 同步模式 | `QComboBox` | `full_sync - 完整同步` / `config_merge - 配置合并` / `line_by_line - 逐行同步` / `no_sync - 不同步` |
| 文件内容 (只读) | `QTextEdit`（只读，Consolas 9pt） | 远端或本地内容 |
| 追踪项 | `QTreeWidget`（表头 `键` / `追踪`）+ `全选/全不选` 按钮 | 勾选需追踪的键 |

- 嵌套键追踪：宿主在 `syncRules.tracked_keys` 中传入点分路径（如 `database.host`）；`trackedLines` 用缩进上下文还原完整路径以便 merge 预览行标记。
- 与 `NeoParser_YAML` 的协作：本扩展产出 `policy` + `tracked_keys`，由 `IConfigParser::merge_entries` 按追踪键执行配置合并。

## 典型用法

同 JSON 族（见 [NeoEditorExtension_Parser_JSON/usage.md](NeoEditorExtension_Parser_JSON/usage.md) 的「典型用法」）：`ModpackContentIde` 构造时 `scanExtensionDirs` → `EditorExtensionRegistry::scan` → 按 meta 判定 Parser → `resolve("CreateConfigEditor")` → 以 `".yml"` / `".yaml"` 小写为键注册 `configMap_`；打开文件时 `configEditorFor(".yaml")` 路由。编辑器「配置文件编辑器扩展(&P)」菜单展示 `YAML Config Editor Extension  v1.0.0`。

## 注意事项

- **导出符号必须完整**：`extern "C" __declspec(dllexport) ... CreateConfigEditor`；缺 `__declspec(dllexport)` → DLL 零导出 → 注册表跳过。
- **键提取不含层级（属当前实现现状：界面直接展示嵌套完整键当前未实现）**：界面「追踪项」列表展示的是逐行行首键（非完整点分路径）；完整路径只在 `trackedLines`（merge 预览行标记）中由缩进栈还原。
- **缩进宽度假设 2 空格**：`trackedLines` 用 `indent / 2` 估层级；使用 4 空格缩进的 YAML 会推高栈层级，可能与实际结构不一致 [观察：以实际文件缩进为准]。
- **无 `line_by_line` 序列化**：若策略选 `line_by_line`，保存的规则不含 `tracked_lines`。
- **配对关系**：编辑界面 ≠ 解析；`NeoParser_YAML` 需部署到 `parsers/`。
- 加载 API 为 `QLibrary::resolve`；meta 必须与 DLL 同目录。

## 相关文档

- [README.md](README.md) — 模块说明、依赖、构建集成
- 配对解析器：[NeoParser_YAML](../NeoParser_YAML/README.md)
- 接口定义：[NeoCore](../NeoCore/README.md)（`IConfigEditorExtension`）
- 注册与加载：[GUIWorker](../GUIWorker/usage.md)、[NeoWorkspaceEditor](../NeoWorkspaceEditor/README.md)
- 总索引：`docs/Modules/README.md`