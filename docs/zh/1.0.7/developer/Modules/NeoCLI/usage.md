---
title: NeoCLI 使用
description: CLI 命令分发 API 与新增命令的落点。
lang: zh
status: complete
updated: 2026-11-27
---

# NeoCLI 使用

## 快速开始

```cpp
#include <arg_parser.h>
#include <cli_dispatcher.h>

NeoCLI::ArgParser parser;
NeoCLI::CliCommand cmd = parser.parse(argc, argv);
NeoCLI::CliDispatcher dispatcher;
return dispatcher.dispatch(cmd);
```

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `ArgParser::parse` | `CliCommand parse(int argc, char* argv[])` | 解析参数（按值返回） |
| `CliCommand` | struct | category/verb/options |
| `CliDispatcher::dispatch` | `int dispatch(const CliCommand&)` | 执行 |
| `CliOutput::jsonBlock` | `static void jsonBlock(const nlohmann::json& value)` | 输出 JSON 块 |

## 典型用法

### 新增 info 命令

1. 在 `ArgParser` 接受 verb；
2. 在 `CliDispatcher` 增加 `cmdXxx()`；
3. 用 `CliOutput::jsonBlock()` 输出 `{category, command, data}`；
4. 更新 `docs/zh/user/cli.md` 与英文页。

### 构建命令

```cpp
// 内部等价
dispatcher.dispatch(cmd); // exec build --repo ... --modpack ...
```

## 注意事项

- `--json` 时 stdout 只能有 JSON 块，日志必须走 stderr；
- 退出码：0/1/2；
- 构建必须传 Git 分支；
- `flow gui` 不在 NeoCLI 库内实现：宿主 `src/main.cpp` 识别后交给 `GUIWorker::WizardWindow`；NeoCLI 的 `dispatchFlow` 只负责 `console`。

## 相关

- [README.md](./README.md)
- [CLI 参考](../../../user/cli.md)
