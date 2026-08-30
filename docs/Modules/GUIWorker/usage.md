# GUIWorker 使用文档

本文档面向把 GUIWorker 集成进宿主程序的开发者：向导如何启动/驱动、`ModpackContentIde` 如何嵌入编辑器、各规则编辑器接口如何调用，以及已知陷阱。所有 API 名称、签名、信号、枚举均逐字摘自 `modules/GUIWorker/include/` 头文件；与代码不一致时以代码为准。

## 快速开始

**1. CMake 链接**（GUIWorker 为 STATIC 库，头文件路径已随 `PUBLIC include` 暴露）：

```cmake
target_link_libraries(YourApp PRIVATE GUIWorker)
# 头文件即可用：
# #include <wizard_window.h>
# #include <modpack_content_ide.h>
```

**2. 在主程序启动 9 页向导**（参考 `src/main.cpp` runGuiMode）：

```cpp
auto* wizard = new GUIWorker::WizardWindow();
wizard->setAttribute(Qt::WA_DeleteOnClose);
wizard->show();
```

**3. 把 `ModpackContentIde` 嵌入编辑器 tab**（参考 `NeoWorkspaceEditor/src/editor_window.cpp`）：

```cpp
auto* ide = new GUIWorker::ModpackContentIde(tabWidget_);
tabWidget_->addTab(ide, "内容");
// 之后必须设置仓库与分支，IDE 才可工作：
ide->setRepository(dir);
ide->setBranch(QString::fromStdString(branchName), branchConfigDir);
```

嵌入后按需连接保存/变更信号（见「公共 API」）。

## 公共 API

### 1. 向导：`wizard_window.h`

| 成员 | 签名 | 说明 |
|------|------|------|
| `FlowConfig` | `struct { QString startPage; QString endPage; bool collectOnly=false; QMap<QString,QString> prefill; }` | Flow（CLI `flow gui`）配置；页名 `repo|branch|modpack|export-type|export-dir|extra-info|checklist|build|done` |
| ctor | `explicit WizardWindow(QWidget* parent = nullptr)` | 构造向导 |
| `setFlowMode` | `void setFlowMode(const FlowConfig& cfg)` | 进入 Flow 模式：预填页自动跳过、终点页收集数据并 emit `flowDataReady` |
| `pageNameToIndex` | `static int pageNameToIndex(const QString& name)` | 页名 → 页索引（非法返回负值）；页面常量 `PAGE_REPO=0…PAGE_DONE=8` |
| `flowTriggerNext` | `void flowTriggerNext()` | 自动推进一页（测试钩子用） |
| `flowDone` | `bool flowDone() const` | Flow 是否已结束 |
| 信号 | `void flowDataReady(const QString& json)` | Flow 终点收集完成，携带 JSON（结构见「典型用法」） |

Flow JSON 输出（`wizard_window.cpp flowCollectJson`）：

```json
{ "category": "flow", "command": "gui",
  "data": { "repo": "...", "repo_local_path": "...", "branch": "...",
            "modpack": "...", "format": "...", "export_dir": "...",
            "extra": { "<字段key>": "<值>" } } }   // extra 仅在有字段时出现
```

prefill 键：`repo` / `branch` / `modpack` / `format` / `exportdir`，外加 exporter meta 定义的额外字段键。

### 2. 向导页（`repo_page.h` / `branch_page.h` / `modpack_page.h` / `export_type_page.h` / `export_dir_page.h` / `extra_info_page.h` / `build_checklist_page.h` / `build_page.h` / `done_page.h`）

