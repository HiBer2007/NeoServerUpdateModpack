# NSUM 文档源

本目录是 NSUM 文档的唯一事实源，按 **版本 × 四板块 × 双语** 组织，结构已按 Next.js + Nextra 的约定设计（每目录一份 `_meta.json`、`index.md` 作为首页、kebab-case slug、相对链接）。

## 目录

```text
docs/
├─ zh/                      # 中文（canonical）
│  ├─ _meta.json            # 版本入口：1.0.6（最新）、1.0.5
│  ├─ index.md              # 文档首页（版本选择）
│  ├─ 1.0.6/                # 当前版本
│  │  ├─ user/              # 板块 1：主程序使用
│  │  ├─ pack-author/       # 板块 2：整合包作者（仅安装 Editor 时随包释放）
│  │  ├─ developer/         # 板块 3：开发者（架构/框架/ABI/Modules/流程/ADR）
│  │  ├─ extensions/        # 板块 4：拓展功能（分组）
│  │  └─ releases/          # 版本说明
│  └─ 1.0.5/                # 历史版本（冻结，不再修改）
├─ en/                      # 英文（与 zh 逐版本、逐页镜像）
├─ assets/                  # 语言/版本中立：diagrams / screenshots / images
├─ _meta/                   # 写作规范、模板、术语表、槽位登记（不发布）
```

## 版本规则

- 新功能只写入**当前版本目录**（如 `docs/zh/1.0.6/`），并在页面顶部注明“自 x.y.z 起”；
- 发布新版本时复制上一版本目录为新版本号，旧目录保持冻结，不删除、不改写；
- 根 `docs/zh/_meta.json` / `docs/en/_meta.json` 的版本入口按“最新在前”排列；
- 打包默认取 `_meta.json` 中第一个版本并**扁平化**为 `docs/<lang>/...`（应用内帮助路径稳定）；`-AllVersions -Profile full` 保留完整版本目录，供文档站发布。

## 语言分开存储

中文与英文分别存放在 `docs/zh/` 与 `docs/en/` 两个独立目录，版本号与文件路径一一对应；共享图片只在 `docs/assets/` 存一份。

## 规范入口

- 写作规范：[`_meta/style-guide.md`](./_meta/style-guide.md)
- 术语表：[`_meta/glossary-zh-en.md`](./_meta/glossary-zh-en.md)
- 外部链接槽位：[`_meta/placeholders.md`](./_meta/placeholders.md)
- 页面清单：[`_meta/doc-map.md`](./_meta/doc-map.md)
- 模板：[`_meta/templates/`](./_meta/templates/)
