---
title: GitIgnoreMarkup 使用
description: 解析/序列化 .gitignore 标记的 API、往返示例，以及在编辑器里打开 GitIgnoreDialog 的方式。
lang: zh
status: complete
updated: 2026-11-27
---

# GitIgnoreMarkup 使用

## 快速开始

```cpp
#include <gitignore_markup.h>
#include <QFile>
#include <QTextStream>

QFile f(".gitignore");
if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString text = QString::fromUtf8(f.readAll());
    f.close();

    QVector<GitIgnoreMarkup::Line> lines = GitIgnoreMarkup::parseLines(text);
    for (const auto& line : lines) {
        if (line.kind == GitIgnoreMarkup::LineKind::DisabledRule) {
            // 这是「临时取消」的规则（#! 前缀），普通文本解析分不出来
        }
    }

    QFile out(".gitignore");
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream(&out) << GitIgnoreMarkup::serialize(lines);
    }
}
```

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `parseLines` | `QVector<Line> parseLines(const QString& content);` | 文本 → 行列表；空行丢弃；`Rule`/`DisabledRule` 的 `checked` 相应置位 |
| `serialize` | `QString serialize(const QVector<Line>& lines);` | 行列表 → 文本；未勾选规则写 `#! `；组首尾成对 |
| `isComment` | `bool isComment(const QString& trimmed);` | 是否 `# ` 普通注释 |
| `isLineLabel` | `bool isLineLabel(const QString& trimmed);` | 是否 `#> ` 下一行标注 |
| `isGroupBegin` | `bool isGroupBegin(const QString& trimmed);` | 是否 `#{ ` 组开始 |
| `isGroupEnd` | `bool isGroupEnd(const QString& trimmed);` | 是否 `#}` 组结束 |
| `isDisabledRule` | `bool isDisabledRule(const QString& trimmed);` | 是否 `#! ` 临时取消规则 |
| `makeRule` | `Line makeRule(const QString& pattern, bool checked);` | 构造规则行 |
| `makeDisabledRule` | `Line makeDisabledRule(const QString& pattern);` | 构造未勾选规则（`#! ` 前缀） |
| `makeLineLabel` | `Line makeLineLabel(const QString& text);` | 构造 `#> ` 标注 |
| `makeGroupBegin` | `Line makeGroupBegin(const QString& title);` | 构造 `#{ ` 组开始 |
| `makeGroupEnd` | `Line makeGroupEnd();` | 构造 `#}` 组结束 |
| `makeComment` | `Line makeComment(const QString& text);` | 构造 `# ` 注释 |
| `prefixFor` | `QString prefixFor(LineKind kind);` | 返回该类型的标准前缀 |
| `GitIgnoreDialog::GitIgnoreDialog` | `explicit GitIgnoreDialog(const QString& repoRoot, int initialTab = 0, QWidget* parent = nullptr);` | `initialTab` 0 = 图形化，1 = 直接编辑 |
| `GitIgnoreDialog::saved` | `void saved(const QString& absPath);` | 保存完成后发出（宿主负责 `git add` / 刷新面板） |

`Line` 的字段：`LineKind kind`、`QString prefix`（标准前缀，不含内容）、`QString text`（去前缀内容）、`bool checked`（`Rule`/`DisabledRule` 的勾选态）。

## 典型用法

### 标记格式速查

```text
# 普通注释（不绑定任何行）
#> 服务器核心文件        <- 下一行标注，只作用于紧随其下的那条规则
#{ 服务端配置            <- 组开始
server.properties
config/paper-global.yml
#}                       <- 组结束
#! logs/                 <- 临时取消忽略（未勾选态，可重新勾选恢复）
*.jar
```

### 勾选 / 取消勾选一条规则

```cpp
QVector<GitIgnoreMarkup::Line> lines = GitIgnoreMarkup::parseLines(text);

for (auto& line : lines) {
    if (line.kind == GitIgnoreMarkup::LineKind::Rule && line.text == "logs/") {
        line.kind = GitIgnoreMarkup::LineKind::DisabledRule;
        line.checked = false;
        line.prefix = GitIgnoreMarkup::prefixFor(line.kind);   // "#! "
    }
}
const QString serialized = GitIgnoreMarkup::serialize(lines);
```

反向（恢复勾选）只需把 `kind` 改回 `Rule`、`checked = true`，序列化会自动去掉 `#! ` 前缀。

### 新加一组带标注的规则

```cpp
QVector<GitIgnoreMarkup::Line> lines;
lines << GitIgnoreMarkup::makeComment("构建产物不入库");
lines << GitIgnoreMarkup::makeGroupBegin("构建输出");
lines << GitIgnoreMarkup::makeLineLabel("版本文件由构建生成");
lines << GitIgnoreMarkup::makeRule("version.json", true);
lines << GitIgnoreMarkup::makeRule("build/", true);
lines << GitIgnoreMarkup::makeGroupEnd();

const QString text = GitIgnoreMarkup::serialize(lines);
```

序列化保证：孤立的组结束标记会被丢弃；末尾未闭合的组自动补 `#}`，因此往返多次结果稳定。

### 在编辑器里打开对话框

```cpp
auto* dlg = new GitIgnoreMarkup::GitIgnoreDialog(
    QString::fromStdString(repoRoot),  // 仓库根（对话框内部定位 <repoRoot>/.gitignore）
    /*initialTab=*/0,
    this);
dlg->setAttribute(Qt::WA_DeleteOnClose);
connect(dlg, &GitIgnoreMarkup::GitIgnoreDialog::saved, this,
        [this](const QString& absPath) {
            // 宿主收尾：暂存并刷新 Git 面板
            gitPanel_->refresh();
        });
dlg->show();
```

## 注意事项

- **不要手写前缀字符串**：`#!`/`#>`/`#{`/`#}` 的前缀以 `prefixFor` 与 `make*` 为准，硬编码空格差异会让解析回退成普通注释。
- **空行不保留**：`parseLines` 丢弃空行、`serialize` 不输出空行——不要指望往返保留用户原有的空行排版。
- **`#> ` 只绑定紧随其下的一行**：中间插入空行或组标记会改变语义，序列化后标注仍紧跟规则行。
- **组标记必须成对**：孤立 `#}` 会被丢弃；手工拼装 `Line` 列表时记得补 `makeGroupEnd()`。
- **`checked` 只对规则行有意义**：注释/标注/组标记的 `checked` 不参与序列化。
- **保存不等于入库**：`saved` 信号只表示文件已写回，`git add`、提交与面板刷新由宿主完成。
- **对话框依赖 Qt Widgets 与 HiBerGUILibrary**：只做纯文本解析的宿主（CLI）只 include `gitignore_markup.h`，不要 include `gitignore_dialog.h`，否则会被拖入 Widgets 依赖。
- **与高亮保持一致**：编辑器里的 `HiBerGUI::GitIgnoreHighlighter` 按同一套前缀着色，改标记约定时两处要一起改。

## 相关文档

- [README.md](./README.md)
- [仓库结构与 .gitignore](../../../pack-author/repo-structure.md)
- [编辑器扩展](../../../extensions/editor/index.md)
