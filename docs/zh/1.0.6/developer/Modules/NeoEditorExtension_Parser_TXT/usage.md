---
title: NeoEditorExtension_Parser_TXT 使用
description: NeoEditorExtension_Parser_TXT 的接口与注册方式。
lang: zh
status: complete
updated: 2026-11-27
---

# NeoEditorExtension_Parser_TXT 使用

## 快速开始

由编辑器在启动时扫描 `editor/extension/parsers/` 自动注册，无需手动调用。

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `createEditor` | `QWidget* createEditor(QWidget* parent)` | 创建编辑控件 |
| capability/register | 见 `IConfigEditorExtension` 头文件 | 能力声明/注册 |

## 注意事项

- 工厂必须导出；
- 扩展与运行时插件应同版本发布；
- 校验失败不要写坏原文件（保留原文并提示）；
- 修改接口后所有扩展需重建。

## 相关

- [README.md](./README.md)