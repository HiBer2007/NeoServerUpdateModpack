---
title: HiBerCommonInstaller 使用
description: HiBerCommonInstaller 的公共 API、product/flow 配置最小示例、拓展注册与 CLI 用法。
lang: zh
status: complete
updated: 2026-11-27
---

# HiBerCommonInstaller 使用

## 快速开始

**以库模式跑一个流程**（与 `hci_cli` 的装配顺序一致，省去参数解析）：

```cpp
#include <hci/entry.h>
#include <hci/extension.h>
#include <hci/flow.h>
#include <hci/product.h>
#include <hci/script.h>

using namespace hci;

ProductConfig product = ProductConfig::loadFile("product.json");

InstallContext ctx;
ctx.vars().set("installDir", "D:/NSUM");

EventBus bus;
ServiceRegistry services;
ExtensionRegistry registry;          // 宿主持有，与 loader、runner 共用同一实例

ExtensionLoader loader(&bus, &services, &ctx, &product, &registry);
loader.loadStatic();                 // HCI_REGISTER_EXTENSION 静态注册的拓展
loader.loadDirectory("extensions");  // 可选：*.dll drop-in 与 *.hci 包

auto script = createLuaEngine();
FlowSpec flow = FlowSpec::loadFile("install.json");

IFlowUi* ui = /* 外壳实现；纯执行流程（无 ui 步骤且产品未请求提权）才可传 nullptr */ nullptr;
FlowRunner runner(product, ctx, ui, script.get());
runner.setEventBus(&bus);
runner.setRegistry(&registry);       // 拓展步骤类型必需
runner.setBaseDir(".");
int rc = runner.run(flow);           // 0 = 成功，1 = 失败/取消，2 = 需外部下载
loader.shutdownAll();
```

> **注意**：`ui` 传 `nullptr` 只在流程不含 `ui` 步骤、且 `product.elevation.request` 为 false 时安全——`handleUiStep` 与提权分支会直接解引用 UI。

**跑现成的安装器**（NSUM 发行包内的 `hci_cli.exe`）：

```powershell
hci_cli --cli --silent --path "<安装目录>" --product product.json --flow install
hci_cli --cli --json --silent --path "<安装目录>"     # stdout 输出 JSON 协议块
```

退出码：`0` 成功、`1` 失败、`2` 参数/用法错误、`10` 外部下载待补齐。

## 公共 API

