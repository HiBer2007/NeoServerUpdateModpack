# NeoEditorExtension_Parser_JSON 说明文档

## 概述

NeoEditorExtension_Parser_JSON 是整合包内容 IDE（NeoWorkspaceEditor）的**配置编辑器扩展插件**（SHARED DLL）。它为 JSON / JSON5 / JSONC 配置文件提供「同步模式选择 + 键级追踪」的专用编辑界面：只读展示配置文件内容，按需勾选需要追踪的配置键，并生成对应的同步规则（`syncRules`，含 `policy` / `tracked_keys` / `tracked_lines`）。本插件只负责编辑界面与键定位；JSON 的实际解析与合并由配对的 `NeoParser_JSON`（`IConfigParser`）完成。

## 设计目标

- 为 JSON 配置在编辑器内提供「同步模式 + 追踪键」的可视化编辑入口，替代裸文本编辑。
- 以 `meta.json` 声明的扩展名集合（`".json"` / `".json5"` / `".jsonc"`）驱动注册表，宿主按文件后缀自动路由到本扩展。
- 实现 `trackedLines()` 键定位：按行匹配 `"key": value`（支持点分路径取末段），为宿主 merge 预览行标记提供行号。
- 与 `NeoParser_JSON` 保持格式配对：编辑界面挑选的同步规则由配对解析器执行。

## 模块边界

**做什么：**

- 提供实现 `NeoCore::IConfigEditorExtension` 的扩展实例与导出工厂 `CreateConfigEditor`。
- 提供 `ConfigEditor` 编辑界面（同步模式下拉、只读内容区、追踪项树 + 全选/全不选）。
- 提取 JSON 顶层键、按 `syncRules` 预置勾选状态、序列化回 `policy` + `tracked_keys` / `tracked_lines`。
- 实现 `trackedLines()`（行内 `"key":` 匹配，1-based）。

**不做什么：**

- 不做 JSON 解析/合并（属于 `NeoParser_JSON` 的 `parse_entries` / `merge_entries`）。
- 不做文件读写、Git 操作（由宿主 `ModpackContentIde` / 编辑器 EXE 完成）。
- 不实现 merge 预览渲染：`mergePreview()` 恒返回空串 `""`。
- 不注册为配置解析器（`IConfigParser`）——两个插件体系各自独立。

## 依赖关系

以 `modules/NeoEditorExtension_Parser_JSON/CMakeLists.txt` 为准：

| 依赖 | 链接方式 | 说明 |
|------|----------|------|
| `NeoCore` | `PRIVATE` | `IConfigEditorExtension` 接口；头文件路径 `../../NeoCore/include` |
| `Qt6::Widgets` | `PRIVATE` | `ConfigEditor` 控件（QTextEdit / QComboBox / QTreeWidget / QPushButton / QLabel） |
| nlohmann-json | 经 NeoCore 传递 | `editor_parser_json.cpp` 直接 `#include <nlohmann/json.hpp>` |
| `src/config_editor.{h,cpp}` | 本模块共享源码 | 界面类，与 YAML/TOML/SNBT/TXT/Properties 五个 parser 扩展共用同一份源文件 |

> 反向依赖：根 CMakeLists（`CMakeLists.txt:126`）`add_subdirectory(modules/NeoEditorExtension_Parser_JSON)`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `src/config_editor.h` | `ConfigEditor`（QWidget）声明：`setContent` / `setSyncMode` / `syncMode` / `setKeys` / `trackedKeys` / `setLineMode` / `mergePreview` / `setTrackedLines` / `trackedLines` |
| `src/config_editor.cpp` | `ConfigEditor` 实现（同步模式组、只读内容组、追踪项组） |
| `src/editor_parser_json.cpp` | `JSONConfigEditorExtension : NeoCore::IConfigEditorExtension` + `CreateConfigEditor` 导出 |
| `NeoEditorExtension_Parser_JSON.meta.json` | 扩展元数据（见「插件契约」） |
| `CMakeLists.txt` | SHARED 目标定义：`PRIVATE NeoCore Qt6::Widgets`，`PREFIX ""` |

## 构建集成

- CMake target：`NeoEditorExtension_Parser_JSON`，类型 **SHARED**（`add_library(... SHARED ...)`）。
- `set_target_properties(NeoEditorExtension_Parser_JSON PROPERTIES PREFIX "")`：不生成 `lib` 前缀，产物 DLL 名为 `NeoEditorExtension_Parser_JSON.dll`。
- **导出符号**（`src/editor_parser_json.cpp:86`）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
      return new JSONConfigEditorExtension();
  }
  ```

  导出名为 `CreateConfigEditor`（对应 `NeoCore::CreateConfigEditorFunc` 函数指针类型）。
- 版本资源：根 CMakeLists `nsum_add_version_info(NeoEditorExtension_Parser_JSON "NSUM 配置编辑器扩展 (NeoEditorExtension_Parser_JSON)" "NSUM构建工具")`。
- **部署目录**：根 CMakeLists `neo_deploy` POST_BUILD 将 DLL 与 `NeoEditorExtension_Parser_JSON.meta.json` 拷贝到 `${DEPLOY_DIR}/editor/extension/parsers/`（`DEPLOY_DIR = ${CMAKE_BINARY_DIR}/deploy`，`copy_if_different`）。
- meta.json 随 DLL 一同部署，两者必须**同名同目录**（注册表按 meta 的 `dll` 字段在同目录找 DLL）。

## 公共符号

| 符号 | 类别 | 说明 |
|------|------|------|
| `CreateConfigEditor` | `extern "C" __declspec(dllexport)` 导出函数 | 返回 `NeoCore::IConfigEditorExtension*`，注册表经 `QLibrary::resolve("CreateConfigEditor")` 调用 |
| `JSONConfigEditorExtension` | class（匿名 TU 内，不可见） | 实现 `IConfigEditorExtension` 的扩展实例 |
| `ConfigEditor` | class（全局可见，非导出） | 编辑界面 QWidget（六个 parser 扩展共享） |

### 实现类接口概览（`IConfigEditorExtension`，摘自 `NeoCore/include/IConfigEditorExtension.h`）

| 方法 | 签名 | 本模块实现 |
|------|------|-----------|
| `fileExtension` | `virtual std::string fileExtension() const = 0;` | 返回 `".json"` |
| `createEditor` | `virtual QWidget* createEditor(QWidget* parent) = 0;` | `new ConfigEditor(parent)` |
| `loadConfig` | `virtual void loadConfig(QWidget* editor, const std::string& remoteContent, const std::string& localContent, const nlohmann::json& syncRules) = 0;` | 内容 = remote 非空取 remote 否则 local；`policy` 默认 `"full_sync"`；顶层键 + `tracked_keys` 预置勾选 |
| `saveSyncRules` | `virtual nlohmann::json saveSyncRules(QWidget* editor) const = 0;` | `policy`；`config_merge` → `tracked_keys`；`line_by_line` → `tracked_lines` |
| `mergePreview` | `virtual std::string mergePreview(QWidget* editor) const = 0;` | 返回 `""`（未实现） |
| `trackedLines` | `virtual std::vector<int> trackedLines(const std::string& content, const std::vector<std::string>& trackedKeys) const;` | 正则 `^\s*"([^"]+)"\s*:` 匹配行内键；键或点分末段命中即计入（1-based） |

详细用法见 [usage.md](usage.md)。