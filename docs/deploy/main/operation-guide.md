# NSUM 构建工具 — 主程序操作指南

本文档面向 NSUM 构建工具（NeoServerUpdateModpack）的最终用户，详细说明主程序 GUI 向导的
每一个页面、完整构建流程以及内置帮助入口的用法。

## 快速上手

1. 双击 `NeoServerUpdateModpack.exe` 启动（或从命令行以零参数启动进入 GUI 模式）。
2. 在「仓库选择」页填入 Git 仓库地址（远程 https/ssh 或本地路径）。
3. 依次选择分支、整合包、导出类型与导出目录。
4. 确认「构建清单」后点击「开始构建」。
5. 构建完成后在「完成」页打开输出目录，将整合包分发给服务器/玩家。

> 完整格式说明见 [导出格式详解](./formats.md)，常见问题见 [故障排查](./troubleshooting.md)。

## 环境与部署目录

| 路径 | 说明 |
|------|------|
| `NeoServerUpdateModpack.exe` | 主程序（GUI 向导 + CLI 双模式） |
| `parsers/` | 配置解析器插件（JSON/YAML/TOML/SNBT/TXT） |
| `pointers/` | 指针解析器插件（Modrinth/直链） |
| `exporters/` | 导出插件（MCBBS/Modrinth/HMCL） |
| `docs/` | 部署文档组（本文档、CLI 文档、PowerHelper 文档等） |
| `PowerHelper.exe` | 文档阅读器（帮助入口依赖） |
| `CrashTracker.exe` | 崩溃转储分析工具 |
| `config/history/` | 最近仓库历史与远程仓库缓存 |
| `crash-report/` | 崩溃报告目录 |

## 界面总览

主窗口为 9 页向导：**仓库选择 → 分支选择 → 整合包选择 → 导出类型 → 导出目录 →
额外信息 → 构建清单 → 构建执行 → 完成**。

底部状态栏包含：

- **左下角「帮助文档」文本标签**：点击打开与当前页面/状态相关的帮助（见 [帮助入口](#帮助入口)）。
- 右下角版本号 `NSUM v1.0.0`（Ctrl+点击为崩溃测试入口，仅供开发者使用）。

## 仓库选择

在输入框填写 Git 仓库地址，支持三种来源：

| 来源 | 示例 | 说明 |
|------|------|------|
| 远程 HTTPS | `https://github.com/owner/repo.git` | 自动克隆到本地缓存（`config/history/cache/`） |
| 远程 SSH | `git@github.com:owner/repo.git` | 需已配置 SSH 密钥 |
| 本地路径 | `D:\workspace\repo` | 直接使用本地仓库目录（含 `.git`） |

- 最近的仓库会显示在下方列表中，点击即可快速复用（按类型过滤：远程/本地/缓存）。
- 点击「开始」后程序会拉取远程更新（`git fetch`），本地仓库要求处于可用状态。

## 分支选择

仓库加载后列出所有分支，每项显示：

- 分支名与描述（description）
- 默认分支标记（默认 `master`/`main`）
- 隐藏分支不在此页出现

选择用于构建的分支。分支继承链（parent 关系）在整合包选择页体现。

## 整合包选择

列出所选分支下可用的整合包（客户端/服务端等），每项包含：

- 整合包名称与版本
- 游戏版本（如 1.20.1）、加载器（Fabric/Forge/NeoForge）
- 继承来源（若整合包继承自其他分支）
- 隐藏标记

## 导出类型

选择整合包打包格式：

| 格式 | 扩展名 | 适用平台 | 详见 |
|------|--------|----------|------|
| MCBBS 整合包 | `.zip` | PCL2 / HMCL / 通用 | [formats.md](./formats.md) |
| Modrinth | `.mrpack` | Modrinth App / 第三方启动器 | [formats.md](./formats.md) |
| HMCL 工作区 | 目录 | HMCL 直接同步到游戏工作目录 | [formats.md](./formats.md) |

## 导出目录

选择输出位置。输出文件名的组成规则随格式不同：

- `mcbbs`：`<整合包名>_modpack.zip`
- `modrinth`：`<整合包名>.mrpack`
- `hmcl`：直接写入所选目录（不打包）

## 额外信息

按所选导出格式填写附加元数据（如 `name`/`version`/`summary` 等）。必填字段未填写时
「下一步」不可用或会被拦截提示。每个字段后都有说明文字。

## 构建清单

构建前预览页，展示：

- 即将执行的构建步骤摘要
- 输出文件结构预览（HMCL 工作区树 / mcbbs 结构 / mrpack 结构）
- 涉及的同步策略、指针文件数量等信息

确认无误后点击「开始构建」。

## 构建执行

构建过程页，显示实时进度：

- 阶段进度与总体百分比（动效进度条）
- 当前操作说明（拉取分支、合并继承、解析配置、下载指针、同步服务端配置、打包导出等）
- 可随时点击「取消」中止（已下载的指针缓存会保留，下次构建复用）

## 构建完成

构建结果页：

| 结果 | 显示 |
|------|------|
| 成功 | ✅ 构建完成 + 输出目录路径 + 「打开输出目录」按钮 |
| 成功（含警告） | 额外显示警告数量与「展开警告详情」按钮 + **「打开帮助文档」按钮** |
| 失败 | ❌ 失败原因 + 可能解决方案 + **「打开帮助文档」按钮** |

> 警告/失败时出现「打开帮助文档」按钮，点击拉起 PowerHelper 阅读器查看故障排查文档。

## 帮助入口

帮助内容由 PowerHelper 文档阅读器渲染（`PowerHelper.exe` + `docs/` 部署文档组），
具体打开的文档取决于当前工作模式、页面与状态：

| 场景 | 打开的文档 |
|------|-----------|
| 正常向导（flow 关闭） | `docs/main/operation-guide.md`，并定位到当前页面章节 |
| 完成页·构建失败 | `docs/main/troubleshooting.md`「构建失败」章节 |
| 完成页·构建成功 | 操作指南「构建完成」章节 |
| flow 模式（`flow gui` 拉起） | `docs/CLI/CLI-flow.md` |
| 构建完成页有警告/失败 | 「完成页」的「打开帮助文档」按钮（同上） |

若 `PowerHelper.exe` 缺失，点击会提示「无法打开帮助」——请确保部署目录完整。

## CLI 模式

主程序同时提供三类子命令 CLI（英文）：`info`（信息获取）、`flow`（流程控制）、
`exec`（执行操作），完整参考见 [CLI 文档](../CLI/CLI.md)。

常用示例：

```
NeoServerUpdateModpack.exe info version
NeoServerUpdateModpack.exe flow gui --from repo --to modpack --prefill repo=https://...
NeoServerUpdateModpack.exe exec verify-repo --repo D:\workspace\repo
```

## 术语表

| 术语 | 说明 |
|------|------|
| 指针文件 | 缺失文件的占位符（`<sha256>.pointer`），构建时按哈希下载还原 |
| 指针缓存 | `.minecraft/versions/.cache/<sha256>`，已下载文件按哈希去重复用 |
| 同步策略 | 配置文件夹/文件随构建同步到输出（跟随默认/增量/镜像/跳过等） |
| 分支继承 | 整合包分支可继承父分支文件，构建时自动合并（含删除/覆盖标记） |
| serverconfig 规则 | 服务端配置的同步规则（覆盖/部分同步/忽略） |
