# NeoEditorExtension_Parser_TOML 说明文档

## 概述

NeoEditorExtension_Parser_TOML 是整合包内容 IDE（NeoWorkspaceEditor）的**配置编辑器扩展插件**（SHARED DLL）。它为 TOML 配置文件提供「同步模式选择 + section.key 键追踪」的专用编辑界面：只读展示配置内容，维护段（`[section]`）上下文把键展开为 `段.键` 完整路径，勾选需追踪的键并生成同步规则（`syncRules`）。本插件只负责编辑界面与键定位；TOML 的实际解析与合并由配对的 `NeoParser_TOML`（`IConfigParser`）完成。

## 设计目标

- 为 TOML 配置提供「同步模式 + section.key 追踪」可视化编辑入口。
- 以 `meta.json` 声明的扩展名（`".toml"`）驱动注册，按后缀自动路由。
- `trackedLines()` 还原段上下文：`[section]` 前缀 + 键名拼接完整路径，供宿主 merge 预览行标记。
- 与 `NeoParser_TOML` 保持格式配对。

## 模块边界

**做什么：**

- 提供实现 `NeoCore::IConfigEditorExtension` 的扩展实例与导出工厂 `CreateConfigEditor`。
- 提供 `ConfigEditor` 编辑界面（共享源码）。
- 提取 TOML 键（`section.key` 形式）、按 `syncRules` 预置勾选、序列化 `policy` + `tracked_keys`。
- 实现 `trackedLines()`（段上下文 + 键名/完整路径匹配，1-based）。

**不做什么：**

- 不做 TOML 解析/合并（属于 `NeoParser_TOML`，tomlplusplus 在解析器侧）。
- 不支持 `line_by_line` 的 `tracked_lines` 序列化。
- 不做文件读写、Git 操作；`mergePreview()` 恒返回 `""`。
- 不注册为配置解析器（`IConfigParser`）。

## 依赖关系

以 `modules/NeoEditorExtension_Parser_TOML/CMakeLists.txt` 为准：

| 依赖 | 链接方式 | 说明 |
|------|----------|------|
| `NeoCore` | `PRIVATE` | `IConfigEditorExtension` 接口；头文件路径 `../../NeoCore/include` |
| `Qt6::Widgets` | `PRIVATE` | `ConfigEditor` 控件 |
| nlohmann-json | 经 NeoCore 传递 | `editor_parser_toml.cpp` 直接 `#include <nlohmann/json.hpp>` |
| `../NeoEditorExtension_Parser_JSON/src/config_editor.{h,cpp}` | 本模块编译的共享源码 | 界面类（六族共用） |

> 反向依赖：根 CMakeLists（`CMakeLists.txt:128`）`add_subdirectory(modules/NeoEditorExtension_Parser_TOML)`。

## 文件组成

| 文件 | 说明 |
|------|------|
| `src/editor_parser_toml.cpp` | `TOMLConfigEditorExtension : NeoCore::IConfigEditorExtension` + `CreateConfigEditor` 导出；含 `extractTomlKeys` 辅助函数 |
| `src/config_editor.h` / `src/config_editor.cpp` | 共享 `ConfigEditor` 界面类 |
| `NeoEditorExtension_Parser_TOML.meta.json` | 扩展元数据 |
| `CMakeLists.txt` | SHARED 目标定义：`PRIVATE NeoCore Qt6::Widgets`，`PREFIX ""` |

## 构建集成

- CMake target：`NeoEditorExtension_Parser_TOML`，类型 **SHARED**。
- `set_target_properties(NeoEditorExtension_Parser_TOML PROPERTIES PREFIX "")`：产物 DLL 名为 `NeoEditorExtension_Parser_TOML.dll`。
- **导出符号**（`src/editor_parser_toml.cpp:83`）：

  ```cpp
  extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor() {
      return new TOMLConfigEditorExtension();
  }
  ```

  导出名为 `CreateConfigEditor`。
- 版本资源：根 CMakeLists `nsum_add_version_info(NeoEditorExtension_Parser_TOML "NSUM 配置编辑器扩展 (NeoEditorExtension_Parser_TOML)" "NSUM构建工具")`。
- **部署目录**：`neo_deploy` POST_BUILD 将 DLL 与 meta.json 拷贝到 `${DEPLOY_DIR}/editor/extension/parsers/`。

## 公共符号

| 符号 | 类别 | 说明 |
|------|------|------|
| `CreateConfigEditor` | `extern "C" __declspec(dllexport)` 导出函数 | 返回 `NeoCore::IConfigEditorExtension*` |
| `TOMLConfigEditorExtension` | class（匿名 TU 内） | 实现 `IConfigEditorExtension` 的扩展实例 |
| `extractTomlKeys` | `static` 函数 | 段上下文 + `key =` 行正则，产出 `段.键`（如 `server.port`） |
| `ConfigEditor` | class（全局可见，非导出） | 共享编辑界面 |

### 实现类接口概览（`IConfigEditorExtension`）

| 方法 | 本模块实现 |
|------|-----------|
| `fileExtension()` | 返回 `".toml"` |
| `createEditor(parent)` | `new ConfigEditor(parent)` |
| `loadConfig` | 内容 = remote 非空取 remote 否则 local；`policy` 默认 `"full_sync"`；`extractTomlKeys` 提取键 + `tracked_keys` 预置勾选 |
| `saveSyncRules` | `policy`；仅 `config_merge` → `tracked_keys` |
| `mergePreview` | 返回 `""` |
| `trackedLines` | 维护 `section` 字符串（`[section]` 行重置为 `section.`）；`fullKey = section + localKey`；追踪键 == `localKey` 或 == `fullKey` 即命中（1-based） |

详细用法见 [usage.md](usage.md)。