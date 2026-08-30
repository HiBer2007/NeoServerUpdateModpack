# NeoEditorExtension_Pointer_DirectURL 说明文档

## 概述

NeoEditorExtension_Pointer_DirectURL 是整合包内容 IDE（NeoWorkspaceEditor）的**指针编辑器扩展插件**（SHARED DLL）。它为 `direct_url` 类型指针文件的 `metadata` 提供专属编辑界面：编辑下载 URL 与可选文件名，保存回指针文件的 resolver metadata。本插件只负责 metadata 编辑界面；按 `url` 执行直链下载、校验哈希由配对的 `NeoPointer_DirectURL`（`IPluginPointer`）完成。

## 设计目标

- 为 `resolver_type == "direct_url"` 的指针文件提供「下载 URL + 文件名」表单编辑界面。
- 以 `meta.json` 的 `resolver_type`（`"direct_url"`）驱动注册，宿主按指针 resolver 类型路由。
- 保持最简单的表单形态（两个文本框），无网络依赖。
- 与 `NeoPointer_DirectURL` 保持 metadata 字段配对（`url` 为必需字符串）。

## 模块边界

**做什么：**

- 提供实现 `NeoCore::IPointerEditorExtension` 的扩展实例与导出工厂 `CreateEditorExtension`。
- 提供 `DirectURLEditor` 表单界面（下载 URL、文件名）。
- 读写指针 metadata（`url` / 可选 `filename`）。

**不做什么：**

- 不解析/下载 URL、不校验哈希（属于 `NeoPointer_DirectURL` 的 `resolve_url` / `validate`）。
- 不做指针文件本身的 JSON 读写与 Git 操作（宿主完成）。
- 不支持除 `direct_url` 外的 resolver 类型。

## 依赖关系

以 `modules/NeoEditorExtension_Pointer_DirectURL/CMakeLists.txt` 为准：

| 依赖 | 链接方式 | 说明 |
|------|----------|------|
| `NeoCore` | `PRIVATE` | `IPointerEditorExtension` 接口；头文件路径 `../NeoCore/include` |
| `Qt6::Widgets` | `PRIVATE` | 表单控件（QLineEdit / QFormLayout / QGroupBox / QVBoxLayout） |

> 无 `Qt6::Network` 依赖（本扩展不发网络请求，与 Modrinth 扩展不同）。

> 反向依赖：根 CMakeLists（`CMakeLists.txt:125`）`add_subdirectory(modules/NeoEditorExtension_Pointer_DirectURL)`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `src/editor_directurl.h` | `DirectURLEditor`（QWidget）声明：`loadMetadata` / `saveMetadata`；成员 `urlEdit_` / `filenameEdit_` |
| `src/editor_directurl.cpp` | `DirectURLEditor` 实现 + `DirectURLEditorExtension : NeoCore::IPointerEditorExtension` + `CreateEditorExtension` 导出 |
| `NeoEditorExtension_Pointer_DirectURL.meta.json` | 扩展元数据 |
| `CMakeLists.txt` | SHARED 目标定义：`PRIVATE NeoCore Qt6::Widgets`，`PREFIX ""` |

## 构建集成

- CMake target：`NeoEditorExtension_Pointer_DirectURL`，类型 **SHARED**。
- `set_target_properties(NeoEditorExtension_Pointer_DirectURL PROPERTIES PREFIX "")`：产物 DLL 名为 `NeoEditorExtension_Pointer_DirectURL.dll`。
- **导出符号**（`src/editor_directurl.cpp:64`）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IPointerEditorExtension* CreateEditorExtension() {
      return new DirectURLEditorExtension();
  }
  ```

  导出名为 `CreateEditorExtension`（对应 `NeoCore::CreateEditorExtensionFunc` 函数指针类型）。
- 版本资源：根 CMakeLists `nsum_add_version_info(NeoEditorExtension_Pointer_DirectURL "NSUM 指针编辑器扩展 (NeoEditorExtension_Pointer_DirectURL)" "NSUM构建工具")`。
- **部署目录**：根 CMakeLists `neo_deploy` POST_BUILD 将 DLL 与 meta.json 拷贝到 `${DEPLOY_DIR}/editor/extension/pointer/`（`DEPLOY_DIR = ${CMAKE_BINARY_DIR}/deploy`，`copy_if_different`）。

## 公共符号

| 符号 | 类别 | 说明 |
|------|------|------|
| `CreateEditorExtension` | `extern "C" __declspec(dllexport)` 导出函数 | 返回 `NeoCore::IPointerEditorExtension*`，注册表/`PointerManager` 经 `QLibrary::resolve("CreateEditorExtension")` 调用 |
| `DirectURLEditorExtension` | class（TU 内） | 实现 `IPointerEditorExtension` 的扩展实例 |
| `DirectURLEditor` | class（全局可见，非导出） | 表单编辑界面 QWidget |

### 实现类接口概览（`IPointerEditorExtension`，摘自 `NeoCore/include/IPointerEditorExtension.h`）

| 方法 | 签名 | 本模块实现 |
|------|------|-----------|
| `resolverType` | `virtual std::string resolverType() const = 0;` | 返回 `"direct_url"` |
| `createEditor` | `virtual QWidget* createEditor(QWidget* parent) = 0;` | `new DirectURLEditor(parent)` |
| `loadMetadata` | `virtual void loadMetadata(QWidget* editor, const QJsonObject& metadata) = 0;` | 填充 `url` / `filename` 到编辑框 |
| `saveMetadata` | `virtual QJsonObject saveMetadata(QWidget* editor) const = 0;` | 输出 `url` /（非空时）`filename` |

详细用法见 [usage.md](usage.md)。