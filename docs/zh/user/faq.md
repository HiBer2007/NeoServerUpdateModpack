---
title: 常见问题
description: NSUM 安装、导入、同步、启动与更新中的常见问题速查。
lang: zh
status: complete
updated: 2026-11-27
---

# 常见问题

## 安装

**Q：NSUM 页面在哪里？**
A：安装 nsum-bridge 后，HMCL 左侧栏出现 NSUM 分类与页面。

**Q：必须装 Editor 吗？**
A：不需要。Editor 只给整合包作者使用；安装器里不勾选就不会释放作者文档与编辑器。

**Q：需要自己装 Git 吗？**
A：不需要。Runtime 内置 Git（MinGit），`exec git-update` 可安装/更新。

## 导入与分支

**Q：`.nsumpack` 为什么这么小？**
A：它是清单型整合包，默认只含仓库信息与文件索引，文件由 Runtime 按仓库同步；只有显式声明才内嵌文件。

**Q：启动时让我选两个分支，选哪个？**
A：Git 分支是仓库代码分支（如 `main`）；整合包分支是内容分支（如 `base`/`client`）。不确定时按服务器说明选择，选错可重选。

**Q：能同时装多个 NSUM 整合包吗？**
A：可以，每个实例独立，共用 `.hmcl/nsum/repos` 与缓存。

## 同步

**Q：首次同步很慢？**
A：首次需要下载全部模组；后续只同步差异。可检查网络/代理。

**Q：同步失败会怎样？**
A：默认中止启动并显示错误；日志在 `.hmcl/nsum/logs/`。

## 启动

**Q：同步成功但游戏崩溃？**
A：检查 Java 版本、游戏版本/加载器是否与服务器一致；查看 HMCL 日志与 `crash-reports`。

**Q：MCLHR 是什么？**
A：mod store 投影工具，同步后把共享 mod 以硬链接方式放进实例 `mods/`。

## 更新

**Q：更新 Runtime 会丢实例吗？**
A：不会。Runtime 在 `.hmcl/nsum/runtime/`，实例内容独立。

**Q：内置 Git 更新失败？**
A：检查网络；也可手动运行 `exec git-update` 重试。

## FLFHMCL 兼容

<!-- slot:FLFHMCL_COMPAT -->

> **待补**：FLFHMCL/HMCL 版本兼容矩阵完成后在此补充链接。

**Q：HMCL 版本有要求吗？**
A：bridge 0.4.x 面向 HMCL 3.17.2 / FLF API 2.x；旧 bridge 可能要求更低版本，见 Release 说明。

## 其他

**Q：日志在哪？**
A：Runtime 日志在运行目录（`builder.log`）；bridge 同步日志在 `.hmcl/nsum/logs/`；HMCL 日志在 HMCL 自己的日志目录。

**Q：如何反馈问题？**
A：附上 Runtime 版本（`info version`）、HMCL 版本、同步日志与错误截图。
