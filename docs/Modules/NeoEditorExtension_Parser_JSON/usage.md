# NeoEditorExtension_Parser_JSON 使用文档

## 快速开始

构建 + 部署（根目录，先 `call vcvars64.bat`）：

```powershell
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
# 产物: build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_JSON.dll
#       build/deploy/editor/extension/parsers/NeoEditorExtension_Parser_JSON.meta.json
```

`neo_deploy` 自动完成「建目录 → 拷 DLL → 拷 meta.json」。也可手工把这两个文件拷到编辑器 EXE 旁的 `editor/extension/` 任意子目录（注册表递归扫描 `*.meta.json`）。

文件落位后启动 NeoWorkspaceEditor，打开一个 `.json` / `.json5` / `.jsonc` 配置文件即可看到本扩展提供的编辑界面（配置内容 IDE 按文件后缀经注册表路由）。

## 插件契约

### 1. `NeoEditorExtension_Parser_JSON.meta.json`（逐字段）

| 字段 | 类型 | 值 | 说明 |
|------|------|-----|------|
| `name` | string | `"JSON Config Editor Extension"` | 扩展显示名（编辑器「扩展」菜单与日志使用） |
| `version` | string | `"1.0.0"` | 版本号（菜单显示为 `名称  v1.0.0`） |
| `dll` | string | `"NeoEditorExtension_Parser_JSON.dll"` | 同目录 DLL 文件名（注册表要求必有此字段，缺则跳过） |
| `editor_type` | string | `"parser"` | 扩展族；注册表据此（或存在 `extensions` 字段）判定为 Parser 类 |
| `extensions` | array[string] | `[".json", ".json5", ".jsonc"]` | 关联文件后缀（注册为 `configMap_` 键；宿主按小写后缀查找） |
| `description` | string | `"JSON配置文件编辑器，支持key-path追踪"` | 描述（菜单 tooltip） |

### 2. 接口实现要点（`NeoCore::IConfigEditorExtension`）

- `fileExtension()` 返回 `".json"`；注册表在 meta 无 `extensions` 时才回退用它，本 meta 已带 `extensions`。
- `createEditor()` 返回共享的 `ConfigEditor` 实例。
- `loadConfig()`：内容取 `remoteContent.empty() ? localContent : remoteContent`（远端优先）；`policy = syncRules.value("policy", "full_sync")`；用 nlohmann-json 解析顶层键列表，`syncRules["tracked_keys"]` 中的键预置为勾选。解析失败（`catch(...)`）时键列表为空，界面仍可用。
- `saveSyncRules()`：始终写 `policy`；仅当 `policy == "config_merge"` 写 `tracked_keys`（数组），`policy == "line_by_line"` 时写 `tracked_lines`（整数数组）。
- `mergePreview()` 返回 `""`（属当前实现现状，非缺陷：宿主当前未在别处提供独立合并预览渲染；预览职责由 `trackedLines` 的行标记承担）。
- `trackedLines(content, trackedKeys)`：逐行匹配 `^\s*"([^"]+)"\s*:`；命中条件 = 行内键等于追踪键、或等于追踪键点分路径的末段（如追踪 `server.port`，行 `"port": 25565` 命中）。返回 1-based 行号。

## 功能细节

### 编辑界面（`ConfigEditor`，与其余 5 个 parser 扩展共用同一份源码）

| 区域 | 控件 | 内容 |
|------|------|------|
| 同步模式 | `QComboBox` | 四项：`full_sync - 完整同步` / `config_merge - 配置合并` / `line_by_line - 逐行同步` / `no_sync - 不同步`；`syncMode()` 按 `contains` 匹配回写规范值（默认 `"full_sync"`） |
| 文件内容 (只读) | `QTextEdit`（`setReadOnly(true)`，Consolas 9pt，最小高 120） | 展示远端或本地内容 |
| 追踪项 | `QTreeWidget`（表头 `键` / `追踪`，可勾选） | 键列表 + 勾选状态；`全选/全不选` 按钮一键切换 |

- 键路径约定：JSON 嵌套键以点分路径表达（如 `server.port`），由宿主在 `syncRules.tracked_keys` 中传入。
- 与配对解析器的协作：本扩展只产出同步规则 JSON；`NeoParser_JSON`（`IConfigParser`）负责按 `tracked_keys` 执行配置合并（`merge_entries`）。

## 典型用法

编辑器（`GUIWorker::ModpackContentIde`）扫描并加载扩展的链路：

