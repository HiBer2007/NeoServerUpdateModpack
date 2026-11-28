---
title: DirectURL 解析器
description: NeoPointer_DirectURL：按指针 metadata 中的 URL 直接下载。
lang: zh
status: complete
updated: 2026-11-27
---

# DirectURL 解析器

`NeoPointer_DirectURL` 是最简单的指针解析器：直接使用指针 `metadata.url` 下载。

## 能力

| 项 | 值 |
|---|---|
| 解析器名 | `direct_url` |
| 元数据字段 | `url`（必填）、`size`（可选） |
| 支持协议 | `https://`（推荐）、`http://` |

## 指针文件

```json
{
  "resolver": "direct_url",
  "metadata": {
    "url": "https://cdn.example.com/mods/example-1.2.3.jar"
  },
  "sha256": "<64 hex>",
  "size": 123456
}
```

## 创建

编辑器：右键文件 →「转换为指针」，自动计算 SHA-256、移动文件到缓存并选择解析器（默认 DirectURL）。

CLI：

```powershell
NeoWorkspaceEditor.exe exec pointer-add --path mods/example.jar --sha256 <64-hex> `
    --resolver direct_url --url https://cdn.example.com/example.jar --branch base --repo <仓库>
```

## 下载行为

- 优先并行 Range 下载（大小 ≥ 阈值且服务器支持 `Accept-Ranges`）；
- 并行失败/取消自动回退单流；
- 下载完成校验 `sha256` 与 `size`；
- 校验失败删除文件并报错，不会写入实例。

## 限制

- URL 必须长期稳定；直链失效后指针无法解析；
- 需要鉴权的地址不支持（不会带凭据）；
- 不支持本地文件协议（本地文件请直接放入仓库或使用 `custom_mods`）。

## 相关

- [指针解析器总览](./index.md)
- [指针（作者视角）](../../pack-author/pointers.md)
