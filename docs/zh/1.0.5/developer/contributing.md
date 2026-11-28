---
title: 贡献指南
description: NSUM 的分支与提交信息约定、PR 检查单、代码规范（UTF-8 路径、英文日志、无注释）与文档更新规则。
lang: zh
status: complete
updated: 2026-11-27
tags: [contributing, workflow, conventions]
---

# 贡献指南

本页是提交代码前的约定汇总：怎么开分支、提交信息怎么写、PR 需要过哪些检查、哪些代码规范是硬性铁律、文档与代码如何同步。构建命令与路径铁律以仓库根的 `AGENTS.md` 与 `CMakePresets.json` 为准。

## 分支模型

- 主分支 `main` 保持可构建、可部署；历史提交基本为 `main` 上的小步提交。
- 外部贡献使用短期主题分支：`feat/<scope>-<topic>`、`fix/<scope>-<topic>`、`docs/<topic>`，合并后删除。
- 不重写已推送的 `main` 历史；需要撤销时用 `git revert` 生成新提交。
- 一个分支只做一件事；混入无关格式化会显著提高评审成本。
- submodule 的代码改动先在其独立仓库提交并推送，主仓库只更新 gitlink（见下文「Submodule 规则」）。

## 提交信息

采用 Conventional Commits，subject 用英文、祈使句、不超过 72 字符，正文可用中文补充「为什么」：

```text
<type>(<scope>): <summary>

<可选正文：问题、约束、验证方式>
```

近期历史提交的 `type` 与 `scope` 直接可作为参考：

| 类型 | 用途 |
|---|---|
| `feat` | 新能力、新格式、新命令 |
| `fix` | 缺陷修复 |
| `chore` | 版本递增、依赖/子模块指针、杂项维护 |
| `docs` | 纯文档改动 |
| `refactor` | 不改变行为的结构调整（含模块拆分/改名） |
| `build` | CMake、预设、打包脚本 |
| `release` | 发布提交，形如 `release: NSUM 1.0.5` |

| `scope` | 覆盖范围 |
|---|---|
| `build` / `export` | NeoBuild、导出插件、构建与导出流程 |
| `sync` | 同步策略、L1/L2/L3、serverconfig |
| `editor` | NeoWorkspaceEditor（GUI 与 CLI） |
| `download` | 指针下载、并行下载、Git 更新 |
| `installer` | HiBerCommonInstaller、`nsum_installer/` |
| `git` | 内置 Git 探测、更新、仓库信任 |
| `branch` | 分支类型、核心、继承 |
| `docs` | 双语文档树 |

示例（均来自真实历史）：

```text
feat(editor): add headless CLI mode for AI operations
fix(sync): strict mirror at version root must not delete HMCL game artifacts
feat(export): make .nsumpack metadata-only with optional embedded files
chore: bump internal version to 0.1.6 for HIC download features
release: NSUM 1.0.5
```

## 提交前检查单

- [ ] `cmake --build build --target neo_deploy` 退出码 0。
- [ ] `NeoServerUpdateModpack.exe -v` 与 `info version --json` 输出与 `version.json` 一致。
- [ ] 改了共享头文件的类成员/布局：已全量重建（`--clean-first` 或删旧 obj），并做过冒烟。
- [ ] 改了 CLI 命令、参数或 JSON 字段：同步更新 `docs/zh/` 对应页面（`user/cli.md`、`pack-author/editor-cli.md`）。
- [ ] 改了插件契约或模块公共 API：同步 `docs/zh/developer/Modules/<模块>/{README,usage}.md`。
- [ ] 运行 `pwsh -NoProfile -File tools/docs_check.ps1`，0 error。
- [ ] 提交内容不含本机绝对路径、用户名、令牌、`crash-report/`、`*.log`、本地 store。
- [ ] 递增内部版本号（日常迭代 `internal-patch`，见 [版本管理](./versioning.md)）。
- [ ] 更新 `PLAN.md` 的功能更新记录与已知问题（维护流程要求，见 AGENTS.md）。

## PR 检查单

| 项目 | 要求 |
|---|---|
| 范围 | 单一主题；标题使用 Conventional Commits 格式 |
| 构建 | 本机全量或增量构建通过，无新增警告 |
| 测试 | 说明跑了哪些层（结构自检/端到端/bridge/冒烟），并给出关键输出 |
| 版本 | 说明本次是否递增版本、递增了哪一段 |
| 文档 | 列出受影响的文档页面；中英同步或注明翻译 PR |
| 兼容性 | 涉及 `.nsumpack`、`workspace.json`、CLI JSON 时说明前后兼容策略 |
| 截图 | GUI 变更附窗口内容区截图，不含桌面与个人路径 |
| 风险 | 指出回滚方式（revert 提交 / 重新发布旧版本） |

## 代码规范

基础：C++17、Qt6（Widgets/Network/Core）、CMake ≥ 3.20、vcpkg 依赖；模块各有命名空间（`NeoCore`、`NeoWorkspace`、`NeoBuild`、`NeoCLI`、`GUIWorker`、`HiBerGUI`）；头文件用 `#pragma once` 或 include guard，放在模块 `include/`；取消检查用 `NeoCore::CancelToken`。

硬性铁律：

