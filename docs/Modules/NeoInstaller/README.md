# NeoInstaller 说明文档

## 概述

NeoInstaller 是 NeoServerUpdateModpack 的**构建工具安装程序 EXE 应用**（产物 `NeoInstaller.exe`）：Qt 安装向导，把主项目 `neo_deploy` 产出的 `build/deploy/` 目录整体交付到用户机器——释放程序文件、安装或复用 Git、写 `install.conf`、创建桌面/开始菜单快捷方式。它由 `installer-static` 预设**独立构建**（`INSTALLER_ONLY_BUILD=ON`），不参与主配置，也不链接任何项目内库，仅依赖 Qt6（Core/Widgets/Network）。同时支持 GUI 向导与 `--silent` 静默安装（命令行参数驱动，无 GUI）。

## 设计目标

- **一键部署**：将主程序 + NeoWorkspaceEditor + CrashTracker + 全部插件 DLL 与 docs 打包进安装程序（部署文件经 `installer.qrc` **内嵌进 EXE**，静态 Qt 单文件分发，约 48MB）。
- **Git 交付闭环**：内建 git 检测（`GitChecker` / `GitDownloader`）——检测系统 Git；没有则从 GitHub Releases 下载 MinGit（默认）或 PortableGit（勾选编辑器时）并解压到 `tools/git`；同时支持 `--use-system-git` / `--use-bundled-git` 强制。
- **安装可脚本化**：`--silent` 模式走 `SilentInstaller`，终端输出进度，返回 0/1 退出码，供自动化/批量部署。
- **配置落地**：统一写 `<install>/install.conf`（`install_path` / `install_editor` / `use_system_git` / `git_path`），与主程序 `InstallConfig::load()` 读取格式一致。

## 模块边界

**做什么**

- GUI 向导 8 页：欢迎 → 许可（GPL v3，内嵌 LICENSE）→ 安装路径（非空目录警告，安装会清空）→ 组件（核心必装 + 编辑器可选）→ Git 选择（系统/内置）→ Git 许可（GPL v2）→ 进度（释放/下载/解压/快捷方式/配置）→ 完成。
- 静默安装：`NeoInstaller.exe --silent --path <path> [--with-editor] [--use-system-git|--use-bundled-git]`。
- 内嵌资源：`:/deploy/`（整个 deploy 目录，跳过 `.dmp` 与 `config/custom/`）、`:/license/LICENSE`、`:/tools/7za.exe`（.7z 自解压 Git 包解压用）。

**不做什么**

- 不参与主配置构建（根 CMake 仅在 `INSTALLER_ONLY_BUILD` 下 `add_subdirectory`）；不链接项目内库/插件。
- 不做程序运行时的任何逻辑（安装完成即退出；`立即运行 NeoServer` 由 FinishPage 勾选状态决定，最终启动由用户触发）。
- 不提供升级/卸载逻辑（目标目录非空会清空重装）。

## 依赖关系

| 依赖 | 类型 | 用途 |
|------|------|------|
| Qt6::Core / Qt6::Widgets / Qt6::Network | vcpkg/系统 Qt（静态模式 = x64-windows-static） | QWizard 向导、QNetworkAccessManager 下载、文件/QSettings |
| `build/deploy/`（DEPLOY_SOURCE） | 构建期外部目录 | 经 `installer.qrc` 内嵌到 EXE（`:/deploy/`） |
| Windows 内置 `tar` / `powershell` | 运行期系统工具 | Git zip 解压（tar 失败回退 Expand-Archive）；下载经 PowerShell Invoke-WebRequest / QNetworkAccessManager |
| `tools/7za.exe` | 内嵌资源 | Git `.7z.exe` 自解压包解压（`x <archive> -o<dir> -y`） |
| GitHub Releases API | 运行期网络 | 获取 git-for-windows 最新 MinGit/PortableGit 64-bit 下载地址 |

**部署形态**：`NeoInstaller.exe`（WIN32 子系统）+ 内嵌全部部署文件；静态 Qt 模式单文件分发（约 48MB，需 `H:/Qt-static/6.11.1/msvc2022_64`，本机 2026-08-16 尚未安装）；动态模式亦可用（`IS_STATIC_QT=OFF` 时走 windeployqt）。

