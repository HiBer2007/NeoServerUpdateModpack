# NeoEditorExtension_Parser_TOML 使用文档

## 快速开始

```powershell
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
# 产物: build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_TOML.dll
#       build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_TOML.meta.json
```

打开 NeoWorkspaceEditor 中的 `.toml` 配置文件即可看到本扩展界面。

## 插件契约

### 1. `NeoEditorExtension_Parser_TOML.meta.json`（逐字段，单行 JSON）

| 字段 | 类型 | 值 | 说明 |
|------|------|-----|------|
| `name` | string | `"TOML Config Editor Extension"` | 显示名 |
| `version` | string | `"1.0.0"` | 版本号 |
| `dll` | string | `"NeoEditorExtension_Parser_TOML.dll"` | 同目录 DLL 文件名 |
| `editor_type` | string | `"parser"` | 扩展族 |
| `extensions` | array[string] | `[".toml"]` | 关联文件后缀 |
| `description` | string | `"TOML配置文件编辑器，支持section.key追踪"` | 描述 |

### 2. 接口实现要点

- `fileExtension()` 返回 `".toml"`。
- `loadConfig()`：内容远端优先；`policy = syncRules.value("policy", "full_sync")`；键 = `extractTomlKeys`：段正则 `^\s*\[([^\]]+)\]` 重置 `section` 前缀，键正则 `^\s*([a-zA-Z_][a-zA-Z0-9_-]*)\s*=` 产出 `section + "." + key`（如 `[server]` 下 `port = 25565` → `server.port`；无段时为裸键名）。
- `saveSyncRules()`：写 `policy`；**仅** `config_merge` 时写 `tracked_keys`（数组）。
- `trackedLines(content, trackedKeys)`：逐行动态维护 `section`；命中条件 = 追踪键等于行内键名（`localKey`）或等于完整 `section.key`（`fullKey`）。1-based。
- `mergePreview()` 返回 `""`。

## 功能细节

### 编辑界面（共享 `ConfigEditor`）

| 区域 | 控件 | 内容 |
|------|------|------|
| 同步模式 | `QComboBox` | `full_sync - 完整同步` / `config_merge - 配置合并` / `line_by_line - 逐行同步` / `no_sync - 不同步` |
| 文件内容 (只读) | `QTextEdit`（只读，Consolas 9pt） | 远端或本地内容 |
| 追踪项 | `QTreeWidget`（表头 `键` / `追踪`）+ `全选/全不选` 按钮 | 键列表（`section.key` 形式） |

- 键路径约定：TOML 的段前缀展开为点分键 `section.key`，与 JSON 族点分约定一致，便于 `syncRules.tracked_keys` 统一表达。
- 与 `NeoParser_TOML` 的协作：本扩展产出 `policy` + `tracked_keys`；段落与键的实际解析由配对解析器负责。

## 典型用法

同 JSON 族（见 [NeoEditorExtension_Parser_JSON/usage.md](NeoEditorExtension_Parser_JSON/usage.md) 的「典型用法」）：`EditorExtensionRegistry::scan` 递归扫 `*.meta.json` → `editor_type == "parser"` → `resolve("CreateConfigEditor")` → `configMap_[".toml"]` 注册；打开文件 `configEditorFor(".toml")` 路由。编辑器菜单展示 `TOML Config Editor Extension  v1.0.0`。

## 注意事项

- **导出符号必须完整**：`extern "C" __declspec(dllexport) ... CreateConfigEditor`。
- **键是裸键或段键**：无 `[section]` 前缀的行产出裸键名；同名裸键与段键（如 `port` 与 `server.port`）在 `trackedLines` 中均可命中。
- **段上下文为顺序解析**：`trackedLines` 按行维护当前段，遇新 `[section]` 立即切换；数组表 `[[...]]` 不在处理范围内 [观察]。
- **无 `line_by_line` 序列化**：策略选 `line_by_line` 时保存的规则不含 `tracked_lines`。
- **配对关系**：界面 ≠ 解析；`NeoParser_TOML` 需部署到 `parsers/`。
- 加载 API 为 `QLibrary::resolve`；meta 必须与 DLL 同目录。

## 相关文档

- [README.md](README.md) — 模块说明、依赖、构建集成
- 配对解析器：[NeoParser_TOML](../NeoParser_TOML/README.md)
- 接口定义：[NeoCore](../NeoCore/README.md)（`IConfigEditorExtension`）
- 注册与加载：[GUIWorker](../GUIWorker/usage.md)、[NeoWorkspaceEditor](../NeoWorkspaceEditor/README.md)
- 总索引：`docs/Modules/README.md`