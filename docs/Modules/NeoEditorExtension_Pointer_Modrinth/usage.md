# NeoEditorExtension_Pointer_Modrinth 使用文档

## 快速开始

```powershell
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
# 产物: build/deploy/editor/extension/pointer/NeoEditorExtension_Pointer_Modrinth.dll
#       build/deploy/editor/extension/pointer/NeoEditorExtension_Pointer_Modrinth.meta.json
```

启动 NeoWorkspaceEditor 打开一个 `modrinth` 类型指针文件（`resolver == "modrinth"` 的 resolver metadata），宿主即按 resolver 类型路由到本扩展表单。

## 插件契约

### 1. `NeoEditorExtension_Pointer_Modrinth.meta.json`（逐字段）

| 字段 | 类型 | 值 | 说明 |
|------|------|-----|------|
| `name` | string | `"Modrinth Pointer Editor Extension"` | 显示名 |
| `version` | string | `"1.0.0"` | 版本号 |
| `dll` | string | `"NeoEditorExtension_Pointer_Modrinth.dll"` | 同目录 DLL 文件名 |
| `resolver_type` | string | `"modrinth"` | 指针 resolver 类型；注册表用它作为 `pointerMap_` 键（小写） |
| `description` | string | `"Modrinth 类型指针文件的专属编辑器，支持API查询模组信息"` | 描述 |

> 本 meta **无** `editor_type` 与 `extensions` 字段——注册表据此（`editor_type` 非 `"parser"` 且无 `extensions`）判定为 Pointer 族，`fileTypes` 取 `resolver_type`。

### 2. 接口实现要点（`NeoCore::IPointerEditorExtension`）

- `resolverType()` 返回 `"modrinth"`（与 `NeoPointer_Modrinth::can_handle` 判定的 `ptr.resolver == "modrinth"` 配对）。
- `createEditor()` 返回 `ModrinthEditor`。
- `loadMetadata(editor, md)`：`project_id` → `projectIdEdit_`、`version_id` → `versionIdEdit_`、`name` → `modNameEdit_`（`metadata.value(key).toString()`）。
- `saveMetadata(editor)`：始终写 `project_id` / `version_id`（trim 后）；`name` 仅当非空才写入。

### 3. 表单字段与 metadata 字段对照

| 表单行 | 控件 | metadata 字段 | 必填 |
|--------|------|---------------|------|
| Project ID: | `QLineEdit`（placeholder `AANobbMI (Project ID / Slug)`） | `project_id` | 是（API 查询前提） |
| Version ID: | `QLineEdit`（placeholder `aOPSMNeo (Version ID)`） | `version_id` | 是（NeoPointer_Modrinth 解析必需） |
| 名称: | `QLineEdit`（只读，placeholder `(自动获取)`） | `name`（可选） | 否 |

## 功能细节

### 「获取模组信息」按钮（阻塞式 API 查询）

1. 取 `project_id`（trim），为空 → 状态栏提示 `请输入 Project ID` 并返回。
2. URL 拼接：
   - 填了 `version_id`：`https://api.modrinth.com/v2/project/{pid}/version/{vid}`
   - 未填：`https://api.modrinth.com/v2/project/{pid}`
3. `QNetworkRequest`：`User-Agent: NeoServerUpdateModpack/1.0`，`setTransferTimeout(15000)`。
4. `QNetworkAccessManager::get` + `QEventLoop::exec()` + `QTimer::singleShot(15000, quit)`——**同步阻塞**等待 15s 超时。
5. 成功：nlohmann-json 解析，名称取 `title` → 回退 `name` → 回退 `version_number`；非空则写入名称框、状态 `已获取`（绿）。
6. 失败：状态 `解析失败` / `获取失败`（红）。按钮恢复可用。

### 与配对指针解析器的协作

- 本扩展产出 `metadata = {project_id, version_id, name?}`；`NeoPointer_Modrinth`（`pointer_modrinth.cpp`）要求 `metadata` 含**字符串** `project_id` 与 `version_id`，据此经 Modrinth API 解析下载 URL 并校验 SHA-256。

## 典型用法

宿主加载本扩展有**两条链路**：

**① `GUIWorker::EditorExtensionRegistry`（meta 驱动，整合包内容 IDE 主链路）**

```cpp
EditorExtensionRegistry reg;
reg.scan(QCoreApplication::applicationDirPath() + "/editor/extension"); // 递归扫 *.meta.json
auto* ext = reg.pointerEditorFor("modrinth");  // pointerMap_["modrinth"]
QWidget* w = ext->createEditor(parent);
ext->loadMetadata(w, pointerMetadata);          // 打开指针文件时
QJsonObject md = ext->saveMetadata(w);          // 保存时
```

**② `GUIWorker::PointerManager`（直接 DLL 扫描，无 meta）**

```cpp
// pointer_manager.cpp: 扫 <exe>/editor/extension/pointer 下 *.dll,
// QLibrary::load -> resolve("CreateEditorExtension") -> factory()
// extRegistry_[QString::fromStdString(ext->resolverType())] = ext;  // "modrinth"
```

编辑器「指针编辑器扩展(&N)」菜单列出 `Modrinth Pointer Editor Extension  v1.0.0`，tooltip 含 `类型: 指针 (pointer)` / `文件类型: modrinth` / description。

## 注意事项

- **导出符号必须完整**：`extern "C" __declspec(dllexport) ... CreateEditorExtension`；pointer 族符号名与 parser 族（`CreateConfigEditor`）不同，勿混。
- **API 查询为阻塞式**：`QEventLoop::exec()` 同步阻塞 UI 线程直至回复或 15s 超时；查询期间界面冻结属预期行为 [观察：可考虑改异步]。
- **必填字段缺失**：`NeoPointer_Modrinth` 要求 `project_id` 与 `version_id` 均为字符串，保存后若为空将无法解析下载 URL。
- **名称字段只读**：`name` 仅由 API 查询自动填充，非人工编辑。
- **配对关系**：本扩展 ≠ 指针解析器；`NeoPointer_Modrinth` 需部署到 `pointers/`。
- 加载 API 为 `QLibrary::resolve`（内部即 GetProcAddress）；meta 必须与 DLL 同目录。
- `src/editor_modrinth.cpp` include `<logger.h>` 但未见 `CLogger` 调用 [观察：遗留 include]。

## 相关文档

- [README.md](README.md) — 模块说明、依赖、构建集成
- 配对指针解析器：[NeoPointer_Modrinth](../NeoPointer_Modrinth/README.md)（Modrinth API，文档 https://docs.modrinth.com/api/）
- 接口定义：[NeoCore](../NeoCore/README.md)（`IPointerEditorExtension` / `IPluginPointer`）
- 注册与加载：[GUIWorker](../GUIWorker/usage.md)（`EditorExtensionRegistry` / `PointerManager`）、[NeoWorkspaceEditor](../NeoWorkspaceEditor/README.md)
- 总索引：`docs/Modules/README.md`