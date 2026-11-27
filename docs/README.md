# NSUM 文档源

本目录是 NSUM 文档的唯一事实源，按 **四板块 × 双语** 组织，结构已按 Next.js + Nextra 的约定设计（每目录一份 `_meta.json`、`index.md` 作为板块首页、kebab-case slug、相对链接）。

## 目录

```text
docs/
├─ zh/                      # 中文（canonical）
│  ├─ user/                 # 板块 1：主程序使用
│  ├─ pack-author/          # 板块 2：整合包作者（仅安装 Editor 时随包释放）
│  ├─ developer/            # 板块 3：开发者（架构/框架/ABI/Modules/流程/ADR）
│  └─ extensions/           # 板块 4：拓展功能（分组）
├─ en/                      # 英文（与 zh 逐页镜像）
├─ assets/                  # 语言中立：diagrams / screenshots / images
├─ _meta/                   # 写作规范、模板、术语表、槽位登记（不发布）
└─ releases/                # 版本说明归档
```

## 语言分开存储

中文与英文分别存放在 `docs/zh/` 与 `docs/en/` 两个独立目录，文件路径、文件名、锚点一一对应；共享图片只在 `docs/assets/` 存一份。

## 规范入口

- 写作规范：[`_meta/style-guide.md`](./_meta/style-guide.md)
- 术语表：[`_meta/glossary-zh-en.md`](./_meta/glossary-zh-en.md)
- 外部链接槽位：[`_meta/placeholders.md`](./_meta/placeholders.md)
- 页面清单：[`_meta/doc-map.md`](./_meta/doc-map.md)
- 模板：[`_meta/templates/`](./_meta/templates/)

## 校验与打包

```powershell
pwsh -NoProfile -File tools/docs_check.ps1

# 发布包：user + 隐藏的 Editor 载荷（_editor）
pwsh -NoProfile -File tools/build_docs.ps1 -Profile with-editor -Out K:\tmp\nsum-docs

# 完整站点内容
pwsh -NoProfile -File tools/build_docs.ps1 -Profile full -Out K:\tmp\nsum-docs-full
```

发布包只包含 `user`；`pack-author` 以 `_editor/` 隐藏载荷随包分发，`NeoWorkspaceEditor.exe` 首次启动时提升为正式文档。