| 类 | 公共 API | 信号 |
|----|----------|------|
| `RepoPage` | `QString repoUrl() const; bool isValid() const; void setUrl(const QString&); int sourceType() const; void recordRecentCache(const QString& url, const QString& cachePath); void commitToHistory();` 枚举 `enum SourceType { SourceRemote=0, SourceLocal=1, SourceCache=2 }` | `repoReady(QString url)`、`validityChanged(bool valid)` |
| `BranchPage` | `void loadBranches(const QString& repoPath); void showLoading(const QString& status, int percent = -1); void stopLoading(); QString selectedBranch() const; bool hasSelection() const; void selectBranch(const QString& name); int contentHeight(int widthHint = 500) const;`（数据 `GitBranchInfo{name, description, isDefault, hidden}`） | `branchSelected(QString branch)`、`branchesLoaded(bool success)` |
| `ModpackPage` | `void loadModpacks(const QString& repoPath); QString selectedModpack() const; bool hasSelection() const; void selectModpack(const QString& name); QString summary(const QString& branchName) const; int contentHeight(int widthHint = 500) const;`（数据 `ModpackBranchInfo{name, parent, gameVersion, modloader, modloaderVersion, description, hidden}`） | `modpackSelected(QString modpackBranch)`、`modpacksLoaded(bool success)` |
| `ExportTypePage` | `QString selectedFormat() const; QString selectedFormatName() const; bool hasSelection() const; void selectFormat(const QString& id);` | `formatSelected(QString format)` |
| `ExportDirPage` | `void setContext(const QString& modpackBranch, const QString& formatId, const QString& extension); QString outputPath() const; bool isValid() const; void setPath(const QString& path);` | `dirSelected(QString path)` |
| `ExtraInfoPage` | `bool loadFormat(const QString& formatId); bool hasFields() const; bool validate() const; QMap<QString,QString> values() const; bool setValue(const QString& key, const QString& value); QStringList missingRequired() const; QString formatName() const; void markMissing(const QStringList& missing); int contentHeight(int widthHint) const;`（`loadFormat` 读 `exeDir/exporters/*.meta.json` 的 `format` + `fields[]`，字段键 `key/label/group/type/placeholder/required`） | `validityChanged(bool valid)` |
| `BuildChecklistPage` | `void setSummary(const QString& key, const QString& value); void clearSummary(); void setExtraInfo(const QMap<QString,QString>&); void loadStructure(const nlohmann::json& entries); void clearFileTree(); void expandAll(); void collapseAll(); int contentHeight(int widthHint) const;`（`CollapsibleSection`：`setExpanded/isExpanded/toggle/content()/headerHeight()/contentHeight(widthHint)`，信号 `expandedChanged()`） | `treeExpandedChanged()` |
| `BuildPage` | `void startBuild(QString repoDir, QString gitBranch, QString modpackBranch, QString exportFormat, QString outputPath); void cancelBuild(); QString outputDir() const; const QStringList& warnings() const;` 槽 `appendLog(const QString&)`（内部 `GuiBuildProgress : NeoCore::IBuildProgress` 在后台线程运行 `ModpackExporter` 插件 `build_modpack`，`target.sync_to_directory = (exportFormat == "hmcl")`） | `buildFinished(bool success, QString message, QStringList warnings)`、`progressUpdated(QString stage, int percent, QString message)`、`subProgressUpdated(QString label, int done, int total)`、`logLine(QString line)`、`subBarUpdate(int handle, QString label, int percent, QString info)` |
| `DonePage` | `void showSuccess(const QString& outputDir, const QStringList& warnings = QStringList()); void showFailure(const QString& reason, const QString& suggestion);` | `openOutputDirRequested(const QString& dir)`、`finishRequested()` |

### 3. 领域编辑器：`modpack_content_ide.h`

| 成员 | 签名 | 说明 |
|------|------|------|
| ctor / dtor | `explicit ModpackContentIde(QWidget* parent = nullptr); ~ModpackContentIde() override;` | 构造/析构 |
| `setRepository` | `void setRepository(const QString& repoDir)` | 设置仓库根目录 |
| `setBranch` | `void setBranch(const QString& branch, const QString& branchConfigDir)` | 设置当前整合包分支（分支目录 = `repoDir/branches/<branch>`） |
| `currentBranch` | `QString currentBranch() const` | 当前分支名 |
| `outputPanel` / `repoPanel` | `HiBerGUI::OutputTreePanel* outputPanel() const; HiBerGUI::RepoTreePanel* repoPanel() const;` | 取树面板（类型为 HiBerGUI 组件） |
| `extensionRegistry` | `EditorExtensionRegistry* extensionRegistry() const` | 取编辑器扩展注册表 |
| `runAsyncCommand` | `void runAsyncCommand(const QString& title, const std::function<void(const std::function<void(int, const QString&)>&)>& work, const std::function<void()>& onFinished = {})` | 后台线程执行耗时操作 + 右上角工作卡（busy 期间拒绝并发；非槽） |

槽（public slots）：

