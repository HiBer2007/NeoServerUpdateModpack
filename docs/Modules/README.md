# NSUM 模块文档总索引

本文档索引 NeoServerUpdateModpack 全部模块的**说明文档**（`README.md`，回答「这个模块是什么、为什么存在」）
与**使用文档**（`usage.md`，回答「怎么集成、怎么调用、有哪些坑」）。

> 所有文档以**实际源码为准**：API 名称、签名、枚举与常量均摘自对应头文件。
> 若文档与代码不一致，请以代码为准并修正文档。

## 模块分组

### 一、核心库（STATIC，逻辑/引擎，无 GUI 或仅服务性 GUI）

| 模块 | 说明文档 | 使用文档 | 定位 |
|------|----------|----------|------|
| NeoCore | [README](NeoCore/README.md) | [usage](NeoCore/usage.md) | 基础库：插件接口契约（IConfigParser/IPluginPointer/IModpackExporter/编辑器扩展接口）、PluginLoader、CancelToken、错误码、Git 分析 |
| CommonLoggerCPP | [README](CommonLoggerCPP/README.md) | [usage](CommonLoggerCPP/usage.md) | 独立日志模块 CLogger（全局命名空间）+ 插件日志注册约定 |
| GitIgnoreMarkup | [README](GitIgnoreMarkup/README.md) | [usage](GitIgnoreMarkup/usage.md) | .gitignore 标记/逆向标记系统（纯逻辑，可复用）+ GitIgnoreDialog 图形化编辑 GUI |
| NeoWorkspace | [README](NeoWorkspace/README.md) | [usage](NeoWorkspace/usage.md) | 工作区引擎：workspace.json、Git 操作封装、同步引擎、文件扫描、历史记录 |
| NeoBuild | [README](NeoBuild/README.md) | [usage](NeoBuild/usage.md) | 构建引擎：build_engine、分支合并、导出器、指针下载、UMD、serverconfig 同步 |
| NeoCLI | [README](NeoCLI/README.md) | [usage](NeoCLI/usage.md) | 命令行子系统（info/flow/exec 子命令、参数解析、JSON 输出协议） |

### 二、GUI 库与功能子系统

| 模块 | 说明文档 | 使用文档 | 定位 |
|------|----------|----------|------|
| HiBerGUILibrary | [README](HiBerGUILibrary/README.md) | [usage](HiBerGUILibrary/usage.md) | 通用 GUI 组件库（零领域依赖，可独立发布）：CodeEditor 套件、GitPanel、树面板、进度/通知卡片等 |
| HiBerGUIWebEditor | [README](HiBerGUIWebEditor/README.md) | [usage](HiBerGUIWebEditor/usage.md) | 编辑器套件 Web 版（WebView 渲染，ICodeEditor 另一实现） |
| GUIWorker | [README](GUIWorker/README.md) | [usage](GUIWorker/usage.md) | GUI 功能子系统（向导 + 领域编辑器）：主程序 9 页向导、整合包内容 IDE、各类规则编辑器 |
| PowerHelper | [README](PowerHelper/README.md) | [usage](PowerHelper/usage.md) | Markdown 文档阅读器（core/app/bridge 三部分，WebView2 + 回退） |
| CrashTrackerHandleLib | [README](CrashTrackerHandleLib/README.md) | [usage](CrashTrackerHandleLib/usage.md) | 崩溃捕获静态库（HiBerCTM） |
| CrashTracker | [README](CrashTracker/README.md) | [usage](CrashTracker/usage.md) | 崩溃转储分析工具 EXE |

### 三、可执行应用

| 模块 | 说明文档 | 使用文档 | 定位 |
|------|----------|----------|------|
| NeoWorkspaceEditor | [README](NeoWorkspaceEditor/README.md) | [usage](NeoWorkspaceEditor/usage.md) | 工作区编辑器 EXE（整合包内容 IDE） |
| EditorDemo | [README](EditorDemo/README.md) | [usage](EditorDemo/usage.md) | 编辑器套件用法演示 EXE |
| NeoInstaller | [README](NeoInstaller/README.md) | [usage](NeoInstaller/usage.md) | 构建工具安装程序（独立 installer-static 预设构建） |

### 四、插件 DLL（运行时加载）

**配置解析器插件**（实现 `IConfigParser`，部署到 `parsers/`）：

