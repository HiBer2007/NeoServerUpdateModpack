---
title: 客户端 bridge
description: nsum-bridge（HMCL/FLF 扩展）：NSUM 页面、整合包类型、启动注入与 Runtime 安装。
lang: zh
status: complete
updated: 2026-11-28
---

# 客户端 bridge

`nsum-bridge` 是 HMCL 的扩展（依赖 FLF API），负责把 NSUM 接入启动器：识别 `.nsumpack`、在启动前执行 Runtime 同步、提供 NSUM 管理页面。

## 能力

| 能力 | 说明 |
|---|---|
| NSUM 页面 | 安装/更新 Runtime、打开仓库、版本信息 |
| 整合包类型 | 识别 `.nsumpack` 并创建实例 |
| 启动注入 | 在 HMCL 启动流程插入 `NSumSyncTask` |
| 分支选择 | 首次启动三步选择（内嵌对话框） |
| Runtime 安装 | 从 metadata 下载并校验安装 |
| 并行下载 | Range 分片 + 单流回退 |
| 多地址选择 | 并行探测取最快，超时自动切换，可手动指定 |
| 内嵌文件缓存 | `.hmcl/nsum/cache/files/<sha256>` |
| MCLHR 集成 | 同步后投影 mod store |

## 版本与依赖

| 项 | 值 |
|---|---|
| 版本 | `0.4.6.17`（当前） |
| 依赖 | HMCL 3.17.2 / FLF API 2.x |
| Java | 与 HMCL 运行环境一致（17+） |

## 仓库地址选择

自 0.4.6.17 起，bridge 对 `repository.urls[]` 中的多个地址**并行探测**（`git ls-remote`），延迟最低的优先；每个地址独立超时，超时/失败自动换下一个。已有检出会复用原目录（`git remote set-url`），不会为每个地址克隆一份。

配置文件 `.hmcl/nsum/repo.properties`（可选）：

```properties
preference=auto
probeTimeoutSeconds=8
cloneTimeoutSeconds=180
syncTimeoutSeconds=300
```

- `preference`：`auto`（默认，最快）/ `ssh` / `https` / `http` / `manual`（保持清单顺序）/ 显式 URL；
- 同步超时后 bridge 会结束该次 Runtime 进程并尝试下一个地址；
- 系统属性 `-Dnsum.repo.preference=...`、`-Dnsum.repo.probeTimeoutSeconds=...` 等可覆盖文件（环境变量 `NSUM_REPO_PREFERENCE` 等同样可用）；
- 成功后记录 `activeRepoUrl`，下次启动优先使用，避免重复探测。

## 页面

- [使用](./usage.md)：安装、NSUM 页面操作、导入整合包；
- [任务链](./tasks.md)：启动注入与同步任务；
- [下载](./download.md)：并行 Range 下载、回退与配置。

## 数据目录

| 路径 | 内容 |
|---|---|
| `.hmcl/nsum/runtime/` | Runtime 部署 |
| `.hmcl/nsum/downloads/` | Runtime 安装包缓存 |
| `.hmcl/nsum/repos/` | 仓库克隆 |
| `.hmcl/nsum/logs/` | 同步日志 |
| `.hmcl/nsum/cache/files/` | 内嵌文件缓存 |
| `.hmcl/nsum/download.properties` | 下载器配置 |
| `.hmcl/nsum/repo.properties` | 仓库地址选择配置 |

## 相关

- [同步与启动](../../user/sync-and-launch.md)
- [故障排查](../../user/troubleshooting.md)
## 相关 FLF 仓库

- [FLF API](https://github.com/HiBer2007/FLFAPI)：HMCL 的 Fabric API 式扩展层（事件/注册表/UI/账号/下载/整合包接口），bridge 依赖其 API。
- [FLFHMCLauncher](https://github.com/HiBer2007/FLFHMCLauncher)：FLFHMCL 的外壳/高阶入口，可用于安装与环境配置。
