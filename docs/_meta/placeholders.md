# 外部文档占位槽位（Placeholders）

> 页面中待回填的外部文档链接统一用槽位标记，**不要**临时写死无效 URL。
> 语法：页面内写 `<!-- slot:SLOT_ID -->`，紧邻其后放提示块。回填时只替换链接目标，不改正文结构。

## 槽位登记表

| 槽位 ID | 使用位置 | 预期目标 | 状态 |
|---|---|---|---|
| `FLFHMCL_INSTALL` | `user/installation`、`user/quick-start` | FLFHMCL 安装与运行时注入 | 已回填（2026-11-27，FLFHMCL docs/bilingual） |
| `FLFHMCL_UPDATE` | `user/updates` | FLFHMCL 升级方式 | 已回填（2026-11-27，FLFHMCL docs/bilingual） |
| `FLFHMCL_COMPAT` | `user/faq`、`user/troubleshooting` | FLFHMCL/HMCL 版本兼容矩阵 | 已回填（2026-11-27，FLFHMCL docs/bilingual） |
| `FLFHMCL_LAUNCH` | `user/sync-and-launch` | FLFHMCL 启动器侧行为 | 已回填（2026-11-27，FLFHMCL docs/bilingual） |
| `MCDR_PLUGIN_REPO` | `extensions/server-plugin/*` | MCDR 插件仓库与发布页 | 待补 |
| `ONLINE_DOCS` | 发布包内跨板块引用 | 线上文档站根地址 | 待补 |

## 使用示例

```markdown
<!-- slot:FLFHMCL_INSTALL -->
> **待补**：FLFHMCL 安装说明完成后在此补充链接。
> 在此之前请参考 FLFHMCL 仓库的 README。
```

## 回填规则

1. 槽位内容由对应文档负责人回填（FLFHMCL 相关由 FLFHMCL 文档作者提供目标 URL）；
2. 同一槽位在中文与英文页面同时回填；
3. 回填后把本表状态改为「已回填」并记录日期；
4. `tools/docs_check.ps1` 会统计未回填槽位数量（列在报告末尾，不作为 error）。
