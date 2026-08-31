# nsum_installer — NSUM 安装器资产

NSUM 安装器 = HiBerCommonInstaller 的 `hci_gui`（静态 Qt 单文件，qrc 内嵌以下资产）。本目录是**主仓库侧全部安装器专属内容**（框架本身保持产品无关，见 `modules/HiBerCommonInstaller/` 与 HCI 仓库 docs/）。

## 目录结构

```
nsum_installer/
├── README.md               # 本说明
├── product.json            # 产品配置（名称/组件/快捷方式/install.conf 模板/流程引用/payload 规则）
├── install.json            # 安装流程（欢迎/许可/路径/组件 → Git 决策 → 释放 → 配置 → 快捷方式 → 完成）
├── uninstall.json          # 卸载流程（确认 → 注册表 → 清空）
├── LICENSE.txt             # 内嵌许可（复制自主仓库根 LICENSE）
└── ext/
    └── nsum_args_ext.cpp   # NSUM 专属参数/步骤拓展（静态链接并入 hci_gui）
```

## 构建编排（根 CMakeLists INSTALLER_ONLY 分支）

1. `HCI_PRODUCT_FILES` = `deploy/<rel>=<abs>` 全量（跳过 `*.dmp`/`config/custom/*`）+ 上面的
   `product.json / install.json / uninstall.json / LICENSE.txt`（configure 期固化进 qrc）
2. `add_subdirectory(modules/HiBerCommonInstaller)`（HCI_BUILD_GUI=ON + HCI_QT_STATIC=ON）
3. `target_sources(hci_gui PRIVATE nsum_installer/ext/nsum_args_ext.cpp)` —— 拓展以
   `HCI_REGISTER_EXTENSION` **静态注册**编译进安装器，无 extensions/ 目录、无 DLL

## 修改须知

- **json 资产改动**：内容在 configure 期固化进 qrc —— 改后必须重跑 configure（不能只 rebuild）
- **ext 改动**：直接改 `nsum_installer/ext/nsum_args_ext.cpp` 后 rebuild（hci_gui 依赖该源文件）
- **deploy 内容**：随 `neo_deploy`（msvc 预设）产出；installer-static 构建时自动嵌入

## 使用文档

面向最终用户/脚本的完整使用文档见 [docs/NSUM-Installer.md](../docs/NSUM-Installer.md)（GUI / 静默 / CLI 参数全表 / 退出码 / 场景）。