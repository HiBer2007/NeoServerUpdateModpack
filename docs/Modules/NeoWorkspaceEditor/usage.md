# NeoWorkspaceEditor 使用文档

## 快速开始

### 构建

随主配置 `msvc` 预设一起构建（根 CMake `add_subdirectory(modules/NeoWorkspaceEditor)`），产物进入 `build/deploy/`：

```powershell
$cmake = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
& $cmake --preset msvc
& $cmake --build build --clean-first --target neo_deploy
```

单独构建亦可：`& $cmake --build build --target NeoWorkspaceEditor`（windeployqt 已在 POST_BUILD 内执行）。

### 运行

```powershell
# 打开无仓库的编辑器（可通过 仓库→打开仓库 选择）
.\build\deploy\NeoWorkspaceEditor.exe

# 直接加载仓库目录或 workspace.json
.\build\deploy\NeoWorkspaceEditor.exe "K:\repo\my-modpack"
.\build\deploy\NeoWorkspaceEditor.exe "K:\repo\my-modpack\workspace.json"
```

## 公共 API

本模块为 EXE 应用，公共面 = 命令行入口 + 少量窗口/对话框类（头文件在 `src/`，无 `include/` 目录）。

### 命令行入口（main.cpp）

> **没有 CLI 子命令模式**：编辑器不是 CLI 工具。`main.cpp` 仅用 QCommandLineParser 提供 help/version 与一个可选位置参数（要打开的 workspace.json 文件路径），然后进入 GUI 事件循环。加载期终端上出现的大字卡片 + 日志滚动 + 进度条是 **`nsum_tui::EditorTui` 加载叠加层**（窗口创建期间短暂渲染，主窗口展示后 `stop()` 重放缓冲日志并恢复普通输出），并非「CLI 模式」。

| 参数 | 说明 |
|------|------|
| `-h` / `--help` | 打印 `QCommandLineParser` 帮助（描述：`NeoServer 工作区配置文件编辑器`） |
| `-v` / `--version` | 打印版本（QApplication 版本 `1.0.0`） |
| `[file]`（位置参数） | 仓库目录或 `workspace.json` 路径；目录 → 直接 `loadWorkspace(dir)`；文件 → `loadWorkspace(父目录)` |

环境变量：`NSUM_TUI_HOLD_MS`（整数毫秒）控制**窗口创建前**加载动画的停留时长，`=0` 跳过停留（进度直接推进到 95/98/100）。TUI 仅在 stdout 连接真实控制台时创建（重定向到文件则不渲染，日志直接输出）。

### EditorWindow（editor_window.h）

```cpp
class EditorWindow : public QMainWindow {
public:
    explicit EditorWindow(QWidget* parent = nullptr);
    ~EditorWindow();
    bool loadWorkspace(const std::string& dirPath,
        RepoSource source = RepoSource::Local);
    bool saveWorkspace();
    bool saveWorkspaceAs();
    bool isModified() const;
};
enum class RepoSource { Local, RemoteCache, Clone, New };
```

要点：
- `loadWorkspace` 返回 `bool`；既非 Git 仓库也无 `workspace.json` → 弹错返回 `false`；有 `workspace.json` 无 Git → 询问初始化并提交；Git 仓库缺 `workspace.json` → 询问后走：本地恢复（fetch/reset/checkout/show 历史）→ 找最近有效提交 → 创建默认配置。
- 陌生仓库（`GitOperations::isDubiousOwnership`）→ 弹「是否信任该仓库并继续加载?」，信任（`trustRepository`，safe.directory 正斜杠规范化）后继续。
- 加载流程末尾自动执行异步完整性检查（`git status --porcelain -b`），有未处理文件时弹出逐项处理对话框。
- 保存时把 `git.current_branch` 写回 `workspaceConfig_["git"]["current_branch"]`。
- 每分支配置存 `<repo>/branch_config/<name>.json`（默认结构：`file_manifest`/`pointer_files`/`serverconfig_sync`/`custom_mods`），保存即 `git add -A`。

### BranchMetaDialog（branch_meta_dialog.h）

```cpp
class BranchMetaDialog : public QDialog {
public:
    explicit BranchMetaDialog(const QString& repoDir, QWidget* parent = nullptr);
};
```

编辑每个 Git 分支 `workspace.json` 顶层的 `description` / `hidden`（主程序据此隐藏分支）。保存对每个修改过的分支执行：`git checkout -f <branch>` → 写 `workspace.json` → `git add` + `git commit -m "chore: update branch meta (<name>)"` → （本地分支）`git push origin <name>` → 切回原分支。

### nsum_tui（editor_tui.h）

```cpp
namespace nsum_tui {
class TuiLogSink final : public spdlog::sinks::sink {
public:
    void log(const spdlog::details::log_msg& msg) override;
    void flush() override {}
    void set_pattern(const std::string&) override;
    void set_formatter(std::unique_ptr<spdlog::formatter>) override;
    std::vector<std::string> snapshot(size_t maxLines) const;
};
class EditorTui {
public:
    EditorTui(); ~EditorTui();
    void start();
    void stop();
    void setStatus(const std::string& stage);
    void setProgress(int percent);
};
}
```

`TuiLogSink` = 加载期日志缓冲 sink（拦截 CLogger 的 stdout sink，无 ANSI 无换行）；`EditorTui` = 渲染线程（120ms/帧）：居中拼接字卡片（亮青）+ 日志滚动（底部对齐）+ 底部进度条/阶段文本。`stop()` 恢复 stdout sink 并重放最近 2000 行缓冲。

### 其他入口侧符号

