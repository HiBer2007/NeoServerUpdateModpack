---
title: ADR 0007 · HTTP Range 并行下载
description: 对支持 Range 的大文件按分片并发下载，失败或超时立即回退单流，C++ 与 Java 两端采用同一套探测与回退约定。
lang: zh
status: complete
updated: 2026-11-27
---

# ADR 0007 · HTTP Range 并行下载

## 状态

已接受（2026-11-27）

## 背景

运行时要下载 Git、模组 jar、`.nsumpack`、Runtime 包与 HIC 依赖。单流下载在慢速或高延迟网络下耗时不可接受，而多线程下载在受限网络（代理、GitHub 302、CDN 忽略 Range）下很容易挂死：

- 部分服务器对 `HEAD` 不返回 `Content-Length`/`Accept-Ranges`，甚至不跟随 302；
- 探测「不支持 Range」后仍继续分片，会得到 200 全量响应，分片写错位；
- 某个分片 0 字节悬挂时，若没有总时限，任务会长时间不退出（Java 版曾跑 11 小时才失败）；
- 代理下并发连接不稳定，单点失败必须整体回退，而不是重试到天荒地老。

## 决策

两端实现同一套语义：

- **探测**：先 `HEAD`；失败或字段缺失时用 `GET` + `Range: bytes=0-0`，从 `Content-Range`（总长）与响应码判断是否支持 Range。Java 的 `HttpURLConnection` 对 GitHub 302 不跟随，因此 Java 侧直接用 `GET Range 0-0` 探测。
- **切分**：支持 Range 且体积 ≥ 阈值（默认 8 MB）时按线程数切片（默认 16，分片大小自动 1–4 MB 或显式指定）；不支持、体积过小或探测失败则走单流。
- **写入**：C++ 先 `QFile::resize` 预分配再定位写；Java 用 `FileChannel.write(buf, position)` 定位写，进度与取消沿用现有任务对话框。
- **并发上限**：同一服务器最多 32 路连接（C++ 常量，Java 侧用进程内 `Semaphore`）。
- **失败即回退**：任一分片失败、校验失败或用户取消 → 删除目标文件、返回失败，由调用方回退单流；**不**保留部分文件。
- **强制时限**：Java 侧必须有总时限（默认 10 分钟），超时 `shutdownNow` 并回退；C++ 侧沿用 Qt 网络超时与取消令牌。
- **接入点**：`PointerDownloader::downloadToPath`（并行优先，失败回退 QNAM 单流）、`CliDispatcher::cmdGitUpdate`（`tryParallel` → curl → PowerShell）、bridge 的 Runtime/HIC 下载（`NSumParallelDownloader`）。
- **配置**：环境变量 `NSUM_DOWNLOAD_THREADS` / `NSUM_DOWNLOAD_MIN_SIZE_MB` / `NSUM_DOWNLOAD_CHUNK_MB` / `NSUM_DOWNLOAD_RETRIES`，`NSUM_DOWNLOAD_PARALLEL=0` 关闭；Java 侧 `<HMCL>/.hmcl/nsum/download.properties`（`enabled/threads/minSizeMB/chunkMB/retries`）或 `-Dnsum.download.*`。

## 备选方案

| 方案 | 优点 | 缺点 |
|---|---|---|
| 只保留单流 | 最稳定、实现简单 | 大文件慢，受限网络下体验差 |
| 外挂 curl 分段进程 | 复用成熟工具 | 进程管理与合并复杂，进度/取消难统一 |
| 无上限并发 | 峰值速度高 | 触发服务器限流/封禁，本机代理易崩 |
| 分片 + 回退 + 时限（本决策） | 快且可控，失败可解释 | 需要维护两套实现与配置项 |

## 后果

- 正面：本地实测 20 MB / 8 线程 0.16 s；`git-update` 40 MB / 8 线程 62.7 s（日志 `Multi-thread download complete (8 threads)`）；Java 侧 20 MB / 8 线程 264 ms，阈值不满足时回退单流 58 ms；失败路径明确，不会留下损坏文件。
- 负面：代码量翻倍（C++/Java 各一套），阈值与线程数需要按服务器调优；分片请求会放大 CDN 与代理压力，排查网络问题时要先关并行（`NSUM_DOWNLOAD_PARALLEL=0` / `enabled=false`）。
- 影响范围：`NeoCore`（`ParallelDownloader`）、`NeoBuild`（指针下载）、`NeoCLI`（git-update）、bridge（Runtime/HIC 下载）、发布包的下载体验。

## 相关

- [下载器](../architecture/downloader.md)
- [客户端 Bridge 下载](../../extensions/client-bridge/download.md)
- [测试与验证](../testing.md)
- [ADR 0001 · 指针文件与内容寻址缓存](./0001-pointer-cache.md)
