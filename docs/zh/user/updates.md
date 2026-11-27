---
title: 更新
description: NSUM Runtime、bridge 与 HMCL 侧组件的版本检查、更新与回滚。
lang: zh
status: complete
updated: 2026-11-27
---

# 更新

NSUM 的版本由 metadata 站点（`https://metadata.hibernet.top`）发布，bridge 与 Runtime 各自检查。

## 组件版本

| 组件 | 版本来源 | 更新方式 |
|---|---|---|
| NSUM Runtime | metadata `NSUM.json` 的 `Latest` | HMCL NSUM 页面「安装 / 更新 Runtime」 |
| nsum-bridge | metadata `nsum-bridge.json` 的 `Latest` | 替换 `mods/` 下的 jar |
| 内置 Git | GitHub MinGit 最新版 | `exec git-update` 或 Runtime 启动时后台更新 |

## Runtime 更新

1. 打开 HMCL → NSUM 页面；
2. 若有新版本，页面提示「Runtime 有新版本」；
3. 确认后 bridge 下载 zip（支持 Range 分片多线程，失败自动回退单流）、校验 SHA-256、解压到 `.hmcl/nsum/runtime/`；
4. 更新完成后页面显示新版本。

### 手动更新

```powershell
# 查看当前版本
.\NeoServerUpdateModpack.exe info version

# 更新内置 Git
.\NeoServerUpdateModpack.exe exec git-update
```

若自动更新失败，可手动下载 `NSUM_Runtime_CLI_<版本>.zip`，解压内容放到 `.hmcl/nsum/downloads/`，再在 NSUM 页面点「从 downloads 安装」。

## bridge 更新

1. 从 metadata 站点或 GitHub Release 下载 `nsum-bridge-<版本>.jar`；
2. 关闭 HMCL；
3. 删除 `mods/` 下旧版 `nsum-bridge-*.jar`，放入新 jar；
4. 启动 HMCL，NSUM 页面显示 bridge 版本。

> bridge 版本与 FLF API/HMCL 版本有兼容要求，详见 bridge Release 说明。

## 回滚

| 场景 | 做法 |
|---|---|
| Runtime 新版本有问题 | 保留旧版 zip，删除 `.hmcl/nsum/runtime/` 后重装旧版 |
| bridge 新版本不兼容 | 换回旧 jar |
| 仓库内容被误同步 | 在实例目录使用 Git/备份恢复，或重新同步指定分支 |

## FLFHMCL 更新

<!-- slot:FLFHMCL_UPDATE -->

> **待补**：FLFHMCL 升级说明完成后在此补充链接。

## 版本号说明

NSUM 使用双版本号：

- 外部版本 `x.y.z`：发布版本（如 `1.0.5`）；
- 内部版本 `a.b.c`：迭代版本（如 `0.1.11`）；
- 完整版本：`1.0.5.0.1.11`。

查看：

```powershell
.\NeoServerUpdateModpack.exe info version
```
