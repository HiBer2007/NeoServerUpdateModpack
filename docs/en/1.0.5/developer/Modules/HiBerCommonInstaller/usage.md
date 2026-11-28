---
title: HiBerCommonInstaller usage
description: Public API of HiBerCommonInstaller, minimal product/flow examples, extension registration and CLI usage.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/developer/Modules/HiBerCommonInstaller/usage.md
---

# HiBerCommonInstaller usage

## Quick start

**Running a flow in library mode** (the same assembly order as `hci_cli`, minus argument parsing):

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
ExtensionRegistry registry;          // host-owned; shared by the loader and the runner

ExtensionLoader loader(&bus, &services, &ctx, &product, &registry);
loader.loadStatic();                 // extensions registered with HCI_REGISTER_EXTENSION
loader.loadDirectory("extensions");  // optional: *.dll drop-ins and *.hci packages

auto script = createLuaEngine();
FlowSpec flow = FlowSpec::loadFile("install.json");

IFlowUi* ui = nullptr;   // shell implementation; nullptr is safe only for pure exec flows
                         // (no ui steps and the product does not request elevation)
FlowRunner runner(product, ctx, ui, script.get());
runner.setEventBus(&bus);
runner.setRegistry(&registry);       // required for extension step types
runner.setBaseDir(".");
int rc = runner.run(flow);           // 0 = success, 1 = failure/cancel, 2 = external downloads required
loader.shutdownAll();
```

> **Note**: passing `nullptr` for `ui` is only safe when the flow contains no `ui` step and `product.elevation.request` is false - `handleUiStep` and the elevation branch dereference the UI directly.

**Running the ready-made installer** (`hci_cli.exe` inside an NSUM release package):

```powershell
hci_cli --cli --silent --path "<install dir>" --product product.json --flow install
hci_cli --cli --json --silent --path "<install dir>"     # JSON protocol blocks on stdout
```

Exit codes: `0` success, `1` failure, `2` usage/argument error, `10` external downloads pending.

## Public API

| API | Signature | Description |
|---|---|---|
| `ProductConfig::loadFile` | `static ProductConfig loadFile(const std::string& jsonPath);` | reads product.json; throws `std::runtime_error` on failure |
| `ProductConfig::loadString` | `static ProductConfig loadString(const std::string& jsonText);` | loads from a string |
| `EntryOptions::resolveMode` | `std::string resolveMode(const ProductConfig& product) const;` | explicit mode wins, otherwise the product default |
| `renderBanner` | `std::string renderBanner(const std::string& productName, const std::string& font = "slant", const std::string& version = "");` | product-name ASCII art plus the powered-by line |
| `runInstall` | `int runInstall(const ProductConfig& product, const std::string& flowPath, const EntryOptions& options);` | runs a whole install flow in library mode and returns the process exit code |
| `Vars::set` / `setBool` / `setInt` | `void set(const std::string& name, const std::string& value);` `void setBool(const std::string& name, bool value);` `void setInt(const std::string& name, long long value);` | write variables |
| `Vars::get` / `has` / `getBool` | `std::string get(const std::string& name) const;` `bool has(const std::string& name) const;` `bool getBool(const std::string& name, bool defaultValue = false) const;` | read variables |
| `Vars::interpolate` | `std::string interpolate(const std::string& text) const;` | recursive `{name}` interpolation (cycle-safe, unknown names become empty) |
| `InstallContext::vars` | `Vars& vars();` / `const Vars& vars() const;` | variable table of this run |
| `InstallContext::cancel` / `cancelled` | `void cancel();` / `bool cancelled() const;` | cancel flag |
| `InstallContext::setFailurePolicy` | `void setFailurePolicy(StepFailure p);` | `Abort` (default) / `Ignore` / `Continue` |
| `FlowSpec::loadFile` / `loadString` | `static FlowSpec loadFile(const std::string& jsonPath);` `static FlowSpec loadString(const std::string& jsonText);` | load a flow |
| `FlowRunner::FlowRunner` | `FlowRunner(ProductConfig& product, InstallContext& ctx, IFlowUi* ui, IScriptEngine* script);` | constructs the runner |
| `FlowRunner::run` | `int run(FlowSpec& flow);` | executes the flow; returns 0/1 (2 in external download mode) |
| `FlowRunner::setRegistry` | `void setRegistry(ExtensionRegistry* registry) { registry_ = registry; }` | injects the extension registry (required for extension steps) |
| `FlowRunner::setExternalDownload` | `void setExternalDownload(const std::string& dir) { externalDir_ = dir; }` | external download mode directory |
| `FlowRunner::externalPending` | `bool externalPending() const { return externalPending_; }` | whether downloads are still pending |
| `ExtensionRegistry::registerStep` | `void registerStep(const std::string& type, StepHandler handler);` | registers a custom step type |
| `ExtensionRegistry::registerCliArg` | `void registerCliArg(const std::string& arg, CliArgHandler handler, const std::string& help = "");` | registers a product-level CLI argument (help appears in the shell's `--help`) |
| `ExtensionRegistry::registerDownloadBackend` | `void registerDownloadBackend(const std::string& name, DownloadBackendFactory factory);` | registers a download backend |
| `IHciExtension::init` | `virtual bool init(HostApi& api) = 0;` | extension initialization: register steps/args/backends here |
| `IHciExtension::capabilities` | `virtual HciCapabilities capabilities() const = 0;` | declares capabilities (steps/pages/cliArgs/services...) |
| `ExtensionLoader::ExtensionLoader` | `ExtensionLoader(EventBus* bus, ServiceRegistry* services, InstallContext* ctx, const ProductConfig* product, ExtensionRegistry* registry = nullptr);` | constructs the loader |
| `ExtensionLoader::loadStatic` / `loadDirectory` | `void loadStatic();` / `void loadDirectory(const std::string& dir);` | static registration / directory loading (DLLs and `.hci` packages) |
| `ExtensionLoader::modules` | `std::vector<std::pair<std::string, std::string>> modules() const;` | (id, version) of the loaded extensions |
| `Log::instance().addSink` | `void addSink(std::shared_ptr<ILogSink> sink);` | attaches a sink (`ConsoleSink` / `FileSink` / `NullSink`) |
| `Log::Info` and friends | `static void Info(const std::string& m) { instance().write(LogLevel::Info, m); }` | Trace/Debug/Info/Warn/Error convenience entries |
| `hci::fmt` | `template <typename... Args> inline std::string fmt(const std::string& pattern, Args&&... args);` | `{}` placeholder formatting |
| `exec::extractZip` | `bool extractZip(const std::string& zipPath, const std::string& destDir, const std::string& sevenZipExe = "", std::string* error = nullptr);` | ZIP extraction (libzippp primary, 7za/tar fallback) |
| `exec::copyTree` | `bool copyTree(const std::string& srcRoot, const std::string& dstRoot, const std::vector<std::string>& skipPatterns = {}, std::function<bool(const std::string& rel)> onFile = nullptr, std::string* error = nullptr);` | directory copy (with skip wildcards) |
| `exec::runProcess` | `bool runProcess(const std::vector<std::string>& command, int waitMs, ProcessResult& result);` | starts a child process and collects stdout+stderr |
| `exec::renderTemplate` | `std::string renderTemplate(const std::string& text, const Vars& vars);` | renders `{name}` text templates |
| `download::runChain` | `DownloadResult runChain(const DownloadRequest& req, const std::vector<std::string>& chain, const std::vector<std::shared_ptr<IDownloadBackend>>& extra, DownloadProgress progress = nullptr);` | downloads by trying the backend chain |
| `download::httpDownload` | `bool httpDownload(const std::string& url, const std::string& dest, int threads, long long maxSeconds, DownloadProgress progress, std::string& error);` | cpr segmented multi-threaded download |
| `download::resolveGitHubAsset` | `bool resolveGitHubAsset(const std::string& repo, const std::string& variant, bool allowExe, std::string& url, long long& size, std::string& error);` | resolves a GitHub release asset URL |
| `elevation::isElevated` / `relaunchAsAdmin` | `bool isElevated();` / `bool relaunchAsAdmin(std::string& err);` | UAC check and relaunch as admin with the original command line |
| `port::exeDir` / `localAppData` / `expandEnv` | `std::string exeDir();` / `std::string localAppData();` / `std::string expandEnv(const std::string& text);` | paths and environment expansion (`%APPDATA%`) |

## Typical usage

### Minimal product.json and flow

`product.json` (fields taken from `hci::ProductConfig`):

```json
{
  "productName": "MyApp",
  "company": "HiBer2007",
  "version": "1.0.0",
  "defaultMode": "gui",
  "defaultInstallPath": "%LOCALAPPDATA%/MyApp",
  "components": [
    { "id": "editor", "label": "Editor", "exe": "MyEditor.exe", "defaultChecked": true }
  ],
  "flows": { "install": "install.json", "uninstall": "uninstall.json" },
  "elevation": { "request": false, "autoRestart": true, "reason": "Writing to Program Files requires administrator rights" },
  "extensions": { "hci.git": { "gitDir": "tools/git" } }
}
```

`install.json` (step types come from `FlowRunner`'s built-in set):

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

Common step fields: `id`, `type` (exec step) or `ui` (interactive step, exactly one of the two), `next` (`__end` finishes), `when` (Lua expression, empty = always run), `onFail` (`abort` by default / `ignore`); all remaining keys go into `params`.

### Registering a custom extension

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

Statically registered extensions must be compiled into the shell through `target_sources` (that is exactly how NSUM builds `git`/`winget`/`nsum_args_ext` into `hci_gui` and `hci_cli`); otherwise the flow reports `unknown step type`.

### Download backend chain

```json
{ "id": "git", "type": "download", "asset": "git-for-windows/git", "variant": "MinGit",
  "chain": ["github", "direct"], "dest": "{installDir}/tools/git.zip", "maxSeconds": 150 }
