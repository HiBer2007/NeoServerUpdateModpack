# EditorDemo 说明文档

## 概述

EditorDemo 是 HiBerGUILibrary **编辑器套件的用法演示 EXE 应用**（产物 `editor_demo.exe`），用于对照评价编辑器双版本实现与 merge 预览窗口：启动后依次查看三个标签页——Qt 版 `CodeEditor`、WebView2 版 `WebCodeEditor`、`MergePreviewDialog` 双版本对比（效果/性能/体积）。它不参与产品功能链路，是开发期验证与选型工具；模块内容仅一个 `src/main.cpp`（**无 `include/` 目录**）。

## 设计目标

- **双实现并排演示**：同一 `ICodeEditor` 接口的 Qt 纯 C++ 实现与 WebView2 实现同屏对比，验证「接口一致、实现可换」的抽象设计。
- **API 用法示范**：演示 `ICodeEditor` 全套能力——语言切换、深/浅主题、只读、Tab 宽度、区域标记（`RegionHighlight`）、扩展动作（`EditorAction`）。
- **merge 预览选型**：以真实 serverconfig 合并场景（JSON 样本 + 追踪键）预览 Qt 版与 Web 版 `MergePreviewDialog`，为产品实际选用提供依据（代码注释：已选定 Qt 版用于产品 `ConfigFileEditor`，Web 版仅对比用）。
- **Web 工厂注册示范**：展示宿主如何调用 `registerCodeEditorFactory(CodeEditorKind::Web, ...)` 注册 Web 实现（仅链接 HiBerGUIWebEditor 的程序可注册）。

## 模块边界

**做什么**

- 演示 HiBerGUI 编辑器套件组件用法：`CodeEditor`、`WebCodeEditor`、`MergePreviewDialog`、`builtinLanguages()`、工厂注册机制。
- 提供 `--web` 启动参数：启动即切到 WebView2 标签页，便于诊断 Web 版性能与就绪耗时。
- POST_BUILD 复制 `WebView2Loader.dll` 并运行 windeployqt，保证演示 EXE 可直接运行。

**不做什么**

- 不做任何编辑/构建/部署功能；不参与 `neo_deploy` 部署（根 CMake `DEPLOY_DEPS` 不含 editor_demo，产品发布包不带它）。
- 不改动 HiBerGUI 组件本身；无自有公共 API 供其他模块使用（`DemoWindow` 为演示私有）。

## 依赖关系

| 依赖 | 类型 | 用途 |
|------|------|------|
| HiBerGUILibrary | STATIC 库 | `CodeEditor`（Qt 版）、`MergePreviewDialog`、`ICodeEditor`/`builtinLanguages()` 等接口 |
| HiBerGUIWebEditor | STATIC 库 | `WebCodeEditor`、`createWebCodeEditor`、Web 工厂注册（引入 WebView2 依赖） |
| Qt6::Core / Qt6::Widgets | vcpkg/系统 Qt | 应用基础 |
| WebView2Loader.dll | POST_BUILD 从 vcpkg_installed 复制到产物目录 | WebView2 加载器（仅当程序使用 Web 版时需要；运行期还需系统 WebView2 运行时） |

**部署形态**：`editor_demo.exe`（控制台子系统）+ 随目录的 Qt DLL（windeployqt）+ `WebView2Loader.dll`。仅开发/演示用途，不进入发布包。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | target `editor_demo`；链接 HiBerGUILibrary/HiBerGUIWebEditor/Qt；POST_BUILD 复制 WebView2Loader.dll + windeployqt |
| `src/main.cpp` | `DemoWindow`（三标签：Qt 版编辑器 / WebView2 版编辑器 / merge 预览对比）+ `main`（注册 Web 工厂、`--web` 切页） |

## 构建集成

- **CMake target**：`editor_demo`（`add_executable`，随主配置 `msvc` 预设构建，根 CMake `add_subdirectory(modules/EditorDemo)`）。
- **链接**：PRIVATE 链接 `HiBerGUILibrary`、`HiBerGUIWebEditor`、`Qt6::Core`、`Qt6::Widgets`；包含目录 `src`、`../HiBerGUILibrary/include`、`../HiBerGUIWebEditor/include`。
- **部署产物**：无版本资源（未调用 `nsum_add_version_info`）；POST_BUILD 复制 `WebView2Loader.dll`（按构建类型取 vcpkg_installed debug/release bin）+ windeployqt（`--no-translations --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler`）。
- **运行前提**：Web 标签页需系统 WebView2 运行时；缺失时 `WebCodeEditor` 显示错误占位、不崩溃。

## 命名空间与公共符号

| 符号 | 位置 | 说明 |
|------|------|------|
| `DemoWindow` | main.cpp（全局） | 演示主窗口（三标签） |
| `main(int, char**)` | main.cpp | 入口；`--web` 启动即切 Web 标签页 |
| `HiBerGUI::CodeEditor` / `WebCodeEditor` | HiBerGUI 库 | 演示的两版编辑器实现（接口一致） |
| `HiBerGUI::MergePreviewDialog` | HiBerGUI 库 | 通用 merge 预览窗口（`CodeEditorKind` 可选 Qt/Web） |
| `HiBerGUI::registerCodeEditorFactory` / `createCodeEditor` / `builtinLanguages` / `ICodeEditor` 等 | HiBerGUI 库 | 演示用到的组件接口（详见 usage.md 公共 API 表） |

> 演示用的组件接口、`CodeEditorKind`、`RegionHighlight`、`EditorAction`、`HighlightStyle` 均定义于 HiBerGUI 库（`code_editor_interface.h`），本模块只消费不定义。