```
refreshPreview();  refreshPoliciesView();  refreshFileTree();
reloadExtraConfigFiles();            // workspace.json sync_policies.config_files → 注入两面板
injectConfigEditorExt(relPath);      // 按扩展名注入 IConfigEditorExtension 到 ConfigFileEditor
importDroppedFiles(filePaths, targetRel);
showDropTargetHint(targetRel, hovering);
applyFolderPolicyToSubfolders(folderPath, policy, toBranch);
refreshFolderEditorState();  undoBranchOp();  redoBranchOp();
deleteCurrentSelection();    restoreCurrentSelection();
onCommitFinished(branch);    cleanupOnExit();    rescanExtensions();
```

信号：

| 信号 | 参数 | 说明 |
|------|------|------|
| `contentModified` | — | 任意编辑触发 |
| `folderPolicySaveRequested` | `(QString path, QString policy, bool toBranch)` | 文件夹策略保存（IDE 内部已持久化，宿主按需处理） |
| `filePolicySaveRequested` | `(QString path, QString mode, std::vector<std::string> trackedKeys, std::vector<int> trackedLines, bool toBranch)` | 文件策略保存 |
| `batchPolicySaveRequested` | `(QStringList paths, QString mode, …keys/lines, bool toBranch)` | 批量策略保存 |
| `topSyncPoliciesSaveRequested` | `(QString jsonString)` | 顶层同步策略保存（修改防抖自动保存，代际号丢弃过期保存） |
| `branchConfigChanged` | `(QString branch)` | 分支配置变更 |
| `gitAddRequested` | `(QStringList paths)` | 请求宿主把文件加入 Git 追踪（如 serverconfig 创建、指针转换后回写等） |
| `logMessage` | `(QString line)` | 日志/状态消息 |
| `extensionRegistryChanged` | — | 扩展重扫完成 |
| `configFileMarkChanged` | `(QString rel, bool mark)` | 普通文件 ↔ 配置文件标记变更（宿主持久化到 workspace.json 并保存） |

内部路由规则（`routeObject`，编辑器栈索引）：Root/普通 Folder → `FolderPolicyEditor`（索引 1，根用 `folders[""]`）；路径恰为 `save/[save]/serverconfig` 的 Folder → `ServerConfigRulesEditor`（索引 4）；ConfigFile → `ConfigFileEditor`（索引 2，继承项走 `FileContentEditor`）；Pointer → `PointerEditorPanel`（索引 3）；PlainFile：有同步策略或 `markedConfigs` 标记 → `ConfigFileEditor`（索引 2），文本可编辑 → `FileContentEditor`，否则 → `PointerEditorPanel::loadFileToConvert`（索引 3）；索引 0 为空占位、5 为 `FileContentEditor`、6 为 `BatchEditorPanel`。

### 4. 领域编辑器：分支/仓库/指针

| 类 | 公共 API | 信号 |
|----|----------|------|
| `BranchEditor` | `void loadBranches(const std::vector<nlohmann::json>& branches, const std::string& defaultBranch = ""); std::vector<nlohmann::json> saveBranches() const; bool isModified() const;` | `contentModified()`、`saveRequested(QString jsonString)` |
| `RepoEditor` | `void loadFromJson(const nlohmann::json& config); nlohmann::json saveToJson() const; bool isModified() const;`（内嵌 `SyncPoliciesEditor`，`connectionTestClicked` 供宿主做连接测试） | `contentModified()`、`saveRequested(const QString&)`、`connectionTestClicked(const QString& url)` |
| `PointerEditor` | `void loadPointer(const NeoCore::PointerInfo& ptr); NeoCore::PointerInfo pointerInfo() const; void clear();` | `pointerModified()` |
| `PointerManager`（QDialog） | `explicit PointerManager(QWidget* parent = nullptr, const std::string& branchConfigDir = ""); void setBranchConfigDir(const std::string& dir);` + 槽 `onSelectPointer/onNewPointer/onDeletePointer/onSavePointer/onAddResolver/onRemoveResolver/onBatchConvertJars/onResolverTypeChanged/onCurrentChanged`（旧版，内部加载 `IPointerEditorExtension`） | （无额外信号） |
| `PointerEditorPanel` | `void setContext(const QString& repoDir, const QString& branch, const QString& branchConfigDir); void setExtensionRegistry(EditorExtensionRegistry* reg); void loadPointer(const QString& sha, const NeoCore::PointerFileData& data); void loadFileToConvert(const QString& relPath, const QString& absPath); void batchConvertJars(const QString& folderPath); void batchConvertJarsList(const QStringList& relPaths); void undoBatchConvert(const QVector<ConvertedItem>& items); void redoBatchConvert(const QVector<ConvertedItem>& items); void removeConverted(const QVector<ConvertedItem>& items); bool restorePointerFromCache(const QString& sha, const NeoCore::PointerFileData& data);`（`MaxResolvers = 6`；`ConvertedItem{sha, relPath, cacheAbs, pointerJson}` 已 `Q_DECLARE_METATYPE`） | `pointerSaved(QString sha)`、`branchConfigChanged(QString branch)`、`gitAddRequested(QStringList paths)`、`requestRefresh()`、`logMessage(QString line)`、`batchConvertFinished(QVector<ConvertedItem> items, int failed)` |

