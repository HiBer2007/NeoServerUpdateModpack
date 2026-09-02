# NSUM 安装器使用文档（NeoServer Installer）

NSUM 安装器 = 单文件可执行 **`NSUM_Installer_<version>.exe`**（当前 1.0.0；版本号定义于根 CMakeLists `NSUM_INSTALLER_VERSION`，产物名/版本资源/`--version`/内嵌 product.json 版本全部依赖它）。NSUM = **N**eo（新）**S**erver **U**pdate **M**odpack——新服务器自动更新整合包；`Neo` 是前缀、SUM 一体。壳体 = HiBerCommonInstaller 的 GUI 壳，**静态 Qt + 全部依赖静态链接（`x64-windows-static` + `/MT` + `-static-runtime -no-icu`），无任何运行时 DLL**（dumpbin 依赖 = 纯 Windows 系统 DLL）。产品配置、安装/卸载/修复/升级流程、许可、全部部署文件均**内嵌于 EXE**，任意工作目录/双击均可独立运行。本文覆盖全部运行模式与命令行参数。

## 1. 运行模式总览

| 模式 | 触发方式 | 说明 |
|---|---|---|
| **GUI 向导** | 双击 EXE，或命令行不带模式参数 | 语言选择 → 欢迎 → 许可 → 路径 → 组件 → Git → 进度 → 完成（含修复/升级流程） |
| **静默安装** | `--silent`（可加 `--path`/`--lang`/`--with-editor`/Git 参数） | 无窗口无提示，全部交互取默认值，供脚本/CI/批量部署 |
| **静默卸载/修复/升级** | `--silent --flow uninstall\|repair\|upgrade` | 确认步骤自动确认；卸载清空目标目录并删注册表项 |
| 终端辅助 | 从命令行启动（非 --silent） | 保留终端输出日志（ANSI 分级颜色）；双击启动无终端时自动释放控制台 |

> 分发物只有 `NSUM_Installer_<version>.exe`（qrc 内嵌需要 Qt，故 CLI 壳 hci_cli 不在分发内）。需要 `=====JSON-BEGIN=====` 标记块协议或纯文本 CLI 网关时，另行构建 HCI 的 hci_cli（见 HiBerCommonInstaller 仓库 docs/shell-cli.md）。

## 2. 命令行参数（完整）

### 核心参数

| 参数 | 取值 | 说明 |
|---|---|---|
| `--gui` | — | 显式 GUI 模式（默认行为） |
| `--silent`, `-s` | — | 无头执行：不建窗口，交互全取默认值；错误输出到 stderr |
| `--path <dir>` | 目录 | 安装路径覆盖（GUI 预填路径页；静默直接使用） |
| `--lang <en\|zh>` | 语言码 | 直接选定语言（跳过语言选择窗；缺省取流程 default / product `defaultLanguage`） |
| `--product <file.json>` | 路径或 `qrc:/product.json` | 产品配置来源（默认内嵌 `qrc:/product.json`；外部配置用于调试） |
| `--flow <install\|uninstall\|repair\|upgrade\|file.json>` | 流程名/路径 | 默认 `install`；`repair`/`upgrade` 走修复/升级流程；外部 json 用于测试/调试 |
| `--help`, `-h` | — | 打印用法，exit 0 |
| `--version`, `-v` | — | 打印版本（显示 `Neo Server Update Modpack v<版本>` 品牌行），exit 0 |

### 产品专属参数（由静态拓展提供：nsum_args_ext `--with-editor`；hci_git 三个 Git 参数）

| 参数 | 提供方 | 说明 |
|---|---|---|
| `--with-editor` | nsum_args_ext | 预选「NeoWorkspaceEditor」组件（安装编辑器 + 其快捷方式；内置 Git 变体改选 PortableGit） |
| `--use-system-git` | hci_git | 强制用系统 Git（未找到时回退下载内置，不视为失败） |
| `--use-bundled-git` | hci_git | 强制下载内置 MinGit/PortableGit，忽略系统 Git |
| `--install-system-git` | hci_git | 下载 Git for Windows 官方安装器并静默安装到系统（需管理员） |

### 退出码

| 码 | 含义 |
|---|---|
| 0 | 成功 |
| 1 | 失败 / 用户取消（GUI 关窗取消亦为 1） |
| 2 | 用法错误（未知参数、产品/流程缺失） |

## 3. GUI 向导流程（当前事实）

页面顺序与行为（全部页面文本支持 en/zh，语言在欢迎页**之前**的小窗口选择；欢迎页期间主窗口不显示）：

