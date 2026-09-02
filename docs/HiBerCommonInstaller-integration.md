# NeoServerUpdateModpack × HiBerCommonInstaller 集成指南

> 本文档从 HiBerCommonInstaller 仓库迁移而来（该仓库保持产品无关，不收入接入方细节）。
> 配套的通用文档（产品配置 / 流程脚本 / 拓展 / 三壳 / 构建 / 踩坑）见
> [HiBerCommonInstaller 仓库 docs/](https://github.com/HiBer2007/HiBerCommonInstaller/blob/main/docs/README.md)。

本文说明主仓库把安装器从旧 `modules/NeoInstaller` 整体替换为 HiBerCommonInstaller（submodule）的完整细节：布局、构建编排、产品配置语义、专属拓展、修改与验证方法。

## 布局

```
NeoServerUpdateModpack/
├── .gitmodules                        # 八条目：+ modules/HiBerCommonInstaller
├── CMakeLists.txt                     # INSTALLER_ONLY_BUILD 分支（见下）
├── CMakePresets.json                  # installer-static 预设
├── vcpkg.json                         # + lua + cpr
├── nsum_installer/                    # NSUM 安装器资产（产品配置 + 静态拓展 + 测试流程 + README）
│   ├── product.json                   # NSUM 产品定义（version 经 configure_file 注入）
│   ├── install.json / uninstall.json  # 安装 / 卸载流程
│   ├── repair.json / upgrade.json     # 修复 / 升级流程
│   ├── LICENSE.txt                    # 内嵌许可（复制自主仓库根 LICENSE）
│   ├── release_hci_gui.cmake          # 便捷释放脚本（exe → 构建目录根）
│   ├── ext/nsum_args_ext.cpp          # NSUM 专属参数拓展（--with-editor）
│   └── tests/install_no_shortcuts.json# 测试用流程（快捷方式 enabled:false）
├── modules/HiBerCommonInstaller/      # submodule（仓库本体；嵌套 HiBerGUILibCPP；
│                                     #   附带通用拓展 ext/git + ext/winget + ext/apt）
```

克隆须知：`git clone --recursive`（三层：主仓库 → HCI → HiBerGUILibCPP）。

## INSTALLER_ONLY 分支（根 CMakeLists）

仅 `installer-static` 预设触发；构建内容 = hci_gui（静态 Qt）+ 三个静态注册拓展：

1. `find_package(nlohmann_json / Lua / ZLIB / libzippp / cpr)`（HCI 核心依赖；cpr 曾缺失于清单——NeoBuild 也用它）
2. **版本单点**：`set(NSUM_INSTALLER_VERSION "1.0.0")` → `HCI_VERSION` / `HCI_GUI_OUTPUT_NAME`（产物名 `NSUM_Installer_<v>.exe`；版本资源、`--version`、内嵌 product.json 版本全部依赖它）
3. `set(HCI_BUILD_GUI ON)`；CLI/TUI/examples OFF
4. `configure_file(product.json, @ONLY)`（`@NSUM_INSTALLER_VERSION@` → 版本）后，`HCI_PRODUCT_FILES` = `deploy/<rel>=<abs>` 全量（跳过 `*.dmp` 与 `config/custom/*`）+ `product.json / install.json / uninstall.json / repair.json / upgrade.json / LICENSE.txt`
5. `add_subdirectory(modules/HiBerCommonInstaller)` → **`target_sources(hci_gui PRIVATE …)` 静态并入三拓展**：`nsum_installer/ext/nsum_args_ext.cpp`（`--with-editor`）+ HCI `ext/git/src/git_ext.cpp`（`hci.git`：Git 策略/系统安装）+ HCI `ext/winget/src/winget_ext.cpp`（`hci.winget` 下载后端）——`HCI_REGISTER_EXTENSION` 静态注册，无 DLL/extensions 目录
6. 便捷释放：`hci_gui_release_to_root` 目标把 exe 复制到构建目录根 `build_nsum_installer/`

## installer-static 预设要点

- `INSTALLER_ONLY_BUILD=ON`、`HCI_QT_STATIC=ON`、`VCPKG_TARGET_TRIPLET=x64-windows-static`、`CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`（/MT 对齐）、**`CMAKE_PREFIX_PATH=H:/Qt-static-rt/6.11.1/msvc2022_64`**（静态 Qt 源码构建 `-static -release -static-runtime -no-icu -schannel`——无 ICU/VC runtime DLL）、`DEPLOY_SOURCE=${sourceDir}/build/deploy`
- **全静态**：安装器自身**无任何运行时 DLL**——dumpbin 依赖 = 纯 Windows 系统 DLL（KERNEL32/USER32/d3d11/WINHTTP…；无 cpr/lua/libzippp/curl/bz2/z、无 MSVCP/VCRUNTIME、ICU 不加载）
- 工作区迁移后删 `build_nsum_installer/` 重配（旧 CMakeCache 绑定旧机器路径会报错）

## NSUM 产品配置语义（nsum_installer/）

**product.json 关键字段**：`productName=NSUM`（Neo=新 服务器自动更新整合包，前缀语义）；`welcomeTitle=Neo Server Update Modpack`（欢迎页大字）；`defaultLanguage=zh`；`backEnabled=true`（「上一步」）；`defaultInstallPath=%APPDATA%\NSUM`（`%VAR%` 环境变量占位自动展开）；`banner.font=neo`（主程序 CLI 同款拼接字）；组件 = `core`（必选「NSUM 核心」+ 描述）+ `editor`（NeoWorkspaceEditor + 描述）；`extensions.hci.git.gitDir=tools/git`（拓展功能配置段示例）；flows 四流程。

**install.json 流程**（GUI 当前事实）：

| 阶段 | 步骤 |
|---|---|
| 语言选择（欢迎页前小窗） | `language`（default zh；`--lang` 直选） |
| 欢迎页（大字 + 安装程序副标 + 右下 Powered） | `welcome` |
| 许可（按钮「下一步」） | `license`（`qrc:/LICENSE.txt`） |
| 安装路径（默认 `%APPDATA%\NSUM`；**写权限自动检测**，无权限 → 提权重启） | `path` |
| 组件（核心必选 + 编辑器 + 描述） | `components` |
| Git 选择页（自动检测；系统/内置/下载安装系统 Git(admin)） | `git`（`installSystemOption: true`） |
| Git 决策 | `git_plan`（通用 hci_git；`editorComponent`/`editorVariant`；输出 `gitUseSystem/gitDownload/gitInstallKind/gitVariant/gitInstallDir/gitPath`） |
| 内置 Git：下载链 + 解压 | `download`（asset git-for-windows/git）→ `extract` → `{gitInstallDir}` |
| 系统安装 Git：下载安装器 + 提权 + 静默安装 + 重探测 | `download`（`ext:"exe"`）→ `elevate` → `run /VERYSILENT /NORESTART` → `git_refresh` |
| 清空目标 + 释放 deploy | `clean` → `extract`（`qrc:/deploy`，skip `*.dmp`/`config/custom/*`） |
| install.conf 四键 | `template`（`git_path` 用动态 `{gitPath}`：系统 Git→`git`，内置→tools 路径） |
| 快捷方式（流程可禁用 `enabled:false`） | `shortcut` ×2 + 编辑器快捷方式（勾选时） |
| 完成（**多启动选项勾选**：主程序/编辑器，detach 启动） | `finish`（`launchOptions`） |

**install.conf 兼容**：四键 `install_path/install_editor/use_system_git/git_path`；`git_path` 随 Git 决策动态写入；主程序 `InstallConfig::load()` 探测链：system 探测（15s 超时）→ conf git_path → `tools/git` 便携布局，失败信息品牌化为 NSUM 安装器。

**生命周期**：`--flow install | uninstall（注册表清理 + 清空）| repair（重释放部署 + 刷新配置/快捷方式）| upgrade（更新到最新部署内容）`。

## 拓展加载与功能配置（当前事实）

- `nsum_args_ext`：仅 `--with-editor`（Git 相关已全部移入通用 hci_git）
- `hci_git`：`--use-system-git/--use-bundled-git/--install-system-git` + 步骤 `git_plan`/`git_refresh`；内置 Git 相对位置 = 步骤参数 `gitDir` > 产品元数据 `extensions.hci.git.gitDir` > 默认 `tools/git`
- `hci.winget`：下载后端 `winget`（`winget install --exact --id <pkg> --silent --accept-*`），可作流程链成员（如 `"chain": ["winget", "github"]`）
- 后端体系（同时内置 `direct`/`github`/`powershell`/`curl`，可经 `ExtensionRegistry::registerDownloadBackend` 扩展链条）

## 修改须知

- **HCI 代码/配置改动**：在 HCI 仓库工作树提交 push → 主仓库 `git submodule update --remote modules/HiBerCommonInstaller` → 重新构建（主仓库构建用 submodule checkout，**不是** HCI 工作树）
- **nsum_installer/*.json 改动**：`product.json` 经 configure_file → **必须重新 configure**；其余内嵌文件（install/uninstall/repair/upgrade/LICENSE）由 **qrc 内容哈希命名**（`hci_product_<hash>.qrc`）——文件编辑后 rcc 自动重编，正常 rebuild 即可
- **快捷方式冒烟**会真实写桌面/开始菜单——测试请用 `tests/install_no_shortcuts.json`（`enabled:false`）或验后清理 `NSUM.lnk`/`NeoWorkspaceEditor.lnk`

## 验证方法（实证步骤）

```powershell
# 1) 构建
cmd /c "vcvars64.bat && cmake --preset installer-static && cmake --build build_nsum_installer"
# 2) 静默安装到项目内测试目录（无快捷方式流程；本机有系统 git 时跳过下载）
& build_nsum_installer/NSUM_Installer_1.0.0.exe `
  --flow <repo>\nsum_installer\tests\install_no_shortcuts.json --path <repo>\test_output\nsum_smoke\install `
  --silent --with-editor
# 期望：exit 0、deploy 文件释放、install.conf 四键、无快捷方式
# 3) 修复/升级/卸载（同一测试目录）
... --flow repair|upgrade|uninstall --silent --path <repo>\test_output\nsum_smoke\install
# 4) GUI 窗口冒烟：启动 4s 存活 + 有窗口 → 关闭 exit 0/1 无崩溃（语言窗先于主窗口）
# 5) 依赖核验：dumpbin /dependents NSUM_Installer_1.0.0.exe → 纯系统 DLL
```

## 已知限制

- 本机 GitHub API 直连 SSL error 35 → GitHub 下载本机不可用（系统 git 探测命中时跳过；`powershell` 后端/正常网络可用）
- 功能实测归用户（向导交互、动画、颜色渲染、安装后主程序运行）