### 5. 批量编辑与同步策略

| 类 | 公共 API | 信号 |
|----|----------|------|
| `BatchEditorPanel` | `void loadSelection(const QList<HiBerGUI::RepoObjectInfo>& infos); void clearSelection(); void setHasParser(bool hasParser);` | `batchPolicySaveRequested(...)`、`batchConvertJarsRequested(QStringList relPaths)`、`batchRestorePointersRequested(QList<HiBerGUI::RepoObjectInfo>)`、`batchDeleteRequested(QList<HiBerGUI::RepoObjectInfo>)`、`contentModified()` |
| `BatchConvertCard` | `explicit BatchConvertCard(); void attachTo(QWidget* host); void begin(const QString& title, int total); void setProgress(int done, int failed, const QString& current); void showReport(const QVector<BatchConvertResult>& results, const QString& summary, bool cancelled); bool isActive() const; void dismiss();`（无父构造，须先 `attachTo(宿主窗口)`） | `cancelRequested()`、`closed()` |
| `ConfigFileEditor` | `void load(const QString& relativePath, const QString& absRepoPath, const QString& repoDir, const QString& branch, const QString& effectiveMode, const std::vector<std::string>& effectiveKeys, const std::vector<int>& effectiveLines, bool branchOverrides); void setEditorExtension(NeoCore::IConfigEditorExtension* ext);` 槽 `setScopeTop()` | `saveRequested(relativePath, mode, trackedKeys, trackedLines, toBranch)`、`contentModified()` |
| `FolderPolicyEditor` | `void load(const QString& folderPath, const QString& effectivePolicy, bool branchOverrides, const QString& branchName); QString currentFolderPath() const;` 槽 `setScopeTop()` | `saveRequested(folderPath, policy, toBranch)`、`applyToSubfoldersRequested(folderPath, policy, toBranch)`、`contentModified()` |
| `SyncPoliciesEditor` / `SyncPoliciesDialog` | `void load(const nlohmann::json& policies); nlohmann::json save() const; void setFolderCandidates(const QStringList& dirs);`（Dialog 同款 load/save） | `contentModified()` |
| `sync_policy_display.h` | `QList<QPair<QString, QString>> folderPolicyDisplayItems(); fileModeDisplayItems(); serverConfigModeDisplayItems();` | —（显示中文文本 ↔ 存储英文 ID） |
| `ServerConfigRulesEditor` | `void setContext(const QString& repoDir, const QString& branch); void load();`（数据：`scDir() = <branchDir>/save/[save]/serverconfig`，`ruleDir() = scDir()/.rule`；`globle.json{default_mode（缺省 "overwrite"）, folder_mode（缺省 "mirror"）, version, description}`；`list.json{files:{rel:{mode, tracked_keys}}}`，兼容旧字符串格式；`.rule/` 下其他文件为只读规则文件组） | `gitAddRequested(QStringList paths)`、`logMessage(QString line)` |

### 6. 工具窗口与扩展注册表

