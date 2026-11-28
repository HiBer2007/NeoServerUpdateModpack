import pathlib
import re

ROOT = pathlib.Path(r"K:\NeoServerUpdateModpack\docs")

def patch(rel, pairs, required=True):
    p = ROOT / rel
    text = p.read_text(encoding="utf-8")
    for old, new in pairs:
        if old not in text:
            print("MISSING", rel, "::", old[:80].replace("\n", "\\n"))
            if required:
                continue
        text = text.replace(old, new)
    p.write_text(text, encoding="utf-8")

# version index
patch("zh/1.0.7/index.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    ("本文档对应 **NSUM Runtime 1.0.5 / 内部 0.1.11**。页面内标注了适用版本的例外情况。",
     "本文档对应 **NSUM Runtime 1.0.7 / 内部 0.1.13**。页面内标注了适用版本的例外情况。\n\n"
     "本版重点：仓库地址自动选最快、单地址超时自动切换与手动优先级（见 [安装](./user/installation.md#仓库地址选择) 与 [CLI](./user/cli.md#仓库地址选择)）。"),
])
patch("en/1.0.7/index.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    ("These docs target **NSUM Runtime 1.0.5 / internal 0.1.11**. Page-level version notes are added where behavior differs.",
     "These docs target **NSUM Runtime 1.0.7 / internal 0.1.13**. Page-level version notes are added where behavior differs.\n\n"
     "This release focuses on automatic fastest repository selection, per-address timeout fallback and manual priority (see [Installation](./user/installation.md#address-selection) and [CLI](./user/cli.md#address-selection))."),
])

# installation
zh_install_note = (
    "> **地址选择（自 1.0.7）**：多地址时默认自动探测最快地址；可在仓库页选择「优先 SSH/HTTPS/HTTP」，或用 `--repo-preference manual` 强制使用输入的 URL。\n\n")
zh_install_new = """## 仓库地址选择

自 1.0.7 起，`.nsumpack` 记录多个仓库地址时，运行时**自动探测并选择延迟最低的地址**；单个地址超时或失败会**自动尝试下一个**。SSH / HTTPS / HTTP 三个地址都保留在包内，客户端按实际网络选择，不需要重新打包。

| 模式 | 行为 | 设置方式 |
|---|---|---|
| 自动最快（默认） | 对每个地址执行 `git ls-remote` 测延迟，最低的优先；失败/超时回退 | `--repo-preference auto`；主程序「地址优先级：自动最快」 |
| 优先协议 | 指定的 SSH / HTTPS / HTTP 优先，失败再回退其他地址 | `--repo-preference ssh\\|https\\|http`；主程序下拉 |
| 手动 | 只用给定 URL，不探测、不换源 | `--repo-preference manual`，或直接输入完整 URL |
| bridge | 按 `.hmcl/nsum/repo.properties` 的 `preference` 执行 | 见[客户端 bridge](../extensions/client-bridge/index.md#仓库地址选择) |

超时默认值：探测 8s、克隆 120s（主程序 GUI 60s）、bridge 同步 300s。可用 `--repo-timeout <秒>` 或 `repo.properties` 调整；全部地址都失败才报错，日志会列出每个地址的探测延迟与失败原因。

"""
patch("zh/1.0.7/user/installation.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    (zh_install_note, zh_install_new),
])
en_install_note = (
    "> **Address selection (since 1.0.7)**: multiple URLs are probed and the fastest is used; choose **prefer SSH/HTTPS/HTTP** on the repository page, or force the entered URL with `--repo-preference manual`.\n\n")
en_install_new = """## Address selection

Since 1.0.7, when a `.nsumpack` carries multiple repository URLs the runtime **probes them and picks the lowest-latency address**; a single address that times out or fails **automatically falls back to the next one**. SSH / HTTPS / HTTP addresses stay in the pack, so clients choose according to their own network without repacking.

| Mode | Behavior | How to set |
|---|---|---|
| Auto fastest (default) | Probe each address with `git ls-remote`, prefer the lowest latency, fall back on failure/timeout | `--repo-preference auto`; **Address priority: auto fastest** in the main program |
| Prefer protocol | Prefer the chosen SSH / HTTPS / HTTP address, then fall back | `--repo-preference ssh\\|https\\|http`; GUI selector |
| Manual | Use only the given URL, no probing or switching | `--repo-preference manual`, or enter a full URL |
| Bridge | Follow `preference` in `.hmcl/nsum/repo.properties` | See [Client bridge](../extensions/client-bridge/index.md#address-selection) |

Default timeouts: 8s probe, 120s clone (60s in the main GUI), 300s bridge sync. Adjust with `--repo-timeout <seconds>` or `repo.properties`; only when every address fails does the command report an error, and the log lists each probe latency and failure reason.

"""
patch("en/1.0.7/user/installation.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    (en_install_note, en_install_new),
])