```cpp
// modpack_content_ide.cpp — 构造时扫描
extRegistry_ = std::make_unique<EditorExtensionRegistry>();
scanExtensionDirs(extRegistry_.get());
// scanExtensionDirs 候选目录（去重后逐个 scan）:
//   <exe>/editor/extension
//   <exe>/../editor/extension
//   <cwd>/build/deploy/editor/extension
```

`EditorExtensionRegistry::scan(baseDir)`（`GUIWorker/src/editor_extension_registry.cpp`）：

1. 递归收集 `*.meta.json`；解析失败 / 无 `dll` 字段 / DLL 不存在 → 跳过并打日志。
2. 族判定：`editor_type == "parser"` **或** meta 含 `extensions` 数组 → `EditorExtensionKind::Parser`。
3. `QLibrary(dllAbs)->load()` → `resolve("CreateConfigEditor")`（找不到 → 跳过）→ `factory()` 创建实例。
4. 以 `extensions` 小写为键写入 `configMap_`（如 `".json"`、`".json5"`、`".jsonc"`）。

文件打开时按后缀路由（`modpack_content_ide.cpp`）：

```cpp
const QString ext = QFileInfo(relPath).suffix().toLower();
configEditor_->setEditorExtension(
    extRegistry_->configEditorFor(ext.isEmpty() ? QString() : QStringLiteral(".") + ext));
```

菜单展示（`NeoWorkspaceEditor/src/editor_window.cpp` `rebuildExtensionsMenu`）：扩展菜单 →「配置文件编辑器扩展(&P)」子菜单列出 `name v版本`，tooltip 含 `类型: 解析器 (parser)` / `文件类型: ...` / `description`（菜单项仅展示，disabled）。

## 注意事项

- **导出符号必须完整**：`extern "C" __declspec(dllexport)` 三要素缺一不可；缺 `__declspec(dllexport)` 时 DLL 零导出，`QLibrary::resolve` 失败、注册表静默跳过（项目 2026-08-06 全局修复教训）。改导出后用 `dumpbin /exports` 核对 `CreateConfigEditor`。
- **meta.json 必须与 DLL 同目录**：注册表用 meta 的 `dll` 字段拼接绝对路径（`metaPath.absoluteDir() + dll 字段`）。
- **加载 API 是 `QLibrary::resolve`**（内部即 GetProcAddress），不是直接 `GetProcAddress`；宿主代码见 `editor_extension_registry.cpp`。
- **卸载顺序**：`EditorExtensionRegistry::unloadEntry` 先 `delete` 扩展实例（按 kind 转 `IConfigEditorExtension*`）再 `lib->unload()`；`ModpackContentIde` 析构先切断 `setEditorExtension(nullptr)` 再卸载，防 vtable 指向已卸载代码页（2026-08-20 崩溃教训）。
- **配对关系**：本扩展 ≠ 解析器。若 `NeoParser_JSON` 未部署到 `parsers/`，编辑器可显示界面但合并/预览无解析能力。
- **TXT 扩展的扩展名重叠已按 priority 仲裁（✅ 已修复，2026-08-30）**：`NeoEditorExtension_Parser_TXT` 的 meta 也声明了 `".properties"`（还有 `".txt"`/`".cfg"`），而 Properties 扩展声明 `".properties"` —— `configMap_` 冲突时按 meta `priority` 高者胜（默认 0）：Properties 扩展 `priority=20` > TXT 扩展 `priority=10`，`".properties"` 稳定归 Properties 扩展，不再依赖扫描（QDirIterator）顺序。
- **内容以远端优先**：`loadConfig` 在 `remoteContent` 非空时展示远端内容；追踪键来自 `syncRules`。

## 相关文档

- [README.md](README.md) — 模块说明、设计目标、依赖关系、构建集成
- 配对解析器：[NeoParser_JSON](../NeoParser_JSON/README.md)（`IConfigParser`，负责解析/合并）
- 接口定义：[NeoCore](../NeoCore/README.md) `include/IConfigEditorExtension.h`
- 注册与加载：[GUIWorker](../GUIWorker/usage.md)（`EditorExtensionRegistry` / `ModpackContentIde::injectConfigEditorExt`）、[NeoWorkspaceEditor](../NeoWorkspaceEditor/README.md)（`EditorWindow::rebuildExtensionsMenu`）
- 总索引：`docs/Modules/README.md`