---
title: 客户端 bridge
description: nsum-bridge（HMCL/FLF 扩展）：NSUM 页面、整合包类型、启动注入与 Runtime 安装。
lang: zh
status: complete
updated: 2026-11-27
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
| 内嵌文件缓存 | `.hmcl/nsum/cache/files/<sha256>` |
| MCLHR 集成 | 同步后投影 mod store |

## 版本与依赖

| 项 | 值 |
|---|---|
| 版本 | `0.4.4.17`（当前） |
| 依赖 | HMCL 3.17.2 / FLF API 2.x |
| Java | 与 HMCL 运行环境一致（17+） |

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

## 相关

- [同步与启动](../../user/sync-and-launch.md)
- [故障排查](../../user/troubleshooting.md)