| 类 | 公共 API | 信号 |
|----|----------|------|
| `BuildToolWindow` | `explicit BuildToolWindow(QWidget* parent = nullptr); void appendOutput(const QString& text); void clearOutput();` | `commandEntered(QString command)` |
| `EditorExtensionKind` | `enum class { Parser, Pointer }` | — |
| `EditorExtensionInfo` | `struct { QString name; QString version; QString description; QString dllName; EditorExtensionKind kind = EditorExtensionKind::Parser; QStringList fileTypes; }` | — |
| `EditorExtensionRegistry` | `void scan(const QString& baseDir); void unloadAll(); const QList<EditorExtensionInfo>& extensions() const; int count() const; NeoCore::IConfigEditorExtension* configEditorFor(const QString& fileExt) const; NeoCore::IPointerEditorExtension* pointerEditorFor(const QString& resolverType) const; QList<NeoCore::IPointerEditorExtension*> pointerEditors() const; QList<NeoCore::IConfigEditorExtension*> configEditors() const;`（非 QObject；拷贝已删除） | — |

`scan` 语义（`editor_extension_registry.cpp`）：递归扫描 `baseDir` 下 `*.meta.json`；meta 必须含 `dll` 字段（否则跳过）；`editor_type == "parser"` 或含 `extensions` 数组 → Parser（`fileTypes` 取 `extensions[]`，DLL 需导出 `CreateConfigEditor`，类型 `NeoCore::CreateConfigEditorFunc`），否则 → Pointer（`fileTypes` 取 `resolver_type`，DLL 需导出 `CreateEditorExtension`，类型 `NeoCore::CreateEditorExtensionFunc`）；DLL 缺失/加载失败/工厂符号缺失均跳过并 `CLogger::Error`；查询按小写扩展名/类型建映射。`ModpackContentIde::scanExtensionDirs` 扫描 3 个目录并去重：`exeDir/editor/extension`、`exeDir/../editor/extension`、`cwd/build/deploy/editor/extension`。

## 典型用法

**1. 创建向导窗口并收集结果（Flow 模式，供 CLI `flow gui` 复用）**（参考 `src/main.cpp runFlowGuiMode`）：

```cpp
GUIWorker::FlowConfig fcfg;
fcfg.startPage = "modpack";           // 页名: repo|branch|modpack|export-type|export-dir|extra-info|checklist|build|done
fcfg.endPage   = "extra-info";        // 终点页
fcfg.collectOnly = true;              // 只收集不构建
fcfg.prefill["repo"] = "git@github.com:HiBer2007/NeoServerUpdateModpack.git";

auto* wizard = new GUIWorker::WizardWindow();
wizard->setAttribute(Qt::WA_DeleteOnClose);
QObject::connect(wizard, &GUIWorker::WizardWindow::flowDataReady,
    [](const QString& json) { qInfo() << json; });
wizard->setFlowMode(fcfg);
wizard->show();
// 预填页自动跳过；终点页收集后 emit flowDataReady 并 close()
```

**2. 把 `ModpackContentIde` 嵌入编辑器 tab 并连接保存链路**（参考 `NeoWorkspaceEditor/src/editor_window.cpp`）：

```cpp
contentIde_ = new GUIWorker::ModpackContentIde(tabWidget_);
tabWidget_->addTab(contentIde_, "内容");
connect(contentIde_, &GUIWorker::ModpackContentIde::folderPolicySaveRequested, ...);
connect(contentIde_, &GUIWorker::ModpackContentIde::filePolicySaveRequested,  ...);
connect(contentIde_, &GUIWorker::ModpackContentIde::topSyncPoliciesSaveRequested, ...);
connect(contentIde_, &GUIWorker::ModpackContentIde::gitAddRequested,
        this, &EditorWindow::gitAddPaths);                 // 把路径加入 git add
connect(contentIde_, &GUIWorker::ModpackContentIde::contentModified, ...);
connect(contentIde_, &GUIWorker::ModpackContentIde::extensionRegistryChanged, ...);
// 打开仓库/切分支时：
contentIde_->setRepository(dir);
contentIde_->setBranch(QString::fromStdString(branchName), branchConfigDir);
```

**3. 编辑文件树路由到规则编辑器**（IDE 已内置 `routeObject`；宿主只需把树面板的选中项传回）：

```cpp
// HiBerGUI::RepoTreePanel 的 objectActivated(RepoObjectInfo) → 宿主槽
void EditorWindow::onRepoItem(const HiBerGUI::RepoObjectInfo& info) {
    // 无需 switch：IDE 按对象类型自动路由
    //   Folder "save/[save]/serverconfig" → ServerConfigRulesEditor
    //   ConfigFile → ConfigFileEditor，Pointer → PointerEditorPanel ...
}
```

