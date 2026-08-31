# NSUM 安装器使用文档（NeoServer Installer）

NSUM 安装器 = 单文件可执行 `hci_gui.exe`（HiBerCommonInstaller 的 GUI 壳，静态 Qt 构建）。产品配置、安装/卸载流程、许可、全部部署文件均**内嵌于 EXE**，分发即单文件。本文覆盖全部运行模式与命令行参数。

## 1. 运行模式总览

| 模式 | 触发方式 | 说明 |
|---|---|---|
| **GUI 向导** | 双击 EXE，或命令行不带模式参数 | 8 步交互向导：欢迎 → 许可 → 安装路径 → 组件 → Git → 进度 → 完成 |
| **静默安装** | `--silent`（可加 `--path`/`--with-editor`/Git 参数） | 无窗口无提示，全默认值执行（详见 §4 默认值），供脚本/CI/批量部署 |
| **静默卸载** | `--silent --flow uninstall` | 确认步骤同样自动确认，执行注册表清理 + 清空目标目录 |
| 终端辅助 | 从命令行启动（非 --silent） | 保留终端输出日志；双击启动无终端时自动释放控制台 |

> 注：分发物只有 `hci_gui.exe`（qrc 内嵌需要 Qt，故 CLI 壳 hci_cli 不在 NSUM 分发内）。
> 需要 `=====JSON-BEGIN=====` 标记块协议或纯文本 CLI 网关时，请构建 HCI 的 hci_cli
> （见 HiBerCommonInstaller 仓库 docs/shell-cli.md）。

## 2. 命令行参数（完整）

### 核心参数

| 参数 | 取值 | 说明 |
|---|---|---|
| `--gui` | — | 显式 GUI 模式（默认行为） |
| `--silent`, `-s` | — | 无头执行：不建窗口，交互步骤全部取默认值；错误输出到 stderr |
| `--path <dir>` | 目录 | 安装路径覆盖（GUI 模式预填路径页；静默模式直接使用） |
| `--product <file.json>` | 文件路径或 `qrc:/product.json` | 产品配置来源（默认内嵌 `qrc:/product.json`；外部配置用于调试） |
| `--flow <install\|uninstall\|file.json>` | 流程名/路径 | 默认 `install`；`uninstall` 走卸载流程；也可指定外部 json |
| `--help`, `-h` | — | 打印用法，exit 0 |
| `--version`, `-v` | — | 打印版本，exit 0 |

### 产品专属参数（由 nsum_args_ext 静态拓展提供）

| 参数 | 说明 |
|---|---|
| `--with-editor` | 预选「NeoWorkspaceEditor」组件（安装编辑器 + 其快捷方式；Git 变体改选 PortableGit） |
| `--use-system-git` | Git 强制用系统安装的 Git（未找到时回退下载内置 Git，不视为失败） |
| `--use-bundled-git` | Git 强制下载内置 MinGit/PortableGit，忽略系统 Git |

### 退出码

| 码 | 含义 |
|---|---|
| 0 | 成功 |
| 1 | 失败 / 用户取消（GUI 关窗取消亦为 1） |
| 2 | 用法错误（未知参数、产品/流程缺失） |

## 3. 模式详解

### 3.1 GUI 向导（默认）

页面顺序与行为：

1. **欢迎**：产品横幅（Powered by HiBer Common Installer Module 强制显示）
2. **许可**：内嵌许可文本，需勾选接受（未接受不能继续）
3. **安装路径**：默认 `C:\Program Files\NeoServer`；目录非空 → 明确警告"将清空"
4. **组件**：核心必装（锁定）；「NeoWorkspaceEditor」可勾选（`--with-editor` 预选）
5. **Git**：自动检测系统 Git → 有则默认使用系统；无则安装内置（MinGit；选编辑器则 PortableGit）；可选手动切换
6. **进度**：释放部署文件（跳过 `*.dmp` 与 `config/custom/*`）→ 按需下载/解压 Git → 写 `install.conf` → 创建桌面/开始菜单快捷方式
7. **完成**：成功/失败提示；成功可勾选「立即运行」

### 3.2 静默安装（--silent，重点：脚本化）

