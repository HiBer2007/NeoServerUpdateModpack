---
title: 发布
description: 整合包作者视角的发布清单：构建、自检、分发与客户端兼容。
lang: zh
status: complete
updated: 2026-11-27
---

# 发布

作者发布的是**整合包配方**（仓库 + `.nsumpack`），不是游戏本体。版本发布流程见线上开发者文档（发布 runbook）。

## 发布清单

1. **冻结内容**：合并所有改动，`git status` 干净；
2. **校验仓库**：

```powershell
NeoWorkspaceEditor.exe exec validate --repo <仓库>
```

3. **预览构建**：

```powershell
NeoServerUpdateModpack.exe info preview --repo <仓库> --modpack base --format nsum --json
```

4. **更新版本号**：`workspace.json` 的 `export.metadata.version`（或导出时填写）；
5. **导出**：

```powershell
NeoServerUpdateModpack.exe exec build --repo <仓库> --modpack base --git-branch main `
    --format nsum --export release.nsumpack
```

6. **自检**：

| 检查 | 期望 |
|---|---|
| 包体积 | 无内嵌 ≈ 1 KB |
| `repository.commit` | 等于当前 release 提交 |
| `files[]` 数量 | 与预览一致 |
| `embedded[]` | 仅预期文件 |

7. **分发**：Release/网盘/服务器公告，附仓库地址与分支说明。

## 版本策略

- 内容更新 → 递增整合包 `version`；
- 不兼容变更（分支/加载器/目录）→ major/minor 并通知玩家；
- 每次发布保留 `.nsumpack` 与对应 Git tag。

## 客户端兼容

| 项 | 要求 |
|---|---|
| Runtime | `runtime.minVersion` |
| bridge | 与包格式兼容（pack_format 2 需 ≥0.4.4.17） |
| HMCL/FLF | 与 bridge Release 说明匹配 |
| Java | 与加载器/模组要求一致 |

## 回滚

- 仓库：`git revert` 或回到上一个 tag；
- 玩家：重新导入上一版 `.nsumpack` 并重新同步；
- 避免删除历史 tag。

## 相关

- [构建与导出](./build-and-export.md)
- [最佳实践](./best-practices.md)