# CLI pages: extend the since-note and add a section before JSON output
zh_cli_note = "> `--repo-preference auto|ssh|https|http|manual` 与 `--repo-timeout <秒>` 自 **1.0.7** 起提供：auto 并行探测最快地址，manual 只用给定 URL。"
zh_cli_new = """## 仓库地址选择

`exec build` / `exec export` 的 `--repo` 可以是一个 URL。若能从 `.nsumpack` 的 `repository.urls` 或 workspace 的 `git.remotes` 推导出 SSH / HTTPS / HTTP 多个地址，默认 `auto` 会先测延迟再克隆：

```powershell
# 默认：自动选最快，失败自动换下一个
.\\NeoServerUpdateModpack.exe exec export --repo https://github.com/owner/repo.git `
    --modpack base --format nsum --export out.nsumpack --repo-preference auto

# 优先 SSH，SSH 不可用再回退 HTTPS/HTTP
.\\NeoServerUpdateModpack.exe exec export --repo https://github.com/owner/repo.git `
    --modpack base --format nsum --export out.nsumpack --repo-preference ssh

# 只用输入的 URL，不探测、不换源；每个地址最多 30 秒
.\\NeoServerUpdateModpack.exe exec export --repo https://github.com/owner/repo.git `
    --modpack base --format nsum --export out.nsumpack --repo-preference manual --repo-timeout 30
```

- `--repo-preference`：`auto`（默认）/ `ssh` / `https` / `http` / `manual`；
- `--repo-timeout <秒>`：同时设置探测与克隆的每地址超时（默认探测 8s、克隆 120s）；
- 环境变量 `NSUM_REPO_PREFERENCE` 可作为默认值，命令行优先；
- 日志中的 `Repository probe: <url> -> <ms>` 显示实际延迟；`Address failed` 后继续下一个，全部失败才退出 1。"""
patch("zh/1.0.7/user/cli.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    (zh_cli_note, zh_cli_note + "\n\n" + zh_cli_new),
])
en_cli_note = "> `--repo-preference auto|ssh|https|http|manual` and `--repo-timeout <seconds>` are available since **1.0.7**: auto probes addresses in parallel and picks the fastest, manual uses only the given URL."
en_cli_new = """## Address selection

`--repo` for `exec build` / `exec export` may be a single URL. When SSH / HTTPS / HTTP alternatives can be derived from `.nsumpack` `repository.urls` or workspace `git.remotes`, the default `auto` probes latency before cloning:

```powershell
# Default: fastest address, fall back on failure
.\\NeoServerUpdateModpack.exe exec export --repo https://github.com/owner/repo.git `
    --modpack base --format nsum --export out.nsumpack --repo-preference auto

# Prefer SSH, fall back to HTTPS/HTTP
.\\NeoServerUpdateModpack.exe exec export --repo https://github.com/owner/repo.git `
    --modpack base --format nsum --export out.nsumpack --repo-preference ssh

# Use only the given URL; at most 30 seconds per address
.\\NeoServerUpdateModpack.exe exec export --repo https://github.com/owner/repo.git `
    --modpack base --format nsum --export out.nsumpack --repo-preference manual --repo-timeout 30
```

- `--repo-preference`: `auto` (default) / `ssh` / `https` / `http` / `manual`;
- `--repo-timeout <seconds>`: per-address timeout for both probe and clone (defaults: 8s probe, 120s clone);
- Environment variable `NSUM_REPO_PREFERENCE` supplies the default; the command line wins;
- `Repository probe: <url> -> <ms>` in the log shows measured latency; after `Address failed` the next address is tried, and only when all fail does the command exit 1."""
patch("en/1.0.7/user/cli.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    (en_cli_note, en_cli_note + "\n\n" + en_cli_new),
])

