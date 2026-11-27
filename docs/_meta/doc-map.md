# 文档地图（Doc Map）

> 单页状态与归属；新增页面先在此登记，再创建文件与 `_meta.json` 条目。
> 状态：`complete` / `wip` / `planned`。

## 顶层

| 路径 | audience | 状态 | 负责人 |
|---|---|---|---|
| `docs/zh/index.md` / `docs/en/index.md` | all | complete | docs |
| `user/` | player | wip | docs-user |
| `pack-author/` | pack author | wip | docs-pack |
| `developer/` | developer | wip | docs-dev |
| `extensions/` | all | wip | docs-ext |

## 板块 1 · user

| 页面 | 状态 | 备注 |
|---|---|---|
| index | wip | |
| installation | wip | 含 Editor 选项说明 |
| quick-start | wip | `FLFHMCL_INSTALL` 槽位 |
| launcher-guide | wip | 截图组 |
| cli | wip | |
| sync-and-launch | wip | `FLFHMCL_LAUNCH` 槽位 |
| updates | wip | `FLFHMCL_UPDATE` 槽位 |
| faq | wip | `FLFHMCL_COMPAT` 槽位 |
| troubleshooting | wip | 分诊树 |

## 板块 2 · pack-author

| 页面 | 状态 |
|---|---|
| index / repo-structure / sync-rules | wip |
| editor-gui / editor-cli | wip |
| build-and-export / nsumpack / pointers | wip |
| best-practices / publish | wip |

## 板块 3 · developer

| 分组 | 页面 | 状态 |
|---|---|---|
| architecture | index、overview、design-goals、data-model、build-pipeline、sync-engine、pointer-system、downloader、security | wip |
| framework | index、base-framework、module-layout、event-bus、extension-registry、plugin-contracts、abi | wip |
| Modules | 34 × {README,usage} | wip |
| process | testing、contributing、versioning、release | wip |
| adr | index + 10 篇 | wip |

## 板块 4 · extensions

| 分组 | 页面 | 状态 |
|---|---|---|
| parsers | index + json/yaml/toml/snbt/txt/properties | wip |
| pointers | index + direct-url/modrinth | wip |
| exporters | index + mcbbs/modrinth/hmcl/nsum | wip |
| editor | index + parser-extensions/pointer-extensions | wip |
| installer | index + cli/flow/extensions/external-download | wip |
| client-bridge | index/usage/tasks/download | wip |
| server-plugin | index/install/sync/config | wip |
| libraries | index/crashtracker/powerhelper/guibergui/gitignore-markup | wip |
| mclhr | 单页 | wip |

## 资产

| 类型 | 目录 | 状态 |
|---|---|---|
| Mermaid 源 | `docs/assets/diagrams/*.mmd` | wip |
| Mermaid 导出 | `docs/assets/diagrams/*.svg` | wip |
| 截图 | `docs/assets/screenshots/*.png` | wip |