- 日志：`CLogger::Init("workspace_editor.log", "editor")`（CommonLoggerCPP，全局命名空间）。
- 崩溃：`HiBerCTM::InstallCrashHandler()` / `InstallCrtReportHook()` / `SetCrashAppName("NSUM Editor")` / `SetCrashHelpText(...)` / `AddCrashTypeInfo(...)`（CrashTrackerHandleLib）。
- Git 环境：`InstallConfig::load()`（根 `src/install_config.h`，字段 `valid/errorMsg/gitPath/useSystemGit`）→ `NeoWorkspace::GitOperations::SetDefaultGitPath(icfg.gitPath)` → `gitPanel_->setGitPath(...)`（`GetDefaultGitPath()`）。

## 典型用法

1. **打开仓库**：`仓库 → 打开仓库` 弹来源选择（本地目录 / 克隆远程 / 远程仓库本地缓存），克隆目标 `QStandardPaths::CacheLocation/NSUM/<repoName>`；或命令行带路径直接进。
2. **三页编辑**：仓库设置（workspace/git/custom_mods/sync_policies）→ 分支管理（继承关系）→ 整合包内容（文件树 + 预览 + 各种规则编辑器 + 指针转换 + 拖入导入）。
3. **Git 操作**：左栏 GitPanel 全异步（commit/push/pull/fetch/stage/unstage/合并提交 squash 等）；顶部 `分支:` 下拉切换内容视图对应分支；`Git → 分支操作 → 分支属性配置` 改描述/隐藏。
4. **.gitignore**：`Git → 忽略文件 (.gitignore)` →「图形化编辑」（GitIgnoreMarkup 规则勾选列表）或「直接编辑」（HiBerGUI CodeEditor + GitIgnoreHighlighter）；保存后自动 `git add` + 刷新 GitPanel。
5. **XML/JSON 校验**：`工具 → 验证配置 (Ctrl+E)`；`工具 → 完整性检查 (Ctrl+I)` 逐文件选择 跟踪/还原/删除/取消暂存/撤销追踪(+忽略)/跳过。
6. **帮助**：`帮助 → 帮助文档` 经 `PowerHelper::Bridge::launchReader(defaultDocsDir())` 拉起 PowerHelper；PowerHelper.exe 缺失时弹「无法打开帮助」。

## 注意事项

- **布局失配需全量重建（最高频陷阱）**：改动被多 target 共享的头文件类成员（如 `modpack_content_ide.h`、`editor_window.h`、GitPanel 类）后，引用它的**每个** target 都得重建——`main.cpp`（内 `new EditorWindow()`）这类入口 TU 最易漏，旧 obj 按旧 sizeof 分配、新代码越界写 → 关闭时 `HEAP CORRUPTION 0xC0000374`。修法：删旧 obj 强制重编或 `--clean-first` 全量重建后冒烟；不信 ninja "no work to do"。
- **GUI 冒烟流程**：启动 → 等 3-4s 确认窗口已建好仍在运行 → CloseMainWindow → 期待 exit 0，并对比 `crash-report/` 文件数；打开后 1-2s 就关会漏检窗口未建完即崩的回归。
- **终端策略**：控制台子系统。从 cmd 启动保持终端（`GetConsoleProcessList>1`，设 UTF-8 代码页 + VT）；双击启动待窗口 show 后 `FreeConsole()` 释放独占控制台（黑窗即加载动画）。stdout 重定向到文件时不渲染 TUI。
- **编辑器日志**：`workspace_editor.log`（logger 名 `editor`），终端启动时同样输出到控制台（`[EDITOR] Log output to this terminal` 标记）。
- **崩溃测试标签**：右下角版本标签按住 Ctrl + 按 N/S/D/T/B/I/F/U/G 之一再点按释放 → 10 秒倒计时后引爆炸对应崩溃（开发期验证 CrashTracker 用，勿在产品环境触发）。
- **配置重置**：`config/custom/editor.ini`（窗口几何/分割宽度/最近列表/两树列布局）删除即重置；崩溃帮助文案也提示此举。
- **git 路径**：编辑器按 install.conf → 系统 git → `tools/git/bin/git.exe` → `../tools/git` 顺序解析（`InstallConfig::load`），非 PATH 环境也能工作。
- **完整性检查已走 `-z`（✅ 已修复，2026-08-30）**：`runIntegrityCheck` 现执行 `git status --porcelain -b -z`，`finishIntegrityCheck` 按 NUL 记录拆分（`## ` 分支头也是 NUL 结尾记录），路径原样 UTF-8，不再有带引号/八进制转义导致的 `git add` 匹配失败风险。
- **目录清理**：加载时 `ensureGitIgnore` 保证 `.NSUM/`（editor-trash/pointer-cache/hashes.json）不被追踪；不要手工把 `.NSUM` 提交进仓库。
- **分支切换语义**：`Git → 切换分支` 会切 git 分支并把 `git.current_branch` 写入 workspace.json 后重载；`workspace.json` 的整合包分支名通常不是 git 分支名（构建时 checkout/merge 语义不同，见 NeoBuild 文档）。

## 相关文档

- [NeoWorkspaceEditor README](README.md) — 说明文档
- NeoWorkspace — GitOperations 领域封装
- GUIWorker — RepoEditor/BranchEditor/ModpackContentIde 领域编辑器
- HiBerGUILibrary — GitPanel/ProgressCard/CodeEditor 组件
- GitIgnoreMarkup — .gitignore 标记系统与 GitIgnoreDialog
- PowerHelper — 帮助文档阅读器入口（`PowerHelper::Bridge::launchReader`）
- CrashTrackerHandleLib — `HiBerCTM::` 崩溃捕获