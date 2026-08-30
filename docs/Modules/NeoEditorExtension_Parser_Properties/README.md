# NeoEditorExtension_Parser_Properties 说明文档

## 概述

NeoEditorExtension_Parser_Properties 是整合包内容 IDE（NeoWorkspaceEditor）的**配置编辑器扩展插件**（SHARED DLL）。它为 properties 配置文件提供「同步模式选择 + 键追踪」的专用编辑界面：只读展示配置内容，按行解析 `key=value` / `key:value` 结构提取键（跳过 `#` / `!` 注释行与空行），勾选需追踪的键并生成同步规则（`syncRules`）。本插件只负责编辑界面与键定位；properties 的实际解析与合并由配对的 `NeoParser_Properties`（`IConfigParser`）完成。

## 设计目标

- 为 properties 配置提供「同步模式 + 键追踪」可视化编辑入口。
- 以 `meta.json` 声明的扩展名（`".properties"`）驱动注册，按后缀自动路由。
- 键解析兼容 Java properties 语法：`=` 与 `:` 双分隔符、行首与键尾空白裁剪、`#` / `!` 注释行跳过。
- `trackedLines()` 实现逐行精确键匹配（1-based），供宿主 merge 预览行标记。
- 与 `NeoParser_Properties` 保持格式配对。

## 模块边界

**做什么：**

- 提供实现 `NeoCore::IConfigEditorExtension` 的扩展实例与导出工厂 `CreateConfigEditor`。
- 提供 `ConfigEditor` 编辑界面（共享源码）。
- 键提取（`parse_property_line` 规则）、按 `syncRules` 预置勾选、序列化 `policy` + `tracked_keys`。
- 实现 `trackedLines()`（逐行解析键后与追踪键**精确匹配**，1-based）。

**不做什么：**

- 不做 properties 解析/合并（属于 `NeoParser_Properties`）。
- 不支持 `line_by_line` 的 `tracked_lines` 序列化。
- 不做文件读写、Git 操作；`mergePreview()` 恒返回 `""`。
- 不注册为配置解析器（`IConfigParser`）。

## 依赖关系

以 `modules/NeoEditorExtension_Parser_Properties/CMakeLists.txt` 为准：

| 依赖 | 链接方式 | 说明 |
|------|----------|------|
| `NeoCore` | `PRIVATE` | `IConfigEditorExtension` 接口；头文件路径 `../../NeoCore/include` |
| `Qt6::Widgets` | `PRIVATE` | `ConfigEditor` 控件 |
| nlohmann-json | 经 NeoCore 传递 | `editor_parser_properties.cpp` 直接 `#include <nlohmann/json.hpp>` |
| `../NeoEditorExtension_Parser_JSON/src/config_editor.{h,cpp}` | 本模块编译的共享源码 | 界面类（六族共用） |

> 反向依赖：根 CMakeLists（`CMakeLists.txt:131`）`add_subdirectory(modules/NeoEditorExtension_Parser_Properties)`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `src/editor_parser_properties.cpp` | `PropertiesConfigEditorExtension : NeoCore::IConfigEditorExtension` + `CreateConfigEditor` 导出；匿名命名空间内 `parse_property_line` 辅助函数 |
| `src/config_editor.h` / `src/config_editor.cpp` | 共享 `ConfigEditor` 界面类 |
| `NeoEditorExtension_Parser_Properties.meta.json` | 扩展元数据 |
| `CMakeLists.txt` | SHARED 目标定义：`PRIVATE NeoCore Qt6::Widgets`，`PREFIX ""` |

## 构建集成

- CMake target：`NeoEditorExtension_Parser_Properties`，类型 **SHARED**。
- `set_target_properties(NeoEditorExtension_Parser_Properties PROPERTIES PREFIX "")`：产物 DLL 名为 `NeoEditorExtension_Parser_Properties.dll`。
- **导出符号**（`src/editor_parser_properties.cpp:113`）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
      return new PropertiesConfigEditorExtension();
  }
  ```

  导出名为 `CreateConfigEditor`。
- 版本资源：根 CMakeLists `nsum_add_version_info(NeoEditorExtension_Parser_Properties "NSUM 配置编辑器扩展 (NeoEditorExtension_Parser_Properties)" "NSUM构建工具")`。
- **部署目录**：`neo_deploy` POST_BUILD 将 DLL 与 meta.json 拷贝到 `${DEPLOY_DIR}/editor/extension/parsers/`。

## 公共符号

| 符号 | 类别 | 说明 |
|------|------|------|
| `CreateConfigEditor` | `extern "C" __declspec(dllexport)` 导出函数 | 返回 `NeoCore::IConfigEditorExtension*` |
| `PropertiesConfigEditorExtension` | class（匿名 TU 内） | 实现 `IConfigEditorExtension` 的扩展实例 |
| `parse_property_line` | `static` 函数（匿名命名空间） | `bool parse_property_line(const std::string& line, std::string& out_key)`；解析 `=` / `:` 分隔、跳过注释/空行 |
| `ConfigEditor` | class（全局可见，非导出） | 共享编辑界面 |

### 实现类接口概览（`IConfigEditorExtension`）

| 方法 | 本模块实现 |
|------|-----------|
| `fileExtension()` | 返回 `".properties"` |
| `createEditor(parent)` | `new ConfigEditor(parent)` |
| `loadConfig` | 内容 = remote 非空取 remote 否则 local；`policy` 默认 `"full_sync"`；`std::getline` 逐行 `parse_property_line` 提取键 + `tracked_keys` 预置勾选 |
| `saveSyncRules` | `policy`；仅 `config_merge` → `tracked_keys` |
| `mergePreview` | 返回 `""` |
| `trackedLines` | 逐行 `parse_property_line`；键与任一追踪键**精确相等**即计入行号（1-based） |

详细用法见 [usage.md](usage.md)。