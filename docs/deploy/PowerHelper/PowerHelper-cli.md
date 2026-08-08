# PowerHelper CLI 参考

CLI 模式仅支持单文档渲染；文档组仅提供文件列表与 TOC 映射查询。

## 命令

### render —— 单文档终端渲染

```
PowerHelper render <file.md> [--json]
```

- 输出 ANSI 终端文本（颜色/粗斜体/制表符/表格对齐）
- `--json`：stdout 输出 JSON 标记块（`data.rendered` 为转义后的终端文本），人类输出走 stderr

### toc —— 标题目录

```
PowerHelper toc <file.md> [--json]
```

- 每行：`<缩进><级别> <标题文本>`（H1 缩进 0，H2 缩进 1，依此类推）
- `--json`：`data.entries` 数组 `[{level, text}]`

### group —— 文档组列表 / TOC 映射

```
PowerHelper group <dir> [--toc] [--json]
```

- 递归扫描目录下所有 `.md`（按路径排序）；每行：`<相对路径>  |  <标题>`
- `--toc`：追加每文档的标题清单（含级别缩进）
- `--json`：`data.docs` 数组 `[{path, title, toc?}]`

## 全局选项

| 选项 | 说明 |
|------|------|
| `-h` `--help` `-help` `help` `/h` `/?` `-?` | 帮助 |
| `-v` `--version` `/v` | 版本 |
| `--json` | JSON 标记块输出（`=====JSON-BEGIN=====`/`=====JSON-END=====`），人类日志走 stderr |

## JSON 协议

```json
=====JSON-BEGIN=====
{ "category": "powerhelper", "command": "render", "data": { ... } }
=====JSON-END=====
```

| command | data |
|---------|------|
| `render` | `{file, rendered}` |
| `toc` | `{file, entries:[{level,text}]}` |
| `group` | `{dir, docs:[{path,title,toc?:[{level,text}]}]}` |

## 退出码

| 码 | 含义 |
|----|------|
| 0 | 成功 |
| 1 | 运行期错误（文件不存在 / 目录无 .md 文档） |
| 2 | 用法错误（未知命令、缺参数、GUI 目标不存在） |

> **捕获建议**：PowerHelper 为 GUI 子系统(WIN32)程序，在 PowerShell 中用
> `$var = & PowerHelper render x.md` 捕获 stdout 常显示为空（console 脱离）。
> 请直接于控制台运行，或用重定向（`> file`、Start-Process -RedirectStandardOutput）。
> CLI 崩溃（render/toc/group）走 `CrashTracker --cli` 控制台报告，不弹 GUI。

## 示例

```
> PowerHelper toc docs/deploy/CLI/CLI.md
1 CLI 命令总览
 2 info 命令
 2 flow 命令
 2 exec 命令

> PowerHelper group docs/deploy/CLI --toc
CLI.md          | CLI 命令总览
CLI-usage.md    | 基础与参数解析
CLI-info.md     | info 命令详解
 2 必配参数
 ...
```