# troubleshooting
zh_trouble = """## 症状：仓库地址慢、卡住或超时

**日志关键字**：`Repository probe`、`Address failed`、`Process timed out`、`trying the next address`。

原因：SSH 未配置密钥、HTTPS 被代理/防火墙阻断、DNS 或 GitHub 线路慢。

处理：

1. 默认 `auto` 会列出每个地址的探测延迟；先看 `builder.log` 中哪条地址最慢/不可达；
2. 主程序仓库页把「地址优先级」改为「优先 SSH/HTTPS/HTTP」；CLI 用 `--repo-preference ssh|https|http`；
3. 需要固定某个地址时用 `--repo-preference manual` 并直接传该 URL；
4. 缩短等待：`--repo-timeout 30`（运行时）或 `repo.properties` 的 `probeTimeoutSeconds` / `cloneTimeoutSeconds` / `syncTimeoutSeconds`（bridge）；
5. SSH 地址失败时先在主程序点「SSH 密钥...」或执行 `exec git-ssh-key --test` 验证密钥；
6. 已有仓库不会重复克隆：bridge 用 `git remote set-url` 复用原目录，Runtime 使用缓存目录。

## 症状五：中文/Unicode 路径异常"""
patch("zh/1.0.7/user/troubleshooting.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    ("## 症状五：中文/Unicode 路径异常", zh_trouble),
])
en_trouble = """## Symptom: repository address is slow, hangs or times out

**Log keywords**: `Repository probe`, `Address failed`, `Process timed out`, `trying the next address`.

Cause: SSH key not configured, HTTPS blocked by a proxy/firewall, or a slow DNS/GitHub route.

Handling:

1. Default `auto` logs each probe latency; check `builder.log` for the slowest/unreachable address;
2. Change **Address priority** in the main program to **prefer SSH/HTTPS/HTTP**, or pass `--repo-preference ssh|https|http`;
3. To pin one address, use `--repo-preference manual` with that exact URL;
4. Shorten waits with `--repo-timeout 30` (runtime) or `probeTimeoutSeconds` / `cloneTimeoutSeconds` / `syncTimeoutSeconds` in `repo.properties` (bridge);
5. For SSH failures, use **SSH Key...** in the main program or run `exec git-ssh-key --test`;
6. Existing checkouts are reused via `git remote set-url` (bridge) or the cache directory (Runtime), never cloned twice.

## Symptom 5: Chinese/Unicode path problems"""
patch("en/1.0.7/user/troubleshooting.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    ("## Symptom 5: Chinese/Unicode path problems", en_trouble),
])

# client bridge index
zh_bridge_old = "| 版本 | `0.4.4.17`（当前） |"
zh_bridge_new = "| 版本 | `0.4.6.17`（当前） |"
zh_bridge_section = """## 仓库地址选择

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

"""
patch("zh/1.0.7/extensions/client-bridge/index.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    (zh_bridge_old, zh_bridge_new),
    ("| 并行下载 | Range 分片 + 单流回退 |", "| 并行下载 | Range 分片 + 单流回退 |\n| 多地址选择 | 并行探测取最快，超时自动切换，可手动指定 |"),
    ("## 页面", zh_bridge_section + "## 页面"),
    ("| `.hmcl/nsum/download.properties` | 下载器配置 |", "| `.hmcl/nsum/download.properties` | 下载器配置 |\n| `.hmcl/nsum/repo.properties` | 仓库地址选择配置 |"),
])
en_bridge_old = "| Version | `0.4.4.17` (current) |"
en_bridge_new = "| Version | `0.4.6.17` (current) |"
en_bridge_section = """## Address selection

Since 0.4.6.17 the bridge probes multiple `repository.urls[]` **in parallel** (`git ls-remote`) and prefers the lowest latency; each address has its own timeout and failure/timeout falls back to the next. Existing checkouts are reused via `git remote set-url`; it never clones one copy per address.

Optional `.hmcl/nsum/repo.properties`:

```properties
preference=auto
probeTimeoutSeconds=8
cloneTimeoutSeconds=180
syncTimeoutSeconds=300
```

- `preference`: `auto` (default, fastest) / `ssh` / `https` / `http` / `manual` (manifest order) / explicit URL;
- On sync timeout the bridge terminates that Runtime process and tries the next address;
- System properties `-Dnsum.repo.preference=...`, `-Dnsum.repo.probeTimeoutSeconds=...` override the file (environment variables such as `NSUM_REPO_PREFERENCE` also work);
- A successful address is stored as `activeRepoUrl` and preferred on the next launch, avoiding repeated probing.

"""
patch("en/1.0.7/extensions/client-bridge/index.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    (en_bridge_old, en_bridge_new),
    ("| Parallel downloads | Range chunks + single-stream fallback |", "| Parallel downloads | Range chunks + single-stream fallback |\n| Multi-address selection | Parallel fastest probe, timeout fallback, manual override |"),
    ("## Pages", en_bridge_section + "## Pages"),
    ("| `.hmcl/nsum/download.properties` | Downloader config |", "| `.hmcl/nsum/download.properties` | Downloader config |\n| `.hmcl/nsum/repo.properties` | Repository address selection |"),
])