1. **语言选择**（欢迎页前小窗）：English / 简体中文；`defaultLanguage`（NSUM=zh）为流程缺省，`--lang` 直接跳过选择
2. **欢迎**：左上大字 **Neo Server Update Modpack**（`welcomeTitle`，微缩 22pt）→ 中等副标「安装程序」→ 加大说明文本 → 右下角灰色小字 Powered by HiBer Common Installer Module；窗口尺寸随页面内容动态计算并**动画调节**（geometry 200ms OutCubic）
3. **许可**：内嵌许可文本，勾选接受（未勾选不能继续；按钮为「下一步」）
4. **安装路径**：默认 **`%APPDATA%\NSUM`**（`%VAR%` 环境变量占位自动展开）；目录非空 → 警告"将清空"；**自动检查写权限**——无权限 → 询问并以管理员身份重启继续（elevation 机制）
5. **组件**：核心「NSUM 核心」必选（在列表中且不可取消卸载）+ 可选「NeoWorkspaceEditor」，每项带灰色描述文本
6. **Git 选择页**：自动检测系统 Git 并显示结果；三选项——使用系统 Git（检测到时可用）/ 使用内置 Git / **下载并安装系统 Git（管理员，仅系统 Git 缺失时显示）**
7. **下载/解压 Git**（按选择）：内置 → 下载链（github 等）→ 解压到 `{gitInstallDir}`（`extensions.hci.git.gitDir`=tools/git）；系统安装 → 下载官方安装器 → 提权 → `/VERYSILENT /NORESTART` 静默安装 → 重探测系统 Git（`git_refresh`）
8. **进度页**：步骤小字 + 百分比在右，进度条恒定于标题分割线之下，下方日志区（Debug 级**极其详细**，含每步骤 start/done、下载链初始化与各后端日志）
9. **快捷方式**：桌面 `NSUM.lnk` + 开始菜单 `NSUM/NSUM.lnk`（编辑器勾选时加桌面 `NeoWorkspaceEditor.lnk`）；**流程可控制**——快捷方式步骤 `enabled:false` 即不创建（测试流程见 §6）
10. **完成**：成功/失败大标题；**多启动选项**（勾选要启动的：Neo Server Update Modpack / NeoWorkspaceEditor，detach 启动不阻塞安装器退出）

### 生命周期

- `--flow install`：全新安装（欢迎 → 许可 → 路径 → 组件 → Git → 释放 → 配置 → 快捷方式 → 完成）
- `--flow repair`：修复——重新释放部署文件 + 刷新 install.conf/快捷方式（保留配置数据；确认页 + 完成页，无 Git 决策）
- `--flow upgrade`：升级——同修复模式更新到最新部署内容，完成页「已升级到最新版本」
- `--flow uninstall`：卸载——确认（静默自动确认）→ 删除注册表卸载项（`HKCU\Software\HiBer2007\NSUM`）→ 清空安装目录内容（目录本身保留）

## 4. 静默模式（--silent，脚本化）

```powershell
# 最小静默安装（默认路径 %APPDATA%\NSUM）
.\NSUM_Installer_1.0.0.exe --silent

# 指定目录 + 编辑器 + 强制系统 Git
.\NSUM_Installer_1.0.0.exe --silent --path "D:\NSUM" --with-editor --use-system-git

# 强制内置 Git（正常网络自动下载并解压到 {installDir}/tools/git）
.\NSUM_Installer_1.0.0.exe --silent --path "D:\NSUM" --use-bundled-git

# 静默卸载 / 修复 / 升级
.\NSUM_Installer_1.0.0.exe --silent --flow uninstall --path "D:\NSUM"
.\NSUM_Installer_1.0.0.exe --silent --flow repair   --path "D:\NSUM"
.\NSUM_Installer_1.0.0.exe --silent --flow upgrade  --path "D:\NSUM"
```

**静默默认值**：

| 交互步骤 | 静默取值 |
|---|---|
| 语言 | `--lang`；无 → 流程 default（NSUM=zh） |
| 许可 | 视为接受 |
| 路径 | `--path` 的值；无 → `%APPDATA%\NSUM`（展开） |
| 组件 | 默认不勾选编辑器（`--with-editor` 除外；核心必装恒选） |
| Git | auto：检测到系统 Git 用之；否则下载内置（三个 Git 参数强制对应模式） |
| 完成 | 不启动任何程序（即使 finish 定义了 launch/launchOptions） |

**CreateProcessA / 无终端调用**：带参数运行即进入无窗口无头执行；无控制台时日志落到 `%TEMP%/hci_gui.log`，错误同步 stderr。
**自动化钩子**：环境变量 `HCI_GUI_AUTOPILOT=1`（非空）等价静默语义，供冒烟测试。

