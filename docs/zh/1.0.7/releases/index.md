---
title: 版本说明
description: NSUM 各版本的主要变化与发布链接。
lang: zh
status: complete
updated: 2026-11-27
---

# 版本说明

完整发布记录见 [GitHub Releases](https://github.com/HiBer2007/NeoServerUpdateModpack/releases)。

## 1.0.7（2026-11-28）

- **仓库地址自动选优**：多地址时逐地址 `git ls-remote` 探测（bridge 并行），优先使用延迟最低的地址（bridge 与运行时 CLI）；
- **超时自动切换**：每个地址独立超时（默认 8s 探测 / 120s 克隆 / 300s 同步），超时或失败自动尝试下一个；
- **手动指定**：`--repo-preference auto|ssh|https|http|manual`（运行时），bridge 支持 `.hmcl/nsum/repo.properties` 的 `preference=auto|ssh|https|http|manual|<URL>`；编辑器主地址下拉与直接输入 URL 仍可手动指定；
- 主程序仓库页新增「地址优先级」下拉（自动最快 / 优先 SSH / 优先 HTTPS / 优先 HTTP），克隆 60s 超时后自动切换。

## 1.0.6（2026-11-28）

- **多仓库地址**：`workspace.json` 升级为 `version: 1.1`，新增 `git.remotes[]`（SSH/HTTPS/HTTP，主地址排第一）；`.nsumpack` 升级为 `pack_format` / `schema` **3**，新增 `repository.urls[]`，旧 `url` 继续写入；
- **SSH 密钥管理**：主程序仓库输入框右侧「SSH 密钥...」、编辑器 Git 菜单、两端 `exec git-ssh-key`（list/generate/public-key/test/configure）；自动注入 `GIT_SSH_COMMAND`，修复内置 MinGit 无 SSH 配置导致的 300 秒克隆超时；
- **仓库地址管理**：`exec repo-remotes` 维护三种地址；编辑器「仓库设置」支持 SSH/HTTPS/HTTP 三个输入框与主地址选择；
- **系统 Git 优先**：`install.conf` 的 `use_system_git=true` 现在真正跳过内置 Git；
- 配套 bridge `0.4.5.17`：按 `repository.urls` 顺序探测/回退，已有检出用 `git remote set-url` 复用目录。

## 1.0.5（2026-11-27）

- 编辑器 CLI：`info` / `exec` / `help`，含内置 Git 命令，供 AI/脚本操作仓库；
- `.nsumpack` 改为清单型 `pack_format 2`：默认只含 `nsum.index.json`，附带文件需 `--embed-patterns` 显式声明并登记来源提交；
- 配套 bridge `0.4.4.17`：按 `embedded[]` 安装并以 SHA-256 缓存；
- 文档体系重构（四板块、中英双语、Nextra-ready）。

## 1.0.4（2026-11-26）

- 运行时与 HIC 的依赖下载支持多线程 Range（失败回退单流）；
- bridge 并行下载器。

## 1.0.3

- 修复中文路径导致的崩溃（UTF-8 argv/路径全链路）；
- 内置 Git 定位与自动更新完善。

## 更早版本

见 GitHub Releases 页面与对应 tag。

## 相关

- [更新](../user/updates.md)
- [发布流程](../developer/release.md)
