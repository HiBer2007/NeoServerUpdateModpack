# NeoEditorExtension_Parser_YAML 说明文档

## 概述

NeoEditorExtension_Parser_YAML 是整合包内容 IDE（NeoWorkspaceEditor）的**配置编辑器扩展插件**（SHARED DLL）。它为 YAML / YML 配置文件提供「同步模式选择 + 嵌套键追踪」的专用编辑界面：只读展示配置内容，以缩进段上下文构建完整点分键路径，勾选需追踪的配置键并生成同步规则（`syncRules`）。本插件只负责编辑界面与键定位；YAML 的实际解析与合并由配对的 `NeoParser_YAML`（`IConfigParser`）完成。

## 设计目标

- 为 YAML 配置提供「同步模式 + 嵌套键追踪」可视化编辑入口。
- 以 `meta.json` 声明的扩展名（`".yml"` / `".yaml"`）驱动注册表，按后缀自动路由。
- `trackedLines()` 支持缩进段上下文：按缩进栈拼出父键前缀 + 点分完整路径，供宿主 merge 预览标记行号。
- 与 `NeoParser_YAML` 保持格式配对。

## 模块边界

**做什么：**

- 提供实现 `NeoCore::IConfigEditorExtension` 的扩展实例与导出工厂 `CreateConfigEditor`。
- 提供 `ConfigEditor` 编辑界面（与 JSON 族共用同一份界面源码）。
- 提取 YAML 键（正则匹配 `key:` 行）、按 `syncRules` 预置勾选、序列化 `policy` + `tracked_keys`。
- 实现 `trackedLines()`（缩进栈上下文 + 点分路径匹配，1-based）。

**不做什么：**

- 不做 YAML 解析/合并（属于 `NeoParser_YAML`）。
- 不支持 `line_by_line` 的 `tracked_lines` 序列化（`saveSyncRules` 仅处理 `config_merge` → `tracked_keys`）。
- 不做文件读写、Git 操作；`mergePreview()` 恒返回 `""`。
- 不注册为配置解析器（`IConfigParser`）。

## 依赖关系

以 `modules/NeoEditorExtension_Parser_YAML/CMakeLists.txt` 为准：

| 依赖 | 链接方式 | 说明 |
|------|----------|------|
| `NeoCore` | `PRIVATE` | `IConfigEditorExtension` 接口；头文件路径 `../../NeoCore/include` |
| `Qt6::Widgets` | `PRIVATE` | `ConfigEditor` 控件 |
| nlohmann-json | 经 NeoCore 传递 | `editor_parser_yaml.cpp` 直接 `#include <nlohmann/json.hpp>` |
| `../NeoEditorExtension_Parser_JSON/src/config_editor.{h,cpp}` | 本模块编译的共享源码 | 界面类（与其余 5 个 parser 扩展共用） |

> 反向依赖：根 CMakeLists（`CMakeLists.txt:127`）`add_subdirectory(modules/NeoEditorExtension_Parser_YAML)`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `src/editor_parser_yaml.cpp` | `YAMLConfigEditorExtension : NeoCore::IConfigEditorExtension` + `CreateConfigEditor` 导出；含 `extractYamlKeys` 辅助函数 |
| `src/config_editor.h` / `src/config_editor.cpp` | 共享 `ConfigEditor` 界面类（编译自 JSON 模块目录） |
| `NeoEditorExtension_Parser_YAML.meta.json` | 扩展元数据 |
| `CMakeLists.txt` | SHARED 目标定义：`PRIVATE NeoCore Qt6::Widgets`，`PREFIX ""` |

## 构建集成

- CMake target：`NeoEditorExtension_Parser_YAML`，类型 **SHARED**。
- `set_target_properties(NeoEditorExtension_Parser_YAML PROPERTIES PREFIX "")`：产物 DLL 名为 `NeoEditorExtension_Parser_YAML.dll`。
- **导出符号**（`src/editor_parser_yaml.cpp:86`）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
      return new YAMLConfigEditorExtension();
  }
  ```

  导出名为 `CreateConfigEditor`。
- 版本资源：根 CMakeLists `nsum_add_version_info(NeoEditorExtension_Parser_YAML "NSUM 配置编辑器扩展 (NeoEditorExtension_Parser_YAML)" "NSUM构建工具")`。
- **部署目录**：`neo_deploy` POST_BUILD 将 DLL 与 meta.json 拷贝到 `${DEPLOY_DIR}/editor/extension/parsers/`（与 DLL 同名同目录）。

## 公共符号

| 符号 | 类别 | 说明 |
|------|------|------|
| `CreateConfigEditor` | `extern "C" __declspec(dllexport)` 导出函数 | 返回 `NeoCore::IConfigEditorExtension*` |
| `YAMLConfigEditorExtension` | class（匿名 TU 内） | 实现 `IConfigEditorExtension` 的扩展实例 |
| `extractYamlKeys` | `static` 函数 | 逐行正则 `^\s*([a-zA-Z_][a-zA-Z0-9_.-]*)\s*:` 提取键（不感知缩进，仅取所有匹配行） |
| `ConfigEditor` | class（全局可见，非导出） | 共享编辑界面 |

### 实现类接口概览（`IConfigEditorExtension`）

| 方法 | 本模块实现 |
|------|-----------|
| `fileExtension()` | 返回 `".yaml"` |
| `createEditor(parent)` | `new ConfigEditor(parent)` |
| `loadConfig` | 内容 = remote 非空取 remote 否则 local；`policy` 默认 `"full_sync"`；`extractYamlKeys` 提取键 + `tracked_keys` 预置勾选 |
| `saveSyncRules` | `policy`；仅 `config_merge` → `tracked_keys`（无 `line_by_line` 分支） |
| `mergePreview` | 返回 `""` |
| `trackedLines` | 缩进栈（以 `indent / 2` 估算层级）拼完整路径；`key == fullKey` 或 `key == localKey` 或点分末段 == localKey 即命中（1-based） |

详细用法见 [usage.md](usage.md)。