```

Backends are tried in `chain` order and the first one that both `supports()` and succeeds wins; `extra` carries host/extension backends (such as `winget` and `apt`). `DownloadRequest` also supports `package` (package-manager ID), `threads` (parallel connections, 1 = single stream) and `maxSeconds` (per-backend time limit).

### Assembling IFlowUi by hand (embedding without a shell)

An `IFlowUi` interaction method returning `false` means the user cancelled; when `backRequested()` returns `true` the flow goes back to the previous step instead of aborting. The GUI/TUI/CLI shells all implement the same interface, so a custom host only needs `onWelcome` / `onLicense` / `onPath` / `onComponents` / `onOption` / `onConfirm` / `onInput` / `onProgress` / `onMessage` / `onFinish` (`onGit`, `onLanguage`, `onStepParam` and `onElevate` have default implementations).

### Querying loaded extensions

```cpp
for (const auto& [id, version] : loader.modules())
    hci::Log::Info(hci::fmt("{} {}", id, version));
```

## Notes

- **Extension steps need the registry**: without `FlowRunner::setRegistry` an extension step type cannot execute; `HostApi::registry()` falls back to the static `ExtensionRegistry::instance()`, but cross-DLL scenarios must use the instance injected by the host.
- **`qrc:` resources need a reader from the shell**: `hci_core` stays zero-Qt and the GUI shell injects the reader through `FlowRunner::setResourceReader`; without a reader `qrc:` sources are unavailable.
- **`ScriptEngine` is not thread-safe**: reuse the single instance returned by `createLuaEngine()` for one run; do not share it across threads.
- **Initialize logging early**: `hci::Log` has no sink by default (there is no built-in default output), so nothing is recorded until `hci::Log::instance().addSink(std::make_shared<hci::ConsoleSink>())` or a `FileSink` is attached; `ConsoleSink` writes `Error` and above to stderr.
- **`--path` and variable precedence**: the CLI's `--path` writes the `installDir` variable, and both `defaultInstallPath` and user input are resolved through `Vars::interpolate`, which is what expands `{installDir}`.
- **Semantics of external download mode**: after `setExternalDownload(dir)` every download step first looks for `<dir>/files/<name>`; when the file is missing it writes `<dir>/downloads.json` and makes `run()` return 2 (CLI exit code 10). Re-run the same command after supplying the files; see [external download mode](../../../extensions/installer/external-download.md).
- **JSON protocol blocks**: `hci_cli --json` prints only `{category:"hci", event, ...}` framed by `=====JSON-BEGIN=====` / `=====JSON-END=====` on stdout, while the human-readable banner and logs go to stderr.
- **Do not rewrite the exit codes**: the 0/1/2/10 of `runInstall` / `hci_cli` are what external scripts (the bridge, for example) rely on.
- **CJK encoding**: framework sources stay pure ASCII and non-ASCII text uses `\uXXXX` escapes (compiled with `/utf-8`) - follow the same convention when adding localized shell text, otherwise it is mangled under code page 936.

## Related

- [README.md](./README.md)
- [ADR 0004 - HIC declarative install flow](../../adr/0004-hic-flow.md)
- [Installer (HIC) overview](../../../extensions/installer/index.md)
- [Flow scripts](../../../extensions/installer/flow.md)
- [Extensions](../../../extensions/installer/extensions.md)
- [Installer CLI](../../../extensions/installer/cli.md)
