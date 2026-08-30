# NeoEditorExtension_Pointer_Modrinth 说明文档

## 概述

NeoEditorExtension_Pointer_Modrinth 是整合包内容 IDE（NeoWorkspaceEditor）的**指针编辑器扩展插件**（SHARED DLL）。它为 `modrinth` 类型指针文件的 `metadata` 提供专属编辑界面：编辑 `project_id` / `version_id` 并可一键调用 Modrinth API 查询模组名称（`title` / `name` / `version_number`），保存回指针文件的 resolver metadata。本插件只负责 metadata 编辑界面与 API 查询；用 project_id/version_id 解析实际下载地址、校验哈希由配对的 `NeoPointer_Modrinth`（`IPluginPointer`）完成。

## 设计目标

- 为 `resolver_type == "modrinth"` 的指针文件提供「Project ID / Version ID」表单编辑界面。
- 内置 Modrinth API 查询：`https://api.modrinth.com/v2`，自动填回模组名称，减少人工查证。
- 以 `meta.json` 的 `resolver_type`（`"modrinth"`）驱动注册，宿主按指针 resolver 类型路由到本扩展。
- 与 `NeoPointer_Modrinth` 保持 metadata 字段配对（`project_id` / `version_id` 为必需字符串）。

## 模块边界

**做什么：**

- 提供实现 `NeoCore::IPointerEditorExtension` 的扩展实例与导出工厂 `CreateEditorExtension`。
- 提供 `ModrinthEditor` 表单界面（Project ID、Version ID、名称、获取模组信息按钮）。
- 读写指针 metadata（`project_id` / `version_id` / 可选 `name`）。
- 经 Modrinth API 查询模组/版本信息（阻塞式，15s 超时）。

**不做什么：**

- 不解析下载 URL、不下载文件、不校验哈希（属于 `NeoPointer_Modrinth` 的 `resolve_url` / `validate`）。
- 不做指针文件本身的 JSON 读写与 Git 操作（宿主完成）。
- 不支持除 `modrinth` 外的 resolver 类型。

## 依赖关系

以 `modules/NeoEditorExtension_Pointer_Modrinth/CMakeLists.txt` 为准：

| 依赖 | 链接方式 | 说明 |
|------|----------|------|
| `NeoCore` | `PRIVATE` | `IPointerEditorExtension` 接口；头文件路径 `../NeoCore/include` |
| `Qt6::Widgets` | `PRIVATE` | 表单控件（QLineEdit / QPushButton / QLabel / QFormLayout / QGroupBox） |
| `Qt6::Network` | `PRIVATE` | `QNetworkAccessManager` / `QNetworkReply` / `QNetworkRequest`（Modrinth API 查询） |
| nlohmann-json | 经 NeoCore 传递 | API 响应解析 |

> 反向依赖：根 CMakeLists（`CMakeLists.txt:124`）`add_subdirectory(modules/NeoEditorExtension_Pointer_Modrinth)`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `src/editor_modrinth.h` | `ModrinthEditor`（QWidget）声明：`loadMetadata` / `saveMetadata`；成员 `projectIdEdit_` / `versionIdEdit_` / `modNameEdit_` / `fetchBtn_` / `statusLabel_` |
| `src/editor_modrinth.cpp` | `ModrinthEditor` 实现 + `ModrinthEditorExtension : NeoCore::IPointerEditorExtension` + `CreateEditorExtension` 导出 |
| `NeoEditorExtension_Pointer_Modrinth.meta.json` | 扩展元数据 |
| `CMakeLists.txt` | SHARED 目标定义：`PRIVATE NeoCore Qt6::Widgets Qt6::Network`，`PREFIX ""` |

## 构建集成

- CMake target：`NeoEditorExtension_Pointer_Modrinth`，类型 **SHARED**。
- `set_target_properties(NeoEditorExtension_Pointer_Modrinth PROPERTIES PREFIX "")`：产物 DLL 名为 `NeoEditorExtension_Pointer_Modrinth.dll`。
- **导出符号**（`src/editor_modrinth.cpp:137`）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IPointerEditorExtension* CreateEditorExtension() {
      return new ModrinthEditorExtension();
  }
  ```

  导出名为 `CreateEditorExtension`（对应 `NeoCore::CreateEditorExtensionFunc` 函数指针类型）。
- 版本资源：根 CMakeLists `nsum_add_version_info(NeoEditorExtension_Pointer_Modrinth "NSUM 指针编辑器扩展 (NeoEditorExtension_Pointer_Modrinth)" "NSUM构建工具")`。
- **部署目录**：根 CMakeLists `neo_deploy` POST_BUILD 将 DLL 与 meta.json 拷贝到 `${DEPLOY_DIR}/editor/extension/pointer/`（`DEPLOY_DIR = ${CMAKE_BINARY_DIR}/deploy`，`copy_if_different`）。

## 公共符号

| 符号 | 类别 | 说明 |
|------|------|------|
| `CreateEditorExtension` | `extern "C" __declspec(dllexport)` 导出函数 | 返回 `NeoCore::IPointerEditorExtension*`，注册表/`PointerManager` 经 `QLibrary::resolve("CreateEditorExtension")` 调用 |
| `ModrinthEditorExtension` | class（TU 内） | 实现 `IPointerEditorExtension` 的扩展实例 |
| `ModrinthEditor` | class（全局可见，非导出） | 表单编辑界面 QWidget |

### 实现类接口概览（`IPointerEditorExtension`，摘自 `NeoCore/include/IPointerEditorExtension.h`）

| 方法 | 签名 | 本模块实现 |
|------|------|-----------|
| `resolverType` | `virtual std::string resolverType() const = 0;` | 返回 `"modrinth"` |
| `createEditor` | `virtual QWidget* createEditor(QWidget* parent) = 0;` | `new ModrinthEditor(parent)` |
| `loadMetadata` | `virtual void loadMetadata(QWidget* editor, const QJsonObject& metadata) = 0;` | 填充 `project_id` / `version_id` / `name` 到编辑框 |
| `saveMetadata` | `virtual QJsonObject saveMetadata(QWidget* editor) const = 0;` | 输出 `project_id` / `version_id` /（非空时）`name` |

详细用法见 [usage.md](usage.md)。