| API | 签名 | 说明 |
|---|---|---|
| `ProductConfig::loadFile` | `static ProductConfig loadFile(const std::string& jsonPath);` | 读 product.json，失败抛 `std::runtime_error` |
| `ProductConfig::loadString` | `static ProductConfig loadString(const std::string& jsonText);` | 从字符串加载 |
| `EntryOptions::resolveMode` | `std::string resolveMode(const ProductConfig& product) const;` | 显式 mode 优先，否则用产品默认 |
| `renderBanner` | `std::string renderBanner(const std::string& productName, const std::string& font = "slant", const std::string& version = "");` | 产品名 ASCII art + powered-by 行 |
| `runInstall` | `int runInstall(const ProductConfig& product, const std::string& flowPath, const EntryOptions& options);` | 库模式跑完整个安装流程，返回进程退出码 |
| `Vars::set` / `setBool` / `setInt` | `void set(const std::string& name, const std::string& value);` `void setBool(const std::string& name, bool value);` `void setInt(const std::string& name, long long value);` | 写变量 |
| `Vars::get` / `has` / `getBool` | `std::string get(const std::string& name) const;` `bool has(const std::string& name) const;` `bool getBool(const std::string& name, bool defaultValue = false) const;` | 读变量 |
| `Vars::interpolate` | `std::string interpolate(const std::string& text) const;` | `{name}` 递归插值（循环安全，未知名替换为空） |
| `InstallContext::vars` | `Vars& vars();` / `const Vars& vars() const;` | 本次运行的变量表 |
| `InstallContext::cancel` / `cancelled` | `void cancel();` / `bool cancelled() const;` | 取消标志 |
| `InstallContext::setFailurePolicy` | `void setFailurePolicy(StepFailure p);` | `Abort`（默认）/`Ignore`/`Continue` |
| `FlowSpec::loadFile` / `loadString` | `static FlowSpec loadFile(const std::string& jsonPath);` `static FlowSpec loadString(const std::string& jsonText);` | 加载流程 |
| `FlowRunner::FlowRunner` | `FlowRunner(ProductConfig& product, InstallContext& ctx, IFlowUi* ui, IScriptEngine* script);` | 构造执行器 |
| `FlowRunner::run` | `int run(FlowSpec& flow);` | 执行流程，返回 0/1（外部下载模式为 2） |
| `FlowRunner::setRegistry` | `void setRegistry(ExtensionRegistry* registry) { registry_ = registry; }` | 注入拓展注册表（拓展步骤必需） |
| `FlowRunner::setExternalDownload` | `void setExternalDownload(const std::string& dir) { externalDir_ = dir; }` | 外部下载模式目录 |
| `FlowRunner::externalPending` | `bool externalPending() const { return externalPending_; }` | 是否有待补齐下载 |
| `ExtensionRegistry::registerStep` | `void registerStep(const std::string& type, StepHandler handler);` | 注册自定义步骤类型 |
| `ExtensionRegistry::registerCliArg` | `void registerCliArg(const std::string& arg, CliArgHandler handler, const std::string& help = "");` | 注册产品级 CLI 参数（help 进外壳 `--help`） |
| `ExtensionRegistry::registerDownloadBackend` | `void registerDownloadBackend(const std::string& name, DownloadBackendFactory factory);` | 注册下载后端 |
| `IHciExtension::init` | `virtual bool init(HostApi& api) = 0;` | 拓展初始化：在此注册步骤/参数/后端 |
| `IHciExtension::capabilities` | `virtual HciCapabilities capabilities() const = 0;` | 声明能力（steps/pages/cliArgs/services…） |
| `ExtensionLoader::ExtensionLoader` | `ExtensionLoader(EventBus* bus, ServiceRegistry* services, InstallContext* ctx, const ProductConfig* product, ExtensionRegistry* registry = nullptr);` | 构造加载器 |
| `ExtensionLoader::loadStatic` / `loadDirectory` | `void loadStatic();` / `void loadDirectory(const std::string& dir);` | 静态注册 / 目录加载（DLL 与 `.hci` 包） |
| `ExtensionLoader::modules` | `std::vector<std::pair<std::string, std::string>> modules() const;` | 已加载拓展的 (id, version) |
| `Log::addSink` | `void addSink(std::shared_ptr<ILogSink> sink);` | 挂 sink（ConsoleSink / FileSink / NullSink） |
| `Log::Info` 等 | `static void Info(const std::string& m) { instance().write(LogLevel::Info, m); }` | Trace/Debug/Info/Warn/Error 便捷入口 |
| `hci::fmt` | `template <typename... Args> inline std::string fmt(const std::string& pattern, Args&&... args);` | `{}` 占位格式化 |
| `exec::extractZip` | `bool extractZip(const std::string& zipPath, const std::string& destDir, const std::string& sevenZipExe = "", std::string* error = nullptr);` | ZIP 解压（libzippp 主路径，7za/tar 兜底） |
| `exec::copyTree` | `bool copyTree(const std::string& srcRoot, const std::string& dstRoot, const std::vector<std::string>& skipPatterns = {}, std::function<bool(const std::string& rel)> onFile = nullptr, std::string* error = nullptr);` | 目录复制（skip 通配） |
| `exec::runProcess` | `bool runProcess(const std::vector<std::string>& command, int waitMs, ProcessResult& result);` | 起子进程并收集 stdout+stderr |
| `exec::renderTemplate` | `std::string renderTemplate(const std::string& text, const Vars& vars);` | 文本模板 `{name}` 渲染 |
| `download::runChain` | `DownloadResult runChain(const DownloadRequest& req, const std::vector<std::string>& chain, const std::vector<std::shared_ptr<IDownloadBackend>>& extra, DownloadProgress progress = nullptr);` | 按后端链择优下载 |
| `download::httpDownload` | `bool httpDownload(const std::string& url, const std::string& dest, int threads, long long maxSeconds, DownloadProgress progress, std::string& error);` | cpr 分段多线程下载 |
| `download::resolveGitHubAsset` | `bool resolveGitHubAsset(const std::string& repo, const std::string& variant, bool allowExe, std::string& url, long long& size, std::string& error);` | 解析 GitHub Release 资产地址 |
| `elevation::isElevated` / `relaunchAsAdmin` | `bool isElevated();` / `bool relaunchAsAdmin(std::string& err);` | UAC 判定与按原命令行提权重启 |
| `port::exeDir` / `localAppData` / `expandEnv` | `std::string exeDir();` / `std::string localAppData();` / `std::string expandEnv(const std::string& text);` | 路径与环境变量（`%APPDATA%` 展开） |

