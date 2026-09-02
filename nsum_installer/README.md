# nsum_installer — NSUM 安装器资产

NSUM 安装器 = 单文件 `NSUM_Installer_<版本>.exe`（版本定义于根 CMakeLists `NSUM_INSTALLER_VERSION`；HiBerCommonInstaller GUI 壳，**静态 Qt + 静态 vcpkg triplet（x64-windows-static + /MT）：无任何运行时 DLL**，qrc 内嵌以下资产）。本目录是**主仓库侧全部安装器专属内容**（框架本身保持产品无关，见 `modules/HiBerCommonInstaller/` 与 HCI 仓库 docs/）。

## 目录结构

```
nsum_installer/
├── README.md                   # 本说明
├── product.json                # 产品配置（名称 NSUM / welcomeTitle / 组件+描述 / %APPDATA% 默认路径 /
│                               #   extensions 配置段 / 四流程引用 / payload 规则；version 经 configure_file 注入）
├── install.json                # 安装流程（语言 → 欢迎 → 许可 → 路径+写权限 → 组件 → Git 选择页 →
│                               #   git_plan → 下载链/内置解压 或 系统安装(提权+静默+refresh) →
│                               #   释放 → install.conf → 快捷方式 → 完成多启动）
├── uninstall.json              # 卸载流程（确认 → 注册表 Software/HiBer2007/NSUM → 清空）
├── repair.json                 # 修复流程（confirm → 重释放 deploy → 刷新配置/快捷方式 → 完成）
├── upgrade.json                # 升级流程（同修复语义，完成页「已升级到最新版本」）
├── LICENSE.txt                 # 内嵌许可（复制自主仓库根 LICENSE）
├── release_hci_gui.cmake       # 便捷释放脚本（cmake -P：exe 复制到构建目录根 build_nsum_installer/）
├── ext/
│   └── nsum_args_ext.cpp       # NSUM 专属参数拓展 --with-editor（Git 相关在通用 hci_git）
└── tests/
    └── install_no_shortcuts.json  # 测试用流程：与 install.json 相同但快捷方式全部 enabled:false
```

## 静态并入的通用拓展（来自 HCI 仓库，target_sources 编译进 hci_gui）

| 拓展 | 提供 | 来源 |
|---|---|---|
| `hci.git` | `--use-system-git/--use-bundled-git/--install-system-git` + 步骤 `git_plan`/`git_refresh`；内置 Git 位置 = `extensions.hci.git.gitDir`（默认 tools/git） | `modules/HiBerCommonInstaller/ext/git/` |
| `hci.winget` | 下载后端 `winget`（包安装，可入下载链） | `modules/HiBerCommonInstaller/ext/winget/` |

## 构建编排（根 CMakeLists INSTALLER_ONLY 分支）

1. 版本单点 `NSUM_INSTALLER_VERSION` → `HCI_VERSION`/`HCI_GUI_OUTPUT_NAME`（产物名 `NSUM_Installer_<v>.exe`）
2. `configure_file(product.json @ONLY)`（版本注入）→ `HCI_PRODUCT_FILES` = `deploy/<rel>=<abs>` 全量（跳过 `*.dmp`/`config/custom/*`）+ `product.json / install.json / uninstall.json / repair.json / upgrade.json / LICENSE.txt`
3. `add_subdirectory(modules/HiBerCommonInstaller)`（HCI_BUILD_GUI=ON + HCI_QT_STATIC=ON）
4. `target_sources(hci_gui PRIVATE nsum_installer/ext/nsum_args_ext.cpp + HCI ext/git + ext/winget 源码)` —— 全部 `HCI_REGISTER_EXTENSION` **静态注册**编译进安装器，无 extensions/ 目录、无 DLL
5. 便捷释放目标 `hci_gui_release_to_root`：exe（含运行时 DLL 过滤中间产物）→ `build_nsum_installer/` 根

## 修改须知

- **product.json 改动**：经 configure_file 生成 → **必须重新 configure**（`cmake --preset installer-static`）
- **其余内嵌 json（install/uninstall/repair/upgrade/LICENSE）改动**：qrc 以**内容哈希命名**（`hci_product_<hash>.qrc`），文件编辑后 rcc 自动重编——正常 rebuild 即可（无需重 configure）
- **ext 改动**：直接改 `nsum_installer/ext/nsum_args_ext.cpp`（或 HCI 的 ext/git、ext/winget 源码）后 rebuild（hci_gui 依赖这些源文件；HCI 源码需先提交并 `submodule update --remote`）
- **deploy 内容**：随 `neo_deploy`（msvc 预设）产出；installer-static 构建时自动嵌入
- **测试**：用 `tests/install_no_shortcuts.json` + `--path test_output/nsum_smoke/...`（不创建任何快捷方式）

## 使用文档

面向最终用户/脚本的完整使用文档见 [docs/NSUM-Installer.md](../docs/NSUM-Installer.md)（GUI 流程 / 静默 / CLI 参数全表 / 退出码 / 生命周期 / 测试 / 场景）。