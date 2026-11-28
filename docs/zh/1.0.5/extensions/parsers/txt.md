---
title: TXT 解析器
description: NeoParser_TXT：.txt/.properties/.cfg 的行级合并与逐行跟踪。
lang: zh
status: complete
updated: 2026-11-27
---

# TXT 解析器

`NeoParser_TXT` 处理纯文本配置（`.txt`、`.properties`、`.cfg`），是唯一支持**行级跟踪**的解析器。

## 能力

| 项 | 值 |
|---|---|
| 能力名 | `TXT` |
| 扩展名 | `.txt`、`.properties`、`.cfg` |
| 合并模式 | `full_sync`、`line_by_line`、`no_sync` |
| 行级跟踪 | 是 |
| 优先级 | 60 |

## 行级合并

```json
{
  "sync_policies": {
    "files": {
      "config/messages.txt": {
        "mode": "partial",
        "tracked_lines": [1, 2, 5]
      }
    }
  }
}
```

- 行号从 1 开始；
- 只替换被跟踪行，其余行保留本地内容；
- 行数不一致时，超出部分按远端内容处理（见 `merge_lines` 实现）。

## 行号获取

```powershell
# 编辑器/CLI 打开文件后查看行号；或在仓库中直接按行定位
```

## 注意

- 使用 `line_by_line` 时，玩家新增/删除行会导致行号偏移，建议只跟踪头部固定行；
- 编码按 UTF-8 读取，非 UTF-8 文件可能乱码；
- `full` 对该解析器等价于整文件覆盖。

## 相关

- [解析器总览](./index.md)
- [Properties 解析器](./properties.md)