## 典型用法

### 最小 product.json 与流程

`product.json`（字段取自 `hci::ProductConfig`）：

```json
{
  "productName": "MyApp",
  "company": "HiBer2007",
  "version": "1.0.0",
  "defaultMode": "gui",
  "defaultInstallPath": "%LOCALAPPDATA%/MyApp",
  "components": [
    { "id": "editor", "label": "编辑器", "exe": "MyEditor.exe", "defaultChecked": true }
  ],
  "flows": { "install": "install.json", "uninstall": "uninstall.json" },
  "elevation": { "request": false, "autoRestart": true, "reason": "写入 Program Files 需要管理员权限" },
  "extensions": { "hci.git": { "gitDir": "tools/git" } }
}
```

`install.json`（步骤类型取自 `FlowRunner` 的内置集合）：

```json
{
  "id": "install",
  "vars": { "installDir": "%LOCALAPPDATA%/MyApp" },
  "steps": [
    { "id": "welcome", "ui": "welcome", "next": "license" },
    { "id": "license", "ui": "license", "source": "LICENSE.txt", "next": "path" },
    { "id": "path", "ui": "path", "next": "components" },
    { "id": "components", "ui": "components", "next": "extract" },
    { "id": "extract", "type": "extract", "source": "payload.zip", "target": "{installDir}",
      "skip": ["*.log"], "next": "conf" },
    { "id": "conf", "type": "template", "file": "install.conf", "target": "{installDir}",
      "header": "# generated by MyApp installer",
      "template": { "installDir": "{installDir}", "version": "1.0.0" }, "next": "done" },
    { "id": "done", "ui": "finish", "next": "__end" }
  ]
}
```

步骤通用字段：`id`、`type`（执行类）或 `ui`（交互类，二选一）、`next`（`__end` 结束）、`when`（Lua 表达式，空=总执行）、`onFail`（`abort` 默认 / `ignore`），其余键进 `params`。

### 注册一个自定义拓展

```cpp
#include "hci/exec.h"
#include "hci/extension.h"

class MyExtension : public hci::IHciExtension {
public:
    const char* id() const override { return "my.app"; }
    const char* version() const override { return "1.0.0"; }

    hci::HciCapabilities capabilities() const override
    {
        hci::HciCapabilities c;
        c.providesSteps = true;
        c.providesCliArgs = true;
        return c;
    }

    bool init(hci::HostApi& api) override
    {
        api.registry().registerCliArg("--with-editor", [](const std::string&, hci::InstallContext& ctx) {
            ctx.vars().setBool("components.editor", true);
            return true;
        }, "install the editor component");

        api.registry().registerStep("my_step", [](const nlohmann::json& params,
                                                  hci::InstallContext& ctx,
                                                  std::string& error) {
            const std::string target = ctx.vars().interpolate(params.value("target", ""));
            if (target.empty()) { error = "my_step: target is empty"; return false; }
            return hci::exec::mkdirs(target);
        });
        return true;
    }

    void shutdown() override {}
};

HCI_REGISTER_EXTENSION(MyExtension);
```