**4. 注册编辑器扩展（独立使用注册表）**：

```cpp
GUIWorker::EditorExtensionRegistry reg;
reg.scan(QCoreApplication::applicationDirPath() + "/editor/extension");
if (auto* parserExt = reg.configEditorFor("json")) {        // -> NeoCore::IConfigEditorExtension*
    configEditor_->setEditorExtension(parserExt);           // merge 预览键值定位
}
if (auto* ptrExt = reg.pointerEditorFor("modrinth")) {      // -> NeoCore::IPointerEditorExtension*
    pointerEditorPanel_->setExtensionRegistry(&reg);        // 或经 IDE 注入
}
const auto exts = reg.extensions();                         // QList<EditorExtensionInfo> 供菜单展示
```

**5. 指针批量转换 + 撤销（`PointerEditorPanel`）**：

```cpp
// mods 工具栏按钮：整目录转指针
pointerEditor_->batchConvertJars(modsDir);
// 拖入导入完成回调：只转实际复制成功的 jar（单次合并调用，勿再二次触发）
connect(pointerEditor_, &GUIWorker::PointerEditorPanel::batchConvertFinished,
    [this](const QVector<GUIWorker::ConvertedItem>& items, int failed) {
        // items: sha/relPath/cacheAbs/pointerJson；宿主可缓存用于 undo
        pointerEditor_->undoBatchConvert(items);            // 撤销：缓存移回分支目录
    });
```

**6. 创建 serverconfig 同步文件夹（IDE 内部流程，供右键菜单触发）**：

```cpp
// 仓库树/输出树 Root 右键 → createServerConfigRequested() → 连接内部即可；
// 流程 = mkpath <branchDir>/save/[save]/serverconfig + 立即写
//   .rule/globle.json {default_mode:"full", folder_mode:"mirror"}
//   .rule/list.json   {files:{}}
// + emit gitAddRequested({dir}) + refreshBranchMeta + refreshPreview
```

## 注意事项

