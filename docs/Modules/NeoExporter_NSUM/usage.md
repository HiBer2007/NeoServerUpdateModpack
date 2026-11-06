# NeoExporter_NSUM 使用文档

## 快速开始

```powershell
# 打包一个 NSUM 整合包（客户端 HMCL 需安装 nsum-bridge）
NeoServerUpdateModpack.exe exec build `
    --repo <仓库路径或 URL> --modpack <整合包分支> [--git-branch <git 分支>] `
    --format nsum --export <输出 zip 路径>
```

```powershell
# 预览包内结构
NeoServerUpdateModpack.exe info preview --repo <仓库> --modpack <分支> --format nsum --json
```

GUI：向导 → 整合包 → 导出类型选「NSUM 整合包 (.zip)」→ 选择输出文件 → 构建。

## 公共 API（IModpackExporter）

| 方法 | 实现 |
|------|------|
| `format_name()` | `"nsum"` |
| `file_extension()` | `".zip"` |
| `format_description()` | `"NSUM 整合包（HMCL 经 NSUM Bridge 接入服务器同步）"` |
| `build_modpack(target, progress, cancel)` | `BuildEngine::init(wsJson, cache_dir, output_path)` → `build(branch, progress, cancel, target.git_branch)` |
| `export_modpack(build_dir, output_path, metadata)` | 写 `nsum.modpack.json` + `overrides/**`（排除 `save/`） |
| `preview_structure(build_dir, metadata, target_dir="")` | `nsum.modpack.json` + `overrides/**` |

## 客户端侧（nsum-bridge）

1. HMCL 安装 FLFHMCL + `nsum-bridge` 模组；
2. 在 HMCL「NSUM」页安装 NSUM Runtime（`.hmcl/nsum/runtime/`）；
3. 导入 `<name>.zip`：创建实例并写 `.NSUM/instance.json`（仓库/分支/版本信息）；
4. 每次启动走 `nsum-runtime` 策略：先 `Runtime exec build --format hmcl --export <实例目录>` 同步，再交回 HMCL 启动。

## 注意事项

- `instance.repo` 依赖 workspace.json 的 `git.remote`；仓库没有 remote 时会回落成本地路径（只适合同机测试）。
- 包内 `overrides/` 是首装兜底内容，正常更新以 Runtime 同步为准。
- `save/`（L3 存档 serverconfig 的规则与源）不会进入包体。
- `version.json` 会随 overrides 一起打进包内，是 NSUM → FLFHMCL 的元数据契约，客户端侧不要改写。