## 文件组成

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | `qt_add_executable(NeoInstaller WIN32 ...)`；生成 installer.qrc（内嵌 deploy/LICENSE/7za.exe）；`IS_STATIC_QT` 分支：MSVC `qt_import_plugins(QWindowsIntegrationPlugin)` / MinGW 静态链接；动态模式 windeployqt |
| `src/installer_main.cpp` | 入口：命令行解析（help/silent/path/with-editor/use-system-git/use-bundled-git），GUI 模式装配 `InstallerWizard`，静默模式装配 `SilentInstaller` |
| `src/installer_wizard.h` / `.cpp` | `InstallerWizard`（QWizard ModernStyle，8 页）与各页面类；进度页执行释放/Git 下载安装/快捷方式/install.conf |
| `src/git_checker.h` / `.cpp` | `GitChecker`：PATH/常见路径探测 git、`--version` 验证、版本有效性判定（≥2.30） |
| `src/git_downloader.h` / `.cpp` | `GitDownloader`（QObject）：GitHub API 取最新 URL（含 403 限流处理）、下载、解压；`GitVariant{MinGit, PortableGit}` |
| `src/silent_installer.h` / `.cpp` | `SilentInstaller`：无 GUI 安装流程；`GitMode{Auto, UseSystem, UseBundled}`；InstallProgress 结构 |
| `src/app.rc` | 版本资源（配合根 CMake `nsum_add_version_info` 生成的 version_NeoInstaller.rc） |
| `tools/7za.exe`、`7-zip.chm`、`license.txt`、`readme.txt` | 内嵌 7-Zip 命令行工具及其说明（仅 7za.exe 进 qrc） |

## 构建集成

- **CMake target**：`NeoInstaller`（`qt_add_executable ... WIN32`，AUTOMOC/AUTORCC ON）。
- **独立构建方式**：根 CMake `if(INSTALLER_ONLY_BUILD) add_subdirectory(modules/NeoInstaller)` —— 主配置（`msvc` 预设）**不编译** NeoInstaller。使用 `installer-static` 预设：
  ```powershell
  & $cmake --preset installer-static
  & $cmake --build build_installer
  ```
  预设关键变量：`INSTALLER_ONLY_BUILD=ON`、`VCPKG_TARGET_TRIPLET=x64-windows-static`（→ `IS_STATIC_QT=ON`）、`CMAKE_PREFIX_PATH=H:/Qt-static/6.11.1/msvc2022_64`（静态 Qt，**本机尚未安装**）、`DEPLOY_SOURCE=${sourceDir}/build/deploy`、`CMAKE_BUILD_TYPE=Release`。
- **DEPLOY_SOURCE**：未显式传入时默认 `${CMAKE_SOURCE_DIR}/build/deploy`；目录不存在 → CMake 配置期 `FATAL_ERROR`（提示先按 `msvc` 预设构建主项目）。内嵌时跳过 `*.dmp` 与 `config/custom/*`。
- **版本资源**：根 CMake 在 INSTALLER_ONLY_BUILD 分支调用 `nsum_add_version_info(NeoInstaller "NSUM 构建工具安装程序" "NSUM构建工具")`。
- **静态特殊处理**：MSVC 下 `qt_import_plugins(NeoInstaller INCLUDE Qt::QWindowsIntegrationPlugin)`（否则静态 Qt 无平台插件无法启动）；MinGW 下追加 `-static -static-libgcc -static-libstdc++` 等。动态模式（非 static）走 windeployqt。
- **部署产物**：`build_installer/.../NeoInstaller.exe`（单文件，内嵌全部部署内容），不经 `neo_deploy`。

## 命名空间与公共符号

| 符号 | 位置 | 说明 |
|------|------|------|
| `main(int, char**)` | installer_main.cpp（全局） | 入口；参数解析与 GUI/静默分发，未知参数 exit 1 |
| `NeoInstaller::InstallerWizard` (+ `PageId` 枚举) | installer_wizard.h | 向导；`setInstallEditor(bool)` / `installEditor()` / `gitVariant()` |
| `NeoInstaller::WelcomePage / LicensePage / InstallPathPage / ComponentPage / GitChoicePage / GitLicensePage / ProgressPage / FinishPage` | installer_wizard.h | 8 个页面类 |
| `NeoInstaller::GitChecker` | git_checker.h | 系统 Git 检测/版本校验 |
| `NeoInstaller::GitDownloader` (+ `enum class GitVariant{MinGit, PortableGit}`) | git_downloader.h | 下载/解压 Git，五个信号 |
| `NeoInstaller::SilentInstaller` (+ `enum GitMode{Auto, UseSystem, UseBundled}`) | silent_installer.h | 静默安装；`struct InstallProgress` |