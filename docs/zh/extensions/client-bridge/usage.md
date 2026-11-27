---
title: bridge 使用
description: 安装 nsum-bridge、使用 NSUM 页面、导入 .nsumpack 与选择分支。
lang: zh
status: complete
updated: 2026-11-27
---

# bridge 使用

## 安装

1. 确认 HMCL 与 FLF 运行时已安装；
2. 把 `nsum-bridge-<版本>.jar` 放入 HMCL 工作目录 `mods/`；
3. 启动 HMCL，左侧出现 NSUM 分类。

## NSUM 页面

| 操作 | 说明 |
|---|---|
| 安装/更新 Runtime | 从 metadata 下载 `NSUM_Runtime_CLI_<版本>.zip`，校验后解压到 `.hmcl/nsum/runtime/` |
| 从 downloads 安装 | 使用已放入 `.hmcl/nsum/downloads/` 的 zip（离线/手动下载） |
| 打开仓库 | 用编辑器打开当前实例对应的仓库 |
| 版本信息 | 显示 bridge 与 Runtime 版本 |

## 导入 .nsumpack

1. HMCL「安装整合包」或拖入 `.nsumpack`；
2. 类型识别为 **NSUM 整合包**；
3. 选择实例目录并确认；
4. bridge 写入 `<实例>/.NSUM/instance.json`（仓库、分支、版本、`packFormat`、`sourceCommit` 等）；
5. 若包带 `embedded[]`，按 SHA-256 缓存安装。

## 分支选择

首次启动时在任务对话框内选择：

1. Git 分支；
2. 整合包分支；
3. 确认。

选择被保存；可在实例设置中重选。

## 实例标记

```json
{
  "repo": "https://github.com/owner/repo.git",
  "branch": "client",
  "gitBranch": "main",
  "packFormat": 2,
  "sourceCommit": "7ee64d8...",
  "sourceGitBranch": "main",
  "defaultBranch": "main",
  "branchType": "client",
  "launchOnSyncFailure": false
}
```

## 注意

- 删除 bridge jar 后 NSUM 页面消失，但实例目录与仓库缓存保留；
- 不要手动修改 `.NSUM/instance.json` 的仓库地址（会导致同步到错误仓库）；
- Runtime 更新不影响实例内容。

## 相关

- [任务链](./tasks.md)
- [下载](./download.md)
