# 术语表（中英对照）

> 全站统一译法。新增术语先在本表登记，再在正文使用。代码/CLI 字段保持英文原样。

| 中文 | English | 说明 |
|---|---|---|
| 仓库 | repository | NSUM 管理的 Git 仓库（含 `workspace.json`） |
| 工作区 | workspace | 仓库根的 `workspace.json` 配置及其语义 |
| 整合包 / 整合包分支 | modpack / modpack branch | `workspace.branches[]` 中的构建分支（非 Git 分支） |
| Git 分支 | Git branch | `git_branch`，checkout 用 |
| 分支继承 | branch inheritance | 子分支合并父分支文件 |
| 父分支 | parent branch | `branches[].parent` |
| 构建 | build | 按整合包分支生成输出目录 |
| 导出 | export | 把构建产物打包为 `.nsumpack`/`.mrpack`/MCBBS zip |
| 指针 / 指针文件 | pointer / pointer file | 以 SHA-256 占位、按需下载的文件标记 |
| 指针解析器 | pointer resolver | 把指针元数据解析为下载 URL 的插件 |
| 文件清单 | file manifest | `file_manifest`，相对路径 → SHA-256 |
| 缓存 | cache | 内容寻址（SHA-256）的本地文件缓存 |
| 同步 | sync | 把仓库内容同步到实例目录 |
| 同步层级 L1/L2/L3 | sync tiers L1/L2/L3 | L1 基础分支、L2 整合包分支、L3 存档 serverconfig |
| 同步策略 | sync policy | `sync_policies`，目录/文件维度的合并规则 |
| 文件标记 | file marker | `delete` / `override` 分支级标记 |
| 服务端配置同步 | serverconfig sync | 存档级 `serverconfig` 规则与文件同步 |
| 构建引擎 | build engine | NeoBuild，执行 clone/checkout/合并/导出 |
| 解析器 | parser | 配置解析器插件（JSON/YAML/…） |
| 导出器 | exporter | 导出格式插件 |
| 拓展 / 扩展 | extension | 可插拔功能模块（parser/pointer/exporter/editor 扩展等） |
| 拓展接口 | extension interface | 插件契约与注册表 |
| 总线 | bus | 事件总线/任务总线等进程内通信机制 |
| 注册表 | registry | 插件/拓展的注册与查找表 |
| ABI | ABI | 二进制接口：导出符号、调用约定、跨 DLL 边界 |
| 插件 | plugin | 以 DLL 形式加载的拓展实现 |
| 安装器 | installer | HiBerCommonInstaller（HIC）安装框架 |
| 安装流程 | install flow | `install.json` 定义的步骤序列 |
| 外部下载模式 | external download mode | `--external-download`，暂停并输出清单 |
| 运行时 | runtime | NSUM Runtime（`NeoServerUpdateModpack.exe` + deploy 目录） |
| 启动注入 | launch injection | bridge 在 HMCL 启动流程插入同步任务 |
| 实例 | instance | HMCL 的游戏实例目录 |
| 实例标记 | instance marker | `<实例>/.NSUM/instance.json` |
| 整合包包格式 | modpack format | `.nsumpack` 文件格式 |
| 文件结构版本 | pack format version | `pack_format`，与整合包自身版本无关 |
| 内嵌文件 | embedded files | 显式 `embed_patterns` 命中并放入包内的文件 |
| 内嵌规则 | embed patterns | 选择内嵌文件的正则列表 |
| 来源提交 | source commit | 构建/导出时的 Git 提交 ID |
| 默认分支 | default branch | 仓库的默认 Git 分支 |
| 元数据站点 | metadata site | `metadata.hibernet.top`，版本清单 API |
| 版本清单 | release manifest | `NSUM.json` / `nsum-bridge.json` |
| 引导程序 | bootstrapper | 安装器的启动/更新入口（如有） |
| 服务端插件 | server plugin | MCDR 插件，负责服务端安装/同步/启动 |
| 崩溃捕获 | crash capture | CrashTracker 的 dump/trace 采集 |
| 文档站 | docs site | 未来的 Next.js + Nextra 站点 |
| 侧边栏 | sidebar | Nextra 导航结构（由 `_meta.json` 驱动） |
| 语言区域 | locale | `zh` / `en` |
| 占位槽位 | placeholder slot | 待补外部文档的锚点标记 |
| 只读查询 | read-only query | CLI `info` 类命令 |
| 执行命令 | mutating command | CLI `exec` 类命令 |
| 退出码 | exit code | 进程返回码（0/1/2/10…） |
| 零信任校验 | zero-trust verification | 使用缓存前重新计算 SHA-256 |
| 内容寻址 | content-addressed | 以内容哈希作为存储键 |
| 硬链接 | hard link | MCLHR store 投影到 `mods/` 的方式 |
| 整合包核心 | server core | paper/folia/velocity/… |
| 分支类型 | branch type | `universal` / `client` / `server` |
| 客户端专用 | client-only | `branchType=client` |
| 服务端专用 | server-only | `branchType=server` |
| 可复现构建 | reproducible build | 由 commit + 配置确定输出 |
| 死链 | dead link | 指向不存在页面/图片的链接 |
| 占位页 | planned page | `status: planned` 的待写页面 |
