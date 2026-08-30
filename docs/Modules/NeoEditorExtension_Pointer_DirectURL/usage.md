# NeoEditorExtension_Pointer_DirectURL 使用文档

## 快速开始

```powershell
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
# 产物: build/deploy/editor/extension/pointer/NeoEditorExtension_Pointer_DirectURL.dll
#       build/deploy/editor/extension/pointer/NeoEditorExtension_Pointer_DirectURL.meta.json
```

启动 NeoWorkspaceEditor 打开一个 `direct_url` 类型指针文件（`resolver == "direct_url"` 的 resolver metadata），宿主即按 resolver 类型路由到本扩展表单。

## 插件契约

### 1. `NeoEditorExtension_Pointer_DirectURL.meta.json`（逐字段）

| 字段 | 类型 | 值 | 说明 |
|------|------|-----|------|
| `name` | string | `"DirectURL Pointer Editor Extension"` | 显示名 |
| `version` | string | `"1.0.0"` | 版本号 |
| `dll` | string | `"NeoEditorExtension_Pointer_DirectURL.dll"` | 同目录 DLL 文件名 |
| `resolver_type` | string | `"direct_url"` | 指针 resolver 类型；注册表用它作为 `pointerMap_` 键（小写） |
| `description` | string | `"DirectURL 类型指针文件的专属编辑器"` | 描述 |

> 本 meta **无** `editor_type` 与 `extensions` 字段——注册表据此判定为 Pointer 族，`fileTypes` 取 `resolver_type`。

### 2. 接口实现要点（`NeoCore::IPointerEditorExtension`）

- `resolverType()` 返回 `"direct_url"`（与 `NeoPointer_DirectURL::can_handle` 判定的 `ptr.resolver == "direct_url"` 配对）。
- `createEditor()` 返回 `DirectURLEditor`。
- `loadMetadata(editor, md)`：`url` → `urlEdit_`、`filename` → `filenameEdit_`。
- `saveMetadata(editor)`：始终写 `url`（trim 后）；`filename` 仅当非空才写入。

### 3. 表单字段与 metadata 字段对照

| 表单行 | 控件 | metadata 字段 | 必填 |
|--------|------|---------------|------|
| 下载 URL: | `QLineEdit`（placeholder `https://cdn.example.com/files/mod.jar`） | `url` | 是（NeoPointer_DirectURL 解析必需） |
| 文件名: | `QLineEdit`（placeholder `sodium.jar (可选)`） | `filename`（可选） | 否 |

## 功能细节

### 编辑界面

- `QGroupBox("DirectURL 元数据")` + `QFormLayout` 两行输入（下载 URL、文件名），无按钮、无网络请求。
- 保存语义：空 URL 也会写入 `url` 字段（空串）；`filename` 为空串时不写入字段，避免多余键。

### 与配对指针解析器的协作

- 本扩展产出 `metadata = {url, filename?}`；`NeoPointer_DirectURL`（`pointer_directurl.cpp`）要求 `metadata` 含**字符串** `url`（无 `url` 或非字符串 → 下载失败），据此直链下载并校验 SHA-256。`filename` 为可选提示/目标文件名。

## 典型用法

宿主加载本扩展同样有**两条链路**（与 [NeoEditorExtension_Pointer_Modrinth/usage.md](NeoEditorExtension_Pointer_Modrinth/usage.md) 相同）：

**① `GUIWorker::EditorExtensionRegistry`（meta 驱动）**

```cpp
EditorExtensionRegistry reg;
reg.scan(QCoreApplication::applicationDirPath() + "/editor/extension");
auto* ext = reg.pointerEditorFor("direct_url");  // pointerMap_["direct_url"]
QWidget* w = ext->createEditor(parent);
ext->loadMetadata(w, pointerMetadata);
QJsonObject md = ext->saveMetadata(w);
```

**② `GUIWorker::PointerManager`（直接 DLL 扫描）**

```cpp
// pointer_manager.cpp: 扫 <exe>/editor/extension/pointer 下 *.dll,
// QLibrary::load -> resolve("CreateEditorExtension") -> factory()
// extRegistry_[QString::fromStdString(ext->resolverType())] = ext;  // "direct_url"
```

编辑器「指针编辑器扩展(&N)」菜单列出 `DirectURL Pointer Editor Extension  v1.0.0`，tooltip 含 `类型: 指针 (pointer)` / `文件类型: direct_url` / description。

## 注意事项

- **导出符号必须完整**：`extern "C" __declspec(dllexport) ... CreateEditorExtension`；pointer 族符号名与 parser 族（`CreateConfigEditor`）不同，勿混。
- **`url` 为解析前提**：保存空 `url` 后 `NeoPointer_DirectURL` 无法解析下载地址（`metadata.contains("url")` 判空/类型检查）。
- **无网络依赖**：本扩展不发请求（`Qt6::Network` 未链接）；URL 合法性校验由远端下载环节负责。
- **配对关系**：本扩展 ≠ 指针解析器；`NeoPointer_DirectURL` 需部署到 `pointers/`。
- 加载 API 为 `QLibrary::resolve`（内部即 GetProcAddress）；meta 必须与 DLL 同目录。

## 相关文档

- [README.md](README.md) — 模块说明、依赖、构建集成
- 配对指针解析器：[NeoPointer_DirectURL](../NeoPointer_DirectURL/README.md)（直链下载）
- 接口定义：[NeoCore](../NeoCore/README.md)（`IPointerEditorExtension` / `IPluginPointer`）
- 注册与加载：[GUIWorker](../GUIWorker/usage.md)（`EditorExtensionRegistry` / `PointerManager`）、[NeoWorkspaceEditor](../NeoWorkspaceEditor/README.md)
- 总索引：`docs/Modules/README.md`