---
title: NeoBuild 使用
description: BuildEngine 的初始化、构建、预览与指针/导出 API。
lang: zh
status: complete
updated: 2026-11-27
---

# NeoBuild 使用

## 快速开始

```cpp
#include <build_engine.h>

NeoBuild::BuildEngine engine;
engine.init(workspaceDir, cacheDir, outputBaseDir, "exporters");
auto result = engine.build("base", &progress, &cancel, "main");
```

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `BuildEngine::init` | `bool init(const std::string& workspacePath, const std::string& cacheDir = "", const std::string& outputBaseDir = "", const std::string& exportersDir = "")` | 加载 workspace/插件 |
| `BuildEngine::build` | `BuildResult build(const std::string& branchName, NeoCore::IBuildProgress* progress = nullptr, NeoCore::CancelToken* cancelToken = nullptr, const std::string& gitBranch = "")` | 构建 |
| `BuildEngine::previewStructure` | `nlohmann::json previewStructure(const std::string& format, ...)` | 预览结构 |
| `PointerDownloader::resolve` | 见 `pointer_downloader.h` | 解析指针 |
| `ModpackExporter::scanExporters` | 见头文件 | 扫描导出器 |
| `ServerConfigSync::sync` | 见头文件 | L3 同步 |
| `SyncPolicyExecutor::apply` | 见头文件 | 策略应用 |

## 典型用法

### 构建并导出

```cpp
NeoBuild::BuildEngine engine;
engine.init(repo, cache, buildBase, "exporters");
auto r = engine.build("base", &progress, &cancel, "main");
if (r.success) { exporter->export_modpack(buildDir, "out.nsumpack", meta); }
```

### 只预览

```cpp
engine.init(repo, cache, tempDir, "exporters");
engine.build("base", nullptr, &cancel, "main");
auto tree = engine.previewStructure("nsum");
```

## 注意事项

- `build` 第四参是 **Git 分支**，不是整合包分支；
- 本地仓库无 remote 时 fetch 会跳过；
- 指针下载失败会终止构建（日志有 resolver 名称）；
- 预览目录放临时目录，避免残留发布包。

## 相关

- [README.md](./README.md)
- [指针系统](../../architecture/pointer-system.md)