1. **不写注释**，代码自文档化；确需解释时把信息放进提交信息、文档或日志。
2. **日志全英文**：`CLogger::*`、`qInfo`/`qWarning`、`reportProgress` 的消息只用 ASCII 英文；界面文本仍用中文。
3. **源码纯 ASCII**：C++ 源码里不写中文字面量，必要的中文用 `\uXXXX` 转义（显式 UTF-8 编译下也不会触发 C4819）。
4. **UTF-8 路径全链**：Windows 上 `fs::path` ↔ `std::string` 一律 `fs::u8path` / `u8string()`；供 JSON/UI 显示的相对路径用 `generic_u8string()`（正斜杠）。禁止 `fs::path(std::string)`、`fs::exists(std::string)`、`path.string()`。
5. **argv 重建**：带路径参数的 EXE 入口必须用 `CommandLineToArgvW(GetCommandLineW())` + `WideCharToMultiByte(CP_UTF8, …)` 重建 UTF-8 argv；exe 目录用 `GetModuleFileNameW`。
6. **`git status` 必须 `-z`**：非 `-z` 输出会对中文路径做八进制转义、对含空格路径加引号，后续 `git add` 静默失败；暂存删除用 `git add -A`。
7. **插件导出符号**：解析器/指针/导出器的 `extern "C" CreateXxx()` 必须带 `__declspec(dllexport)`，否则 DLL 零导出、`GetProcAddress` 失败；插件日志走 `plugin_log_sink.h` 的 `NEO_DECLARE_PLUGIN_LOG_SINK`。
8. **共享头文件类成员变更后全量重建**：Ninja 可能漏检头依赖，旧 obj 按旧 `sizeof` 分配会写越界（表现为关闭时 0xC0000374）。改完必须确认所有 `new Xxx()` 的 TU 都重编，并做冒烟。
9. **`std::move` 后不得再读被移对象**；Qt 子对象交给父对象析构，不在析构里手动 `delete` 已注册父子关系的成员。
10. **选项文案不带注释**：GUI 下拉/按钮文本只用简洁功能名，不写 `(git add)` 之类的括号说明。

## 文档更新规则

- 中文 `docs/zh/` 是 canonical 源；英文 `docs/en/` 逐页镜像，路径与文件名完全一致。
- 每个页面必须有 front-matter：`title`、`description`、`lang`、`status`、`updated`（占位页用 `status: planned`，发布前必须清零）。
- 新增/删除页面同步对应目录的 `_meta.json`（侧边栏顺序与显示名），并在 `docs/_meta/doc-map.md` 登记。
- 站内链接用相对 `.md` 路径；不引用不存在的图片；架构图用 `docs/assets/diagrams/` 下的 Mermaid 源与导出 SVG。
- `user/` 与 `pack-author/` 不得相对链接 `developer/`、`extensions/`（发布包不含这两个板块），需要引用时用线上文档 URL 或「在线文档」文字。
- 文档事实必须逐字核对源码/头文件/meta.json：API 名称、参数、字段、退出码不许凭记忆写；不确定处标 `[待确认]`。
- 提交前跑门禁：

```powershell
pwsh -NoProfile -File tools/docs_check.ps1
pwsh -NoProfile -File tools/build_docs.ps1 -Profile with-editor -Out '<输出目录>'
```

- 改 CLI 代码需同步 CLI 页面；改模块公共 API 需同步该模块 `README.md` 与 `usage.md`；改发布流程需同步 [发布](./release.md) 与 [测试](./testing.md)。

## Submodule 规则

主仓库以 submodule 引用可复用库与通用组件：`CommonLoggerCPP`、`HiBerGUILibCPP`、`GitIgnoreMarkup`、`PowerHelper`、`CrashTracker`、`CrashTrackerHandleLib`、`HiBerCommonInstaller`、`nbtcpp`。

- 克隆：`git clone --recursive` 或克隆后 `git submodule update --init --recursive`（GitIgnoreMarkup 还嵌套 HiBerGUILibCPP，需递归）。
- 修改：在 submodule 仓库内提交并推送，然后主仓库 `git submodule update --remote modules/<名>`，提交更新的 gitlink。
- 不要在 submodule 目录内改文件后直接在主仓库提交；不要把 submodule 的文件复制回主仓库。
- 根 `CMakeLists.txt` 的 `add_subdirectory` 顺序有依赖约束：`CrashTrackerHandleLib` 与 `PowerHelper` 必须先于 `NeoCore`；`HiBerGUILibCPP` 必须先于 `GitIgnoreMarkup`。调整顺序前先看该处注释。
- 独立仓库的构建/依赖（vcpkg.json、示例、版本资源函数）各自维护，主仓库不做兜底。

## 常见退回原因

| 现象 | 原因 | 处理 |
|---|---|---|
| 构建报 `LNK2019` / 运行期成员错位 | 共享头签名或类布局变了，旧 obj/旧插件 DLL 未重编 | `--clean-first` 全量重建，插件 DLL 与宿主一起重编 |
| 中文路径导出崩溃（0xC0000409） | 入口 argv 仍是 ANSI | 按第 5 条重建 UTF-8 argv |
| 日志里出现中文或乱码 | 违反第 2 条 | 改为英文；判据用 UTF-8 解码后统计 U+FFFD |
| `docs_check` 报 `en missing page` | 只加了中文页 | 同 PR 补英文镜像页 |
| 评审发现提交含本机路径 | 复制命令时未替换占位符 | 文档与日志统一用 `<仓库根>`、`<实例目录>` 等占位符 |

## 相关文档

- [测试与验证](./testing.md)
- [版本管理](./versioning.md)
- [发布](./release.md)
- [模块参考](./Modules/NeoCore/README.md)
- [架构概览](./architecture/overview.md)
