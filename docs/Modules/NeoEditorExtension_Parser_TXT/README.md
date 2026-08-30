# NeoEditorExtension_Parser_TXT 说明文档

## 概述

NeoEditorExtension_Parser_TXT 是整合包内容 IDE（NeoWorkspaceEditor）的**配置编辑器扩展插件**（SHARED DLL）。它为 TXT / properties / cfg 类无结构化键的配置文件提供「同步模式选择 + 行号追踪」的专用编辑界面：只读展示文件内容，以**行号**为单位勾选需要追踪的行，生成同步规则（`syncRules`，`line_by_line` 策略 + `tracked_lines`）。本插件只负责编辑界面；文本的实际逐行同步/合并由配对的 `NeoParser_TXT`（`IConfigParser`）完成。

## 设计目标

- 为无键结构的文本配置提供行级追踪的可视化编辑入口（`ConfigEditor` 行模式：表头为 `行号`）。
- 以 `meta.json` 声明的扩展名集合（`".txt"` / `".properties"` / `".cfg"`）驱动注册。
- 以 `tracked_lines`（整数行号数组）作为唯一追踪维度，与 `line_by_line` 策略配对。
- 与 `NeoParser_TXT` 保持格式配对（`parse_lines` / `merge_lines`）。

## 模块边界

**做什么：**

- 提供实现 `NeoCore::IConfigEditorExtension` 的扩展实例与导出工厂 `CreateConfigEditor`。
- 以行模式创建 `ConfigEditor`（`setLineMode(true)` → 树表头 `行号` / `追踪`）。
- 读取 `syncRules["tracked_lines"]` 预置行勾选、序列化 `policy` + `tracked_lines`。
- 行追踪：`setTrackedLines` / `trackedLines` 以内容行号（1-based）为追踪单元。

**不做什么：**

- 不做文本解析/合并（属于 `NeoParser_TXT`）。
- 不支持键级追踪（`config_merge` → `tracked_keys` 不产出）。
- 不实现 `trackedLines(content, trackedKeys)` 键定位（沿用接口默认空数组 → 宿主回退整行标记）。
- 不做文件读写、Git 操作；`mergePreview()` 恒返回 `""`。
- 不注册为配置解析器（`IConfigParser`）。

## 依赖关系

以 `modules/NeoEditorExtension_Parser_TXT/CMakeLists.txt` 为准：

| 依赖 | 链接方式 | 说明 |
|------|----------|------|
| `NeoCore` | `PRIVATE` | `IConfigEditorExtension` 接口；头文件路径 `../../NeoCore/include` |
| `Qt6::Widgets` | `PRIVATE` | `ConfigEditor` 控件 |
| nlohmann-json | 经 NeoCore 传递 | `editor_parser_txt.cpp` 直接 `#include <nlohmann/json.hpp>` |
| `../NeoEditorExtension_Parser_JSON/src/config_editor.{h,cpp}` | 本模块编译的共享源码 | 界面类（六族共用） |

> 反向依赖：根 CMakeLists（`CMakeLists.txt:130`）`add_subdirectory(modules/NeoEditorExtension_Parser_TXT)`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `src/editor_parser_txt.cpp` | `TXTConfigEditorExtension : NeoCore::IConfigEditorExtension` + `CreateConfigEditor` 导出 |
| `src/config_editor.h` / `src/config_editor.cpp` | 共享 `ConfigEditor` 界面类（行模式） |
| `NeoEditorExtension_Parser_TXT.meta.json` | 扩展元数据 |
| `CMakeLists.txt` | SHARED 目标定义：`PRIVATE NeoCore Qt6::Widgets`，`PREFIX ""` |

## 构建集成

- CMake target：`NeoEditorExtension_Parser_TXT`，类型 **SHARED**。
- `set_target_properties(NeoEditorExtension_Parser_TXT PROPERTIES PREFIX "")`：产物 DLL 名为 `NeoEditorExtension_Parser_TXT.dll`。
- **导出符号**（`src/editor_parser_txt.cpp:40`）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
      return new TXTConfigEditorExtension();
  }
  ```

  导出名为 `CreateConfigEditor`。
- 版本资源：根 CMakeLists `nsum_add_version_info(NeoEditorExtension_Parser_TXT "NSUM 配置编辑器扩展 (NeoEditorExtension_Parser_TXT)" "NSUM构建工具")`。
- **部署目录**：`neo_deploy` POST_BUILD 将 DLL 与 meta.json 拷贝到 `${DEPLOY_DIR}/editor/extension/parsers/`。

## 公共符号

| 符号 | 类别 | 说明 |
|------|------|------|
| `CreateConfigEditor` | `extern "C" __declspec(dllexport)` 导出函数 | 返回 `NeoCore::IConfigEditorExtension*` |
| `TXTConfigEditorExtension` | class（匿名 TU 内） | 实现 `IConfigEditorExtension` 的扩展实例（行模式） |
| `ConfigEditor` | class（全局可见，非导出） | 共享编辑界面（`setLineMode(true)`） |

### 实现类接口概览（`IConfigEditorExtension`）

| 方法 | 本模块实现 |
|------|-----------|
| `fileExtension()` | 返回 `".txt"` |
| `createEditor(parent)` | `new ConfigEditor(parent)` 后 `ce->setLineMode(true)` |
| `loadConfig` | 内容 = remote 非空取 remote 否则 local；`policy` 默认 `"full_sync"`；`syncRules["tracked_lines"]`（整数数组）→ `setTrackedLines` |
| `saveSyncRules` | `policy`；仅 `line_by_line` → `tracked_lines`（收集勾选行号） |
| `mergePreview` | 返回 `""` |
| `trackedLines` | **未重写**（沿用接口默认返回 `{}`） |

详细用法见 [usage.md](usage.md)。