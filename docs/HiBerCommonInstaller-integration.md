# NeoServerUpdateModpack × HiBerCommonInstaller 集成指南

> 本文档从 HiBerCommonInstaller 仓库迁移而来（该仓库保持产品无关，不收入接入方细节）。
> 配套的通用文档（产品配置 / 流程脚本 / 拓展 / 三壳 / 构建）见
> [HiBerCommonInstaller 仓库 docs/](https://github.com/HiBer2007/HiBerCommonInstaller/blob/main/docs/README.md)。

本文说明主仓库把安装器从旧 `modules/NeoInstaller` 整体替换为 HiBerCommonInstaller（submodule）的完整细节：布局、构建编排、产品配置语义、专属拓展、修改与验证方法。

## 布局

```
NeoServerUpdateModpack/
├── .gitmodules                        # 八条目：+ modules/HiBerCommonInstaller
├── CMakeLists.txt                     # INSTALLER_ONLY_BUILD 分支（见下）
├── CMakePresets.json                  # installer-static 预设
├── vcpkg.json                         # + lua + cpr
├── nsum_installer/                    # NSUM 安装器资产（产品配置 + 静态拓展 + README）
│   ├── product.json                   # NeoServer Tools 产品定义
│   ├── install.json / uninstall.json  # 流程脚本
│   ├── LICENSE.txt                    # 内嵌许可（复制自主仓库根 LICENSE）
│   └── ext/nsum_args_ext.cpp          # NSUM 专属参数/步骤拓展（HCI_REGISTER_EXTENSION 静态链接）
├── modules/HiBerCommonInstaller/      # submodule（仓库本体；嵌套 HiBerGUILibCPP）
```

克隆须知：`git clone --recursive`（三层：主仓库 → HCI → HiBerGUILibCPP）。

## INSTALLER_ONLY 分支（根 CMakeLists）

仅 `installer-static` 预设触发；构建内容 = hci_gui（静态 Qt）+ nsum_args_ext：

1. `find_package(nlohmann_json / Lua / ZLIB / libzippp / cpr)`（HCI 核心依赖；cpr 曾缺失于清单——NeoBuild 也用它）
2. `set(HCI_BUILD_GUI ON)`；CLI/TUI/examples OFF
3. `HCI_PRODUCT_FILES` = `deploy/<rel>=<abs>` 全量（跳过 `*.dmp` 与 `config/custom/*`，沿旧规则）+ `product.json/install.json/uninstall.json/LICENSE.txt`（来自 `nsum_installer/`）
4. `add_subdirectory(modules/HiBerCommonInstaller)` → `target_sources(hci_gui PRIVATE nsum_installer/ext/nsum_args_ext.cpp)`（静态链接并入，无 DLL/extensions）

## installer-static 预设要点

- `INSTALLER_ONLY_BUILD=ON`、`HCI_QT_STATIC=ON`、`CMAKE_PREFIX_PATH=H:/Qt-static/6.11.1/msvc2022_64`、`DEPLOY_SOURCE=${sourceDir}/build/deploy`
- **全静态链接**：`VCPKG_TARGET_TRIPLET=x64-windows-static` + 静态 Qt（`HCI_QT_STATIC=ON`）——安装器自身**无任何运行时 DLL 依赖**（含 Lua 解析器等；系统 DLL 除外），单文件独立分发（静态插件 nsum_args_ext 亦编译进 EXE）
- 工作区迁移后删 `build_nsum_installer/` 重配（旧 CMakeCache 绑定旧机器路径会报错）

## NSUM 产品配置语义（nsum_installer/）

**install.json 流程**（对应旧 8 页行为）：

| 旧 NeoInstaller | 新流程步骤 |
|---|---|
| 欢迎页 | `welcome` |
| 许可页（GPL v3 文本） | `license`（`qrc:/LICENSE.txt`） |
| 路径页 + 非空清空警告 | `path` + `clean`（`{installDir}`） |
| 组件页（核心必装 + 编辑器可选） | `components`（product.components 仅 editor） |
| Git 选择（系统/内置 + MinGit/PortableGit） | `nsum_git_plan`（拓展步骤）+ `download`（asset=git-for-windows/git，variant=`{gitVariant}`）+ `extract` 到 `{installDir}/tools/git` |
| 进度页（释放 deploy） | `extract`（`qrc:/deploy` → `{installDir}`，skip `*.dmp`/`config/custom/*`） |
| install.conf 四键 | `template`（install_path/install_editor/use_system_git/git_path） |
| 桌面/开始菜单快捷方式 | `shortcut` ×2（编辑器勾选时 + editor 快捷方式） |
| 完成页（立即运行） | `finish`（launch=NeoServerUpdateModpack.exe） |

**install.conf 兼容**：`git_path` 固定模板 `{installDir}/tools/git/bin/git.exe`；主程序 `InstallConfig::load()` 跨布局探测（tools/git 下 MinGit 目录结构）——与旧行为一致。

## nsum_args_ext（nsum_installer/ext/，静态链接）

- **参数处理器**：`--with-editor`（预选 editor 组件）、`--use-system-git`（gitMode=system）、`--use-bundled-git`（gitMode=bundled）——核心不感知，全部走拓展模型
- **自定义步骤** `nsum_git_plan`：探测 PATH/ProgramFiles/ProgramFiles(x86)/LocalAppData 常见路径的 git（`--version` 验证）；auto=有系统 git 用之否则下载；system 模式找不到 → 回退下载（沿旧语义）；写出 `gitUseSystem/gitDownload/gitVariant/gitPlanned` 变量
- **加载方式**：`HCI_REGISTER_EXTENSION(NsumArgsExtension)` **静态注册**直接编译进 hci_gui（宿主 `ExtensionLoader::loadStatic()` 装载）——不再部署 DLL，安装器无 `extensions/` 目录

## 修改须知

- **HCI 代码/配置改动**：在 HCI 仓库工作树提交 push → 主仓库 `git submodule update --remote modules/HiBerCommonInstaller` → 重新构建（主仓库构建用 submodule checkout，**不是** HCI 工作树）
- **nsum_installer/*.json 改动**：必须重新 configure（内容 configure 期固化进 qrc）
- **快捷方式冒烟**会真实写桌面/开始菜单——验证后清理 `NeoServer.lnk`

## 验证方法（实证步骤）

```powershell
# 1) 构建
cmd /c "vcvars64.bat && cmake --preset installer-static && cmake --build build_nsum_installer"
# 2) 静默安装（--with-editor 走拓展链路；本机有系统 git 时跳过下载）
& build_nsum_installer/modules/HiBerCommonInstaller/gui/NSUM_Installer_1.0.0.exe --product qrc:/product.json `
  --flow install --path <out> --silent --with-editor
# 期望：exit 0、deploy 文件释放、install.conf 四键、快捷方式创建
# 3) 卸载
... --flow uninstall --silent --path <out>
# 4) GUI 窗口冒烟：启动 4s 存活 + 有窗口 → CloseMainWindow exit 0/1 无崩溃
```

## 已知限制

- 本机 GitHub API 直连 SSL error 35 → `gitDownload` 步骤本机不可用（系统 git 探测命中时跳过）；正常网络环境可下载
- 功能实测归用户（向导交互、安装后主程序运行）