| 模块 | 说明文档 | 使用文档 | 格式 |
|------|----------|----------|------|
| NeoParser_JSON | [README](NeoParser_JSON/README.md) | [usage](NeoParser_JSON/usage.md) | JSON |
| NeoParser_YAML | [README](NeoParser_YAML/README.md) | [usage](NeoParser_YAML/usage.md) | YAML |
| NeoParser_TOML | [README](NeoParser_TOML/README.md) | [usage](NeoParser_TOML/usage.md) | TOML |
| NeoParser_SNBT | [README](NeoParser_SNBT/README.md) | [usage](NeoParser_SNBT/usage.md) | SNBT/NBT（含保序写入器） |
| NeoParser_TXT | [README](NeoParser_TXT/README.md) | [usage](NeoParser_TXT/usage.md) | TXT |
| NeoParser_Properties | [README](NeoParser_Properties/README.md) | [usage](NeoParser_Properties/usage.md) | Properties/INI |

**指针解析器插件**（实现 `IPluginPointer`，部署到 `pointers/`）：

| 模块 | 说明文档 | 使用文档 | 平台 |
|------|----------|----------|------|
| NeoPointer_Modrinth | [README](NeoPointer_Modrinth/README.md) | [usage](NeoPointer_Modrinth/usage.md) | Modrinth API |
| NeoPointer_DirectURL | [README](NeoPointer_DirectURL/README.md) | [usage](NeoPointer_DirectURL/usage.md) | 直链下载 |

**导出插件**（实现 `IModpackExporter`，部署到 `exporters/`）：

| 模块 | 说明文档 | 使用文档 | 格式 |
|------|----------|----------|------|
| NeoExporter_MCBBS | [README](NeoExporter_MCBBS/README.md) | [usage](NeoExporter_MCBBS/usage.md) | MCBBS/PCL/HMCL 通用 .zip |
| NeoExporter_Modrinth | [README](NeoExporter_Modrinth/README.md) | [usage](NeoExporter_Modrinth/usage.md) | .mrpack |
| NeoExporter_HMCL | [README](NeoExporter_HMCL/README.md) | [usage](NeoExporter_HMCL/usage.md) | HMCL 原生格式 |

**编辑器扩展插件**（实现 `IConfigEditorExtension`/`IPointerEditorExtension`，部署到 `editor/extension/`）：

| 模块 | 说明文档 | 使用文档 | 配对 |
|------|----------|----------|------|
| NeoEditorExtension_Parser_JSON | [README](NeoEditorExtension_Parser_JSON/README.md) | [usage](NeoEditorExtension_Parser_JSON/usage.md) | NeoParser_JSON |
| NeoEditorExtension_Parser_YAML | [README](NeoEditorExtension_Parser_YAML/README.md) | [usage](NeoEditorExtension_Parser_YAML/usage.md) | NeoParser_YAML |
| NeoEditorExtension_Parser_TOML | [README](NeoEditorExtension_Parser_TOML/README.md) | [usage](NeoEditorExtension_Parser_TOML/usage.md) | NeoParser_TOML |
| NeoEditorExtension_Parser_SNBT | [README](NeoEditorExtension_Parser_SNBT/README.md) | [usage](NeoEditorExtension_Parser_SNBT/usage.md) | NeoParser_SNBT |
| NeoEditorExtension_Parser_TXT | [README](NeoEditorExtension_Parser_TXT/README.md) | [usage](NeoEditorExtension_Parser_TXT/usage.md) | NeoParser_TXT |
| NeoEditorExtension_Parser_Properties | [README](NeoEditorExtension_Parser_Properties/README.md) | [usage](NeoEditorExtension_Parser_Properties/usage.md) | NeoParser_Properties |
| NeoEditorExtension_Pointer_Modrinth | [README](NeoEditorExtension_Pointer_Modrinth/README.md) | [usage](NeoEditorExtension_Pointer_Modrinth/usage.md) | NeoPointer_Modrinth |
| NeoEditorExtension_Pointer_DirectURL | [README](NeoEditorExtension_Pointer_DirectURL/README.md) | [usage](NeoEditorExtension_Pointer_DirectURL/usage.md) | NeoPointer_DirectURL |

## 文档维护约定

- **新增模块**：必须同时提供 `docs/Modules/<模块>/README.md` 与 `usage.md`，并在本索引登记。
- **修改公共 API（头文件）**：同步更新对应模块的 `usage.md` 公共 API 章节；改插件契约（接口/meta.json）后同步插件族文档。
- **文档依据**：以实际源码为准；发现不一致时以代码为准修正文档。
- **README 回答「是什么」**：概述/设计目标/模块边界/依赖/文件组成/构建集成/公共符号概览。
- **usage 回答「怎么用」**：快速开始/公共 API（摘自头文件）/典型用法/注意事项（陷阱与约定）/相关文档。

## 部署文档链接

- 主程序操作指南：`docs/deploy/main/operation-guide.md`
- 导出格式详解：`docs/deploy/main/formats.md`
- CLI 文档组：`docs/deploy/CLI/`
- PowerHelper 文档：`docs/deploy/PowerHelper/`
- CrashTracker 文档：`docs/deploy/CrashTracker.md`