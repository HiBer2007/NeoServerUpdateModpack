# 导出格式详解

本文详细说明 NSUM 构建工具支持的三种导出格式：内部结构、适用场景与额外字段。

## 概览

| 格式 | 产物 | 扩展名 | 导入方式 |
|------|------|--------|----------|
| MCBBS 整合包 | 单个压缩包 | `.zip` | PCL2 / HMCL / 通用启动器直接导入 |
| Modrinth | 单个压缩包 | `.mrpack` | Modrinth App / 支持 mrpack 的启动器 |
| HMCL 工作区 | 目录（不打包） | — | HMCL 同步到游戏工作目录 |

> 构建产出的整合包**不包含 Minecraft 本体与加载器**——启动器会在导入时自动补装。

## MCBBS 整合包（.zip）

适用于 PCL2、HMCL 等国产启动器与通用分发场景。

### 包内结构

```
<整合包>_modpack.zip
├── manifest.json          # 整合包清单（名称/版本/依赖/文件列表）
├── mcbbs.packmeta         # MCBBS 元数据（启动器识别）
└── overrides/             # 覆盖文件（mods/config 等实际文件）
```

### 额外字段

| 字段 | 必填 | 说明 |
|------|------|------|
| `name` | 是 | 整合包名称 |
| `version` | 是 | 整合包版本号 |

## Modrinth 整合包（.mrpack）

适用于 Modrinth App 及支持 Modrinth Modpack Format 的启动器。

### 包内结构

```
<整合包>.mrpack
├── modrinth.index.json   # 格式索引（格式版本/依赖/文件哈希清单）
└── overrides/            # 覆盖文件
    └── (可选) client-overrides/  server-overrides/
```

### 额外字段

| 字段 | 必填 | 说明 |
|------|------|------|
| `name` | 是 | 整合包名称 |
| `version` | 是 | 整合包版本号 |
| `summary` | 否 | 一句话简介 |

### 格式规范

依据 [Modrinth Modpack Format](https://support.modrinth.com/en/articles/8802351-modrinth-modpack-format-mrpack)：
文件条目使用 SHA-1 哈希；构建器输出 `overrides/`（文件以相对路径记录）。

## HMCL 工作区（目录）

适用于将整合包内容直接同步进 HMCL 的游戏工作目录（`<HMCL 目录>/versions/<版本>`）。

### 输出结构

```
<导出目录>/
├── version.json          # 版本清单（HMCL 识别）
├── mods/                 # 模组文件（指针文件在构建期已还原）
├── config/               # 配置（按同步策略合并）
├── [save]/serverconfig/  # 服务端配置（若配置了 serverconfig 规则）
└── .minecraft/           # 其他游戏文件
```

不打包成单一文件——导出目录即目标工作目录，适合直接指向 HMCL 版本目录。

## 格式选择建议

| 场景 | 推荐格式 |
|------|----------|
| 分发给玩家（PCL2/HMCL 导入） | MCBBS `.zip` |
| 发布到 Modrinth 平台 | Modrinth `.mrpack` |
| 自己/服务器使用，与 HMCL 工作区同步 | HMCL 工作区（目录） |

## 常见问题

- **导出目录已存在同名文件？** 构建器会覆盖写入；建议使用独立目录。
- **额外字段必填提示**：mcbbs/modrinth 的 `name`/`version` 必填，缺一不可。
- **hmcl 模式没有额外字段**：直接选择导出目录即可。