静态注册的拓展必须由外壳 `target_sources` 编入（NSUM 侧就是这么把 `git`/`winget`/`nsum_args_ext` 编进 `hci_gui` 与 `hci_cli` 的）；否则流程会报 `unknown step type`。

### 下载后端链

```json
{ "id": "git", "type": "download", "asset": "git-for-windows/git", "variant": "MinGit",
  "chain": ["github", "direct"], "dest": "{installDir}/tools/git.zip", "maxSeconds": 150 }
```

后端按 `chain` 顺序尝试，第一个既 `supports()` 又成功的胜出；`extra` 传入宿主/拓展注册的后端（如 `winget`、`apt`）。`DownloadRequest` 还支持 `package`（包管理器 ID）、`threads`（并发连接数，1 = 单流）、`maxSeconds`（单后端时限）。

### 手动装配 IFlowUi（无壳嵌入）

`IFlowUi` 的交互方法返回 `false` 表示用户取消；`backRequested()` 返回 `true` 时流程回退到上一步而不是中止。GUI/TUI/CLI 三套壳都实现了同一接口，自定义宿主只需实现 `onWelcome` / `onLicense` / `onPath` / `onComponents` / `onOption` / `onConfirm` / `onInput` / `onProgress` / `onMessage` / `onFinish`（`onGit`、`onLanguage`、`onStepParam`、`onElevate` 有默认实现）。

### 查询已加载拓展

```cpp
for (const auto& [id, version] : loader.modules())
    hci::Log::Info(hci::fmt("{} {}", id, version));
```

## 注意事项

- **拓展步骤必须有 registry**：`FlowRunner::setRegistry` 未注入时，拓展步骤类型无法执行；`HostApi::registry()` 会退回 `ExtensionRegistry::instance()` 静态兜底，但跨 DLL 场景必须用宿主注入的实例。
- **`qrc:` 资源需要外壳提供读取器**：`hci_core` 保持零 Qt，`FlowRunner::setResourceReader` 由 GUI 壳注入；无读取器时 `qrc:` 源不可用。
- **`ScriptEngine` 非线程安全**：一次运行复用一个 `createLuaEngine()` 返回的实例即可，不要跨线程共享。
- **提前初始化日志**：`hci::Log` 默认无 sink（不存在内置默认输出），需要 `addSink(std::make_shared<hci::ConsoleSink>())` 或 `FileSink` 才会留下记录；`ConsoleSink` 把 `Error` 及以上写 stderr。
- **`--path` / 变量优先级**：CLI 传 `--path` 会写入 `installDir` 变量，流程里的 `defaultInstallPath` 与用户输入都经 `Vars::interpolate` 解析，`{installDir}` 才会展开。
- **外部下载模式的语义**：`setExternalDownload(dir)` 后每个下载步骤先查 `<dir>/files/<name>`，缺失则写 `<dir>/downloads.json` 并让 `run()` 返回 2（CLI 退出码 10）；补齐文件后重跑同一条命令即可，详见 [外部下载模式](../../../extensions/installer/external-download.md)。
- **JSON 协议块**：`hci_cli --json` 的 stdout 只含 `=====JSON-BEGIN=====` / `=====JSON-END=====` 包围的 `{category:"hci", event, ...}`，人类可读 banner 与日志走 stderr。
- **退出码不要自行改写**：`runInstall` / `hci_cli` 的 0/1/2/10 是外部脚本（如 bridge）的判定依据。
- **CJK 编码**：框架源码保持纯 ASCII，中文文案统一用 `\uXXXX` 转义（`/utf-8` 编译）——给外壳加中文时沿用该约定，否则 936 代码页下会乱码。

## 相关文档

- [README.md](./README.md)
- [ADR 0004 · HIC 声明式安装流程](../../adr/0004-hic-flow.md)
- [安装器（HIC）总览](../../../extensions/installer/index.md)
- [流程脚本](../../../extensions/installer/flow.md)
- [拓展](../../../extensions/installer/extensions.md)
- [安装器 CLI](../../../extensions/installer/cli.md)