## 5. 提权（管理员）

- **启动申请**：product `elevation.request: true` 时流程开始前申请并以管理员重启（UAC）
- **中途申请**：`{"ui":"elevate"}` 步骤（如系统安装 Git 时），提权后重启继续；"elevate" 步骤在已提权进程内直接通过
- **写权限自动检测**：路径步骤后探测目标目录可写性，无权限 → 自动申请提权重启
- GUI 弹「需要管理员权限」确认框；静默（无头）模式自动发起 UAC；拒绝则继续以当前权限运行（各模式通用）

## 6. 测试流程（不创建任何快捷方式）

- **测试目录**：`<仓库>/test_output/nsum_smoke`（git 忽略）
- **测试流程**：`nsum_installer/tests/install_no_shortcuts.json` —— 与 install.json 相同但三个 shortcut 步骤均 `"enabled": false`（不产生任何快捷方式）
- **测试安装命令**：

```powershell
.\NSUM_Installer_1.0.0.exe --flow K:\NeoServerUpdateModpack\nsum_installer\tests\install_no_shortcuts.json `
  --path K:\NeoServerUpdateModpack\test_output\nsum_smoke\install --silent --use-system-git
```

- 卸载同测试目录：`--flow uninstall --path ... --silent`；验证后清理 `test_output/nsum_smoke`

## 7. 产物与副作用

| 项 | 位置/内容 |
|---|---|
| 释放文件 | 目标目录整体 = 构建产物 `build/deploy` 全量（跳过 `*.dmp` 与 `config/custom/*`） |
| `install.conf` | `<安装目录>/install.conf`，四键：`install_path` / `install_editor` / `use_system_git` / `git_path`；`git_path` 按 Git 决策动态写入（`git_plan` 输出 `{gitPath}`：系统 Git 时 = `git`，内置时 = `tools/git/bin/git.exe` 对应路径） |
| Git | 系统 Git（存在时）；否则 `<安装目录>/tools/git/`（MinGit/PortableGit，zip 解压）；系统安装模式 = 静默安装 Git for Windows |
| 快捷方式 | 桌面 `NSUM.lnk` + 开始菜单 `NSUM/NSUM.lnk`；`--with-editor` 加桌面 `NeoWorkspaceEditor.lnk`（流程可禁用） |
| 立即运行 | 完成页多选启动（主程序/编辑器勾选，detach）；静默模式不启动 |

## 8. 常见场景

```powershell
# 1) 图形安装（默认 %APPDATA%\NSUM）
.\NSUM_Installer_1.0.0.exe

# 2) 图形安装 + 编辑器 + 内置 Git（PortableGit）
.\NSUM_Installer_1.0.0.exe --with-editor --use-bundled-git

# 3) 静默安装到指定目录（CI/一键脚本）
.\NSUM_Installer_1.0.0.exe --silent --path "D:\NSUM" --with-editor
echo $LASTEXITCODE        # 0 = 成功

# 4) 修复 / 升级
.\NSUM_Installer_1.0.0.exe --flow repair --path "D:\NSUM"
.\NSUM_Installer_1.0.0.exe --silent --flow upgrade --path "D:\NSUM"

# 5) 外部产品/流程调试（不修改内嵌资产时）
.\NSUM_Installer_1.0.0.exe --product .\nsum_installer\product.json --flow .\nsum_installer\install.json --path "D:\NSUM"
```

## 9. 已知限制

- **GitHub Releases 下载**：获取 Git 下载地址依赖 `api.github.com`（正常网络可用）。受限网络（本机直连 SSL error 35 等环境）下无法下载——建议 `--use-system-git` 或预装系统 Git；winget 已作为可选下载后端链入（`"backend": "winget"`），可作包管理替代路径
- **目标目录会被清空**（确认目录语义后再指路径；GUI 有警告弹窗，静默模式无）
- **系统安装 Git 需管理员**（UAC；拒绝则回退继续以当前权限运行）
- **功能实测归用户**：向导交互、动效、颜色渲染与安装后主程序行为由用户验收（冒烟仅验证无崩溃）

## 10. 相关文档

- [HiBerCommonInstaller 集成指南](HiBerCommonInstaller-integration.md) — 构建编排/资产修改/验证方法
- [nsum_installer README](../nsum_installer/README.md) — 资产目录结构与修改须知
- HiBerCommonInstaller 仓库 [docs/](https://github.com/HiBer2007/HiBerCommonInstaller/blob/main/docs/README.md) — 框架通用文档（product-config / flow-script / shells / pitfalls）