- **serverconfig 同步目录全链路同步**：路径固定为 `branches/<branch>/save/[save]/serverconfig`（`save` = 存档文件夹字面名，`[save]` = 单个存档目录占位，`serverconfig` = 同步目标）。**改此路径必须同时改 6 处**：`serverconfig_sync.cpp`（init ruleBase + sourcePathFor）、`serverconfig_rules_editor.cpp`（scDir + 面板标题）、`modpack_content_ide.cpp`（createServerConfigFolder + routeObject）、`branch_editor.cpp` 帮助文本、`output_tree_panel.cpp` tooltip、相关头文件注释。`globle.json` 缺省 `default_mode="overwrite"`、`folder_mode="mirror"`；`list.json` 新格式 `{mode, tracked_keys}`（与 `sync_policies.files` 一致），兼容旧字符串格式。空目录 git 无法追踪——创建 serverconfig 文件夹**必须先写规则配置**再 `gitAddRequested`。
- **布局失配必须全量重建（三次复发教训）**：GUIWorker 是 STATIC 库，改 `ModpackContentIde` 等被多 target 共享的类成员后，**所有 `new Xxx()` 的 TU 与共享头引用方都必须重编**（含入口 `main.cpp`），否则旧 obj 按旧 `sizeof` 分配、新代码越界写入 → 关闭时 HEAP CORRUPTION 0xC0000374。守则：删旧 obj 强制重编或 `--clean-first` 全量重建，核对 .obj 时间戳晚于头文件，部署后必做 GUI 冒烟（启动 → 等 3-4s → CloseMainWindow → 期待 exit 0 + 无新 crash-report）。
- **`using HiBerGUI::X;` 引用模式**：引用 HiBerGUI 组件时头文件顶部 include 真实头 + 命名空间内 `using`；**勿在 GUIWorker 内嵌 `namespace HiBerGUI` 前置声明块**（会造出 `GUIWorker::HiBerGUI::X` 编译错误），前置声明须放全局作用域。
- **Flow 时序陷阱**：`loadBranches/loadModpacks` 为同步调用且 `branchesLoaded/modpacksLoaded` 在 `navigateTo` **之前** emit → 预填驱动链中途会被 `onRepoReady`/`finishRepoLoading` 尾部 `navigateTo` 回跳，这两处尾部导航必须带 flow 守卫 `if (!(flowActive_ && (flowDone_ || currentPage_ > PAGE_BRANCH/MODPACK)))`。修改 `wizard_window.cpp` 相关逻辑时勿破坏。
- **配置文件识别扩展名清单两处同步**：`isConfigPath`（output_tree_panel.cpp:23）与 `isConfigFile`（repo_tree_panel.cpp:31）共用清单 `.json/.yaml/.yml/.toml/.snbt/.txt/.properties/.ini`；缺扩展名会使 `.ini` 等误入指针转换编辑器。新增扩展名需同时改两处 + 配置解析器插件支持。
- **拖入导入 = 单次合并调用**：`importDroppedFiles` 内部必须一次 `startImport(filePaths, targetRel, ...)`，**不要拆成两批**（第二批会被 `importRunning_` 守卫丢弃）；`makeImportJobs` 无 jar→mods 硬编码，所有文件（含 jar）统一按落点 `targetRel` 路由；完成回调用 `batchConvertJarsList(jarRels)`（`batchConvertJars(modsDir)` 仅保留给 mods 工具栏按钮）。
- **Qt 子对象与生命周期**：`new QObject(this)` 已入父 children，析构中勿再手动 `delete 成员`（统一 stop/close 后置空交父析构）；delete 后不得再访问对象。
- **QLabel sizeHint 陷阱**：wordWrap + RichText 的 QLabel `sizeHint().height()` 无宽度约束时返回巨大值；算含 wrap 文本控件高度用 `layout()->heightForWidth(实际宽度)`。
- **零信任指针缓存**：`restorePointerFromCache` 从 pointer-cache 回写文件前先验 SHA-256；新增读写缓存文件的代码同样必须校验哈希。
- **`PointerManager`（旧 QDialog）与 `PointerEditorPanel`（P3）并存**：新代码应使用 `PointerEditorPanel`；`MaxResolvers = 6`。
- **编辑器扩展注册表**：meta.json 必须含 `dll` 字段；DLL 须导出 `CreateConfigEditor`（parser）/ `CreateEditorExtension`（pointer，带 `__declspec(dllexport)`，否则 GetProcAddress 失败）；扫描目录 `exeDir/editor/extension`、`exeDir/../editor/extension`、`cwd/build/deploy/editor/extension`（去重）；运行期重扫用 `rescanExtensions()`（内部会重建注册表并通知 `extensionRegistryChanged`）。
- **文案与日志约定**：选项文案禁带 `(…)` 括号注释；`CLogger::*`/日志消息一律英文（ANSI 安全），GUI 界面文本用中文。
- **构建部署前提**：`BuildPage`/`ExportTypePage`/`ExtraInfoPage` 依赖 exe 目录下 `exporters/`（含插件 DLL + `*.meta.json`，`fields` 供额外字段页）；预览依赖 `BuildEngine` 虚拟构建链路与 `%TEMP%/NSUM-virtual-build/<分支>/` 临时目录。

## 相关文档

- [模块文档总索引](../README.md)（docs/Modules/README.md）
- [HiBerGUILibrary](../HiBerGUILibrary/README.md) / usage —— 通用组件（树面板/进度/CodeEditor/GitPanel/Toast，本模块 `using HiBerGUI::X` 的来源）
- [NeoBuild](../NeoBuild/README.md) / usage —— 构建引擎、`ModpackExporter`、`IBuildProgress`、`BranchLayer`
- [NeoWorkspace](../NeoWorkspace/README.md) / usage —— `GitOperations`、`workspace_manager`
- [NeoCore](../NeoCore/README.md) / usage —— 插件契约、`CancelToken`、`PluginLoader`
- [NeoWorkspaceEditor](../NeoWorkspaceEditor/README.md) / usage —— 本模块最大宿主（`ModpackContentIde` 嵌入示例）
- 主程序操作指南 `docs/deploy/main/operation-guide.md`、`docs/deploy/main/formats.md`（导出格式）、`docs/deploy/main/troubleshooting.md`
- CLI flow gui：`docs/deploy/CLI/CLI-flow.md`（`WizardWindow` Flow 模式的命令行入口）