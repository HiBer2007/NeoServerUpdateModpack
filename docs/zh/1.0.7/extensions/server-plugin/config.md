---
title: 服务端配置
description: MCDR config.yml 的 handler 改写与服务器启动参数。
lang: zh
status: complete
updated: 2026-11-27
---

# 服务端配置

## handler 映射

MCDR 的 `config.yml` 需要按服务器核心设置 `handler`：

| 核心类型 | handler |
|---|---|
| paper / spigot / purpur / folia | `bukkit` |
| fabric / quilt | `fabric` |
| forge / neoforge | `forge` |
| velocity | `velocity` |
| bungeecord / waterfall | `bungeecord` |

插件在安装/切换核心时自动改写该字段。

## 其他配置

| 键 | 说明 |
|---|---|
| `working_directory` | 服务器目录 |
| `start_command` | 启动命令（`java -jar server.jar` 等） |
| `handler` | 见上表 |
| `rcon` | 如需 RCON，开启并设置密码 |

## 启动参数建议

```text
-Xms4G -Xmx4G -XX:+UseG1GC -jar server.jar nogui
```

- 内存按玩家数与模组量调整；
- Windows 路径含空格时用引号；
- 服务端 Java 版本需匹配 Minecraft/核心要求。

## 排错

| 现象 | 检查 |
|---|---|
| MCDR 启动后立刻退出 | `start_command` 与 `working_directory` |
| 插件未加载 | `plugins/` 路径与 plugin.yml |
| 核心类型识别错误 | `handler` 是否匹配 |

## 相关

- [服务端插件总览](./index.md)
- [安装](./install.md)