```powershell
# 最小静默安装（默认路径 C:\Program Files\NeoServer）
.\hci_gui.exe --silent

# 指定目录 + 编辑器 + 强制系统 Git（CI/批量部署）
.\hci_gui.exe --silent --path "D:\NeoServer" --with-editor --use-system-git

# 强制内置 Git（无网络外网机不适合；正常网络自动下载）
.\hci_gui.exe --silent --path "D:\NeoServer" --use-bundled-git

# 静默卸载（自动确认 + 清空目标目录）
.\hci_gui.exe --silent --flow uninstall --path "D:\NeoServer"
```

**静默默认值**（与旧 NeoInstaller --silent 语义一致）：

| 交互步骤 | 静默取值 |
|---|---|
| 许可 | 视为接受 |
| 路径 | `--path` 的值；无 → product 默认 `C:\Program Files\NeoServer` |
| 组件 | 默认不勾选编辑器（`--with-editor` 除外） |
| Git | auto：检测到系统 Git 用之；否则下载内置（`--use-system-git/--use-bundled-git` 强制） |
| 完成 | 不启动程序（即使 finish 定义了 launch） |

**CreateProcessA / 无终端调用**（服务启动器、安装器再打包等）：带参数运行即进入无窗口无头执行；无控制台时日志落到 `%TEMP%/hci_gui.log`，错误同步输出 stderr（有重定向时）。
**自动化钩子**：环境变量 `HCI_GUI_AUTOPILOT=1`（非空）等价静默语义，供冒烟测试。

### 3.3 静默卸载

`--flow uninstall` 流程 = 确认（静默自动确认）→ 删除注册表卸载项（`HKCU\Software\HiBer2007\NeoServer`，若存在）→ 清空安装目录内容（目录本身保留）。

## 4. 产物与副作用

| 项 | 位置/内容 |
|---|---|
| 释放文件 | 目标目录整体 = 构建产物 `build/deploy` 全量（跳过 `*.dmp` 与 `config/custom/*`） |
| `install.conf` | `<安装目录>/install.conf`，四键：`install_path` / `install_editor` / `use_system_git` / `git_path`（主程序 `InstallConfig::load()` 读取，跨 Git 布局探测不变） |
| Git | 系统 Git（存在时）；否则 `<安装目录>/tools/git/`（MinGit/PortableGit，zip 解压） |
| 快捷方式 | 桌面 `NeoServer.lnk` + 开始菜单 `NeoServer/NeoServer.lnk`；`--with-editor` 额外桌面 `NeoWorkspaceEditor.lnk` |
| 立即运行 | GUI 完成页勾选（默认勾选）；静默模式不启动 |

## 5. 常见场景

```powershell
# 1) 图形安装（默认路径）
.\hci_gui.exe

# 2) 图形安装 + 编辑器 + 内置 Git（PortableGit）
.\hci_gui.exe --with-editor --use-bundled-git

# 3) 静默安装到指定目录（供 CI/一键脚本）
.\hci_gui.exe --silent --path "D:\NeoServer" --with-editor
echo $LASTEXITCODE        # 0 = 成功

# 4) 带 JSON 协议的流程控制/审计（需另行构建 hci_cli，见 HCI 仓库 shell-cli.md）
hci_cli.exe --product product.json --flow install --silent --path "D:\NeoServer" --json

# 5) 外部产品/流程调试（不修改内嵌资产时）
.\hci_gui.exe --product .\nsum_installer\product.json --flow .\nsum_installer\install.json --path "D:\NeoServer"
```

## 6. 已知限制

- **GitHub Releases 下载**：获取 Git 下载地址依赖 `api.github.com`（正常网络可用）。某些受限网络（如本机直连 SSL error 35）无法下载——此时建议 `--use-system-git` 或预装系统 Git（检测到系统 Git 时 auto 模式直接使用，不联网）
- **目标目录会被清空**（先确认目录语义再指路径；GUI 有警告弹窗，静默模式无）
- **权限**：装到 `C:\Program Files` 需管理员权限；无权限会失败（exit 1）
- **功能实测归用户**：向导交互体验与安装后主程序行为由用户验收（冒烟仅验证无崩溃）

## 7. 相关文档

- [HiBerCommonInstaller 集成指南](HiBerCommonInstaller-integration.md) — 构建编排/资产修改/验证方法
- [nsum_installer README](../nsum_installer/README.md) — 资产目录结构与修改须知
- HiBerCommonInstaller 仓库 [docs/](https://github.com/HiBer2007/HiBerCommonInstaller/blob/main/docs/README.md) — 框架通用文档（product-config / flow-script / shells / pitfalls）