# NeoWorkspace API
patch("zh/1.0.7/developer/Modules/NeoWorkspace/usage.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    ("| `GitOperations::fetch` |", "| `GitOperations::lsRemote` | `GitResult lsRemote(const std::string& url, int timeoutMs = 8000)` | 探测仓库地址（`git ls-remote`） |\n| `GitOperations::fetch` |"),
])
patch("en/1.0.7/developer/Modules/NeoWorkspace/usage.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    ("| `GitOperations::fetch` |", "| `GitOperations::lsRemote` | `GitResult lsRemote(const std::string& url, int timeoutMs = 8000)` | Probe a repository address (`git ls-remote`) |\n| `GitOperations::fetch` |"),
])

# data model
patch("zh/1.0.7/developer/architecture/data-model.md", [
    ("updated: 2026-11-27", "updated: 2026-11-28"),
    ("`git.remotes` 记录同一仓库的多种访问地址，按优先级排序；`git.remote` 仍是主地址（旧字段）。",
     "`git.remotes` 记录同一仓库的多种访问地址，按优先级排序；`git.remote` 仍是主地址（旧字段）。运行时按 `--repo-preference` 或默认 `auto` 探测延迟，选择最快地址并保留失败回退顺序（自 1.0.7）。"),
])
patch("en/1.0.7/developer/architecture/data-model.md", [
    ("updated: 2026-11-27", "2026-11-28"),
    ("`git.remotes` records the multiple access addresses of the same repository, ordered by priority; `git.remote` remains the primary (legacy) address.",
     "`git.remotes` records the multiple access addresses of the same repository, ordered by priority; `git.remote` remains the primary (legacy) address. At runtime the fastest address is probed according to `--repo-preference` or the default `auto`, while the failure fallback order is preserved (since 1.0.7)."),
])

# nsumpack ordering wording
patch("zh/1.0.7/pack-author/nsumpack.md", [
    ("客户端按顺序回退（自 1.0.6 / pack_format 3）", "客户端优先探测延迟最低的地址并超时回退（自 1.0.7；旧客户端按清单顺序，自 1.0.6 / pack_format 3）"),
])
patch("en/1.0.7/pack-author/nsumpack.md", [
    ("clients fall back in list order (since 1.0.6 / pack_format 3)", "clients probe and prefer the lowest-latency address, with timeout fallback (since 1.0.7; older clients use list order, since 1.0.6 / pack_format 3)"),
])

# editor GUI correction
patch("zh/1.0.7/pack-author/editor-gui.md", [
    ("- 主程序仓库页新增「地址优先级」下拉（自动最快 / 优先 SSH / 优先 HTTPS / 优先 HTTP，自 1.0.7）；克隆失败或 60s 超时自动尝试下一地址；",
     "- 主程序（非编辑器）仓库页新增「地址优先级」下拉（自动最快 / 优先 SSH / 优先 HTTPS / 优先 HTTP，自 1.0.7）；克隆失败或 60s 超时自动尝试下一地址；编辑器内仍通过主地址下拉与三个输入框手动指定；"),
])
patch("en/1.0.7/pack-author/editor-gui.md", [
    ("- The main program repository page has an **Address priority** selector (auto fastest / prefer SSH / HTTPS / HTTP, since 1.0.7); a failed clone or 60s timeout tries the next address;",
     "- The main program (not the Editor) repository page has an **Address priority** selector (auto fastest / prefer SSH / HTTPS / HTTP, since 1.0.7); a failed clone or 60s timeout tries the next address; inside the Editor, the primary selector and three URL fields remain the manual controls;"),
])

# translation_of: 1.0.7 pages must point at 1.0.7
for p in (ROOT / "en" / "1.0.7").rglob("*.md"):
    text = p.read_text(encoding="utf-8")
    new = re.sub(r"translation_of: /zh/1\.0\.6/", "translation_of: /zh/1.0.7/", text)
    if new != text:
        p.write_text(new, encoding="utf-8")

print("docs 1.0.7 updated")
