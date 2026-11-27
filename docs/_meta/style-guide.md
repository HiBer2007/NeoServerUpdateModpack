# 文档写作规范（Style Guide）

> 本目录（`docs/_meta/`）不进入站点与发布包。所有文档贡献者必读。

## 1. 语言与双语

- 中文（`docs/zh/`）是 **canonical** 源；英文（`docs/en/`）逐页镜像，路径、文件名、锚点 ID 完全一致。
- 代码块、CLI 命令、JSON 字段、配置键、日志一律保留英文，不翻译。
- 专有名词不译：`NSUM`、`NSUM Runtime`、`FLFHMCL`、`HMCL`、`HIC`、`MCLHR`、`MCBBS`、`Modrinth`、`nsum-bridge`、`MCDR`。
- 固定译法见 [glossary-zh-en.md](./glossary-zh-en.md)。新增术语先补术语表。

## 2. 目录与命名

- 目录：`docs/<lang>/<section>/...`，四板块固定为 `user`、`pack-author`、`developer`、`extensions`。
- 文件名：小写 kebab-case（`build-pipeline.md`）；模块文档例外：`developer/Modules/<Target>/{README,usage}.md`（`Modules` 大写）。
- 每个目录必须有 `_meta.json`（Nextra 侧边栏顺序与显示名）；新增/删除页面必须同步 `_meta.json`。
- 页面 slug 即文件名；不随意改名；确需改名时中英同时改并更新所有链接。

## 3. front-matter

```yaml
---
title: 页面标题
description: 一句话摘要（搜索与卡片使用，40–80 字）
lang: zh            # zh | en
status: complete    # complete | planned
updated: 2026-11-27
tags: [build, export]
related:            # 可选，站内相对路径
  - ../developer/architecture/overview.md
---
```

- `status: planned` 表示占位页（`docs_check` 只警告）；发布前必须清零。
- 英文页额外加 `translation_of: /zh/<path>`。

## 4. 页面结构

按页面类型套模板（`templates/`）：

| 类型 | 模板 | 适用 |
|---|---|---|
| 概念/原理 | `concept.md` | `architecture/**`、`pack-author/repo-structure` 等 |
| 操作指南 | `howto.md` | `user/**`、`pack-author/editor-*` 等 |
| 参考 | `reference.md` | CLI、配置字段、格式字段 |
| 模块说明 | `module-readme.md` | `Modules/<M>/README.md` |
| 模块用法 | `module-usage.md` | `Modules/<M>/usage.md` |
| 设计决策 | `adr.md` | `developer/adr/*` |

通用要求：

1. 开头一段「本页解决什么问题」，不超过 3 句；
2. 二级标题用 `##`，最多四级；
3. 每页结尾「相关文档」链接 2–5 条；
4. 命令、JSON、路径一律 fenced code block 并标注语言；
5. 图：`![图 3-1 构建流水线](../../../assets/diagrams/03-build-pipeline.svg)`，图号在正文首次引用；Mermaid 源放 `docs/assets/diagrams/NN-name.mmd`，导出同名 `.svg` 提交；
6. 截图放 `docs/assets/screenshots/`，命名 `screen-<场景>.png`；不得含用户名/本机路径等隐私；
7. 提示块统一：`> **注意**：…`、`> **提示**：…`、`> **警告**：…`；
8. 不写裸 HTML（Nextra/MDX 兼容），需要交互组件时用 MDX 组件占位并注明。

## 5. 链接规则

- 站内链接使用**相对 `.md` 路径**（GitHub 与 Nextra 均可解析）：`../developer/architecture/overview.md`；
- 图片使用相对路径；
- 跨语言链接不要手写，语言切换交给站点；
- 外链使用完整 `https://`；
- `user/` 与 `pack-author/` 禁止链接 `developer/`、`extensions/` 的**相对**路径（发布包不含这些板块），需要引用时用线上文档完整 URL 或「在线文档」文字；
- 占位槽位语法：`<!-- slot:FLFHMCL_INSTALL -->` 紧邻一段「待补」提示块；槽位 ID 必须登记在 [placeholders.md](./placeholders.md)。

## 6. 图表与截图规范

- 图源与导出成对提交（`.mmd` + `.svg`）；SVG 优先，避免位图截图用于架构图；
- Mermaid 统一 `flowchart LR`/`sequenceDiagram`/`stateDiagram-v2`/`erDiagram`，主题色跟随默认；
- 截图：窗口内容区域、2x、裁剪掉桌面与个人路径；英文页复用同图并写英文图注；
- 图表编号：`图 <板块编号>-<序号>`，板块编号 user=1、pack-author=2、developer=3、extensions=4。

## 7. 代码与事实

- API、字段、参数、退出码必须**逐字核对源码/头文件/meta.json**，禁止凭记忆；
- 示例命令必须可复制执行；涉及路径用占位符 `<仓库>`、`<实例目录>`；
- 版本相关行为标注（如「Runtime ≥ 1.0.5」）；
- 不写内部临时路径（`K:\...`）、个人目录、令牌。

## 8. 校验与提交

```powershell
pwsh -NoProfile -File tools/docs_check.ps1
pwsh -NoProfile -File tools/build_docs.ps1 -Profile with-editor -Out K:\tmp\docs_out
```

- 提交前 `docs_check` 必须 0 error（planned 页除外）；
- 文档改动与代码改动同 PR 时，在 PR 描述里列出受影响页面；
- 双语：先中文合并，英文翻译必须与中文同 PR 或紧随的翻译 PR（同一里程碑内不允许长期缺页）。
