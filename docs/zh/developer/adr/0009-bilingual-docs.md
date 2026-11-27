---
title: ADR 0009 · 中英双语文档体系
description: 中文作为 canonical 源、英文逐页镜像，配合 front-matter 契约、docs_check 门禁与按发布场景分档打包。
lang: zh
status: complete
updated: 2026-11-27
---

# ADR 0009 · 中英双语文档体系

## 状态

已接受（2026-11-27，文档体系重构落地）

## 背景

早期文档是散落在 `docs/` 下的单语页面，既没有统一结构，也没有任何校验：

- 读者分成玩家、整合包作者、开发者、扩展作者四类，需求差异大，混在一起无法导航；
- 发布包只需要玩家与整合包作者文档，站点需要全部板块，但当时只有一份拷贝；
- 双语靠人工维护，英文页经常缺页、链接指向不存在的锚点；
- 文档里的字段、命令、退出码容易与代码脱节，且没有任何门禁能发现。

## 决策

- **中文为 canonical**：`docs/zh/` 是唯一权威源，`docs/en/` 逐页镜像，**路径与文件名完全一致**；代码/CLI/字段保持英文不译，专有名词按术语表保留。
- **页面契约**：每页必须有 front-matter（`title`、`description`、`lang`、`status`、`updated`），`status: planned` 表示占位、发布前必须清零；页面结构按模板（概念/操作/参考/模块说明/ADR）组织，结尾给 2–5 条相关文档。
- **板块划分**：`user`、`pack-author`、`developer`、`extensions` 四个板块，另有 `developer/Modules`、`releases`；每个目录用 `_meta.json` 定义侧边栏顺序与显示名。
- **链接约定**：站内链接一律相对 `.md` 路径；图片用相对路径；跨语言切换交给站点，不手写；`user/` 与 `pack-author/` 不得相对链接 `developer/`、`extensions/`（发布包不含这两个板块）。
- **门禁 `tools/docs_check.ps1`**：校验 front-matter 必需项与 `lang` 一致性、中英页面一一对应（缺页为 error）、绝对本机路径、死链与死图、锚点（warning）、打包板块的跨板块相对链接、占位槽位是否登记、代码围栏成对、`_meta.json` 条目与页面是否对应；0 error 才允许提交。
- **分档打包 `tools/build_docs.ps1`**：
  - `runtime`：只发 `user`；
  - `with-editor`：`user` + `pack-author`（后者放在 `_editor/`，由编辑器首次启动时提升进 `docs/<lang>/pack-author` 并替换 `_meta.json`，随后删除 `_editor`）；
  - `full`：四个板块 + `releases` + 根 `_meta.json`，用于站点与本地预览。
  `neo_deploy` 构建时以 `with-editor` 生成 `deploy/docs`。
- **辅助资产**：`docs/_meta/` 存放写作规范、术语表、占位槽位登记、模板与文档地图，不进入站点与发布包。

## 备选方案

| 方案 | 优点 | 缺点 |
|---|---|---|
| 只维护中文 | 成本最低 | 英文用户无法阅读，扩展生态受限 |
| 中英分仓 | 各自独立演进 | 同步成本更高，链接与锚点更容易漂移 |
| 站点运行时 i18n（同页多语言字段） | 单一文件 | 翻译与评审困难，离线/发布包无法复用 |
| 中文 canonical + 英文镜像 + 门禁（本决策） | 结构统一、可校验、可按场景打包 | 每页要维护两份，翻译需与中文同 PR |

## 后果

- 正面：发布包只带必要板块且体积可控；缺页/死链在提交前暴露；术语与结构统一，读者可按角色进入；编辑器内置帮助随包分发。
- 负面：双语镜像使每篇文档的维护成本翻倍；`docs_check` 只覆盖结构与链接，不校验技术事实，仍需逐字核对源码；`_meta.json` 与目录必须同步，否则侧边栏缺项。
- 影响范围：`docs/zh`、`docs/en`、`tools/docs_check.ps1`、`tools/build_docs.ps1`、根 `CMakeLists.txt` 的 `neo_deploy` 文档步骤、编辑器首次启动的文档提升逻辑。

## 相关

- [贡献指南](../contributing.md)（文档更新规则）
- [测试与验证](../testing.md)（文档校验命令）
- [架构概览](../architecture/overview.md)
- [编辑器 CLI（打包者视角）](../../pack-author/editor-cli.md)
