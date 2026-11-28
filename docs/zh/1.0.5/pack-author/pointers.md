---
title: 指针
description: 指针文件的创建、转换、解析、缓存与失败排查。
lang: zh
status: complete
updated: 2026-11-27
---

# 指针

指针让大文件（jar）不进 Git 也能被同步：仓库里只放一个以 SHA-256 命名的占位文件，客户端按需下载并校验。

## 位置

```text
branch_config/<sha256>.pointer
branch_config/<branch>.json
  ├─ file_manifest: "mods/a.jar" → "<sha256>"
  └─ pointer_files: "<sha256>" → { resolver, metadata }
```

> 指针文件本身不含目标路径；路径由 `file_manifest` 关联。

## 创建

编辑器：右键文件 →「转换为指针」，选择 resolver（DirectURL/Modrinth）。

CLI：

```powershell
NeoWorkspaceEditor.exe exec pointer-add --path mods/a.jar --sha256 <64-hex> `
    --resolver direct_url --url https://example.com/a.jar --branch base --repo <仓库>
```

## 解析

1. 构建发现 `mods/a.jar` 缺失/哈希不符；
2. 查 `file_manifest` 得到 sha；
3. 查缓存 `<cache>/<sha>`；命中则校验后复制；
4. 未命中：用 `resolver` 解析 URL 下载 → 校验 → 入缓存。

## 缓存与校验

- 构建侧：`.minecraft/versions/.cache/<sha256>`；
- 客户端内嵌文件：`.hmcl/nsum/cache/files/<sha256>`；
- 每次使用前重算 SHA-256（零信任）。

## 失败排查

| 现象 | 原因 | 处理 |
|---|---|---|
| `No resolver found` | 解析器缺失 / resolver 名错 | 检查 `pointers/` 与 `resolver` 字段 |
| `SHA-256 hash mismatch` | 缓存损坏/URL 内容变了 | 删除缓存重试；更新指针 |
| 404 | 直链失效 | 换 URL 或改用 Modrinth |
| 转换后文件消失 | 指针化会移动文件到缓存 | 正常；确认 `file_manifest` 已写入 |

## 注意

- 指针哈希必须与内容一致；改文件后要重新生成；
- `pointer_files` 与实际 `.pointer` 文件必须同步（编辑器/CLI 会一起写）；
- 不要手工编辑 `.pointer` 的 sha 字段；
- Modrinth 指针固定 `version_id` 以免内容漂移。

## 相关

- [同步规则](./sync-rules.md)
- 指针解析器（DirectURL/Modrinth）：见在线拓展文档
