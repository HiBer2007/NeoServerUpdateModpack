---
title: NeoCLI usage
description: NeoCLI APIs and typical usage: parsing a command line, dispatching, the JSON protocol and exit codes, and where a new command has to be registered.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/Modules/NeoCLI/usage.md
---

# NeoCLI usage

## Quick start

```cpp
#include <arg_parser.h>
#include <cli_dispatcher.h>

int runCli(int argc, char* argv[])
{
    NeoCLI::ArgParser parser;
    NeoCLI::CliCommand cmd = parser.parse(argc, argv);   // returns the command by value

    NeoCLI::CliDispatcher dispatcher;
    dispatcher.setGitConfig(gitPath, useSystemGit);      // resolved bundled/system git
    return dispatcher.dispatch(cmd);                     // 0 success, 1 failure, 2 usage error
}
```

`parse` never fails with an exception: an unknown category, an unknown verb or a missing verb is reported through `CliCommand::error`, and `dispatch` turns that into a message plus exit code `2`. The host executable also passes the same parsed command to its own `flow gui` path before dispatch, so `argc`/`argv` are parsed once per process.

## Public API

| API | Signature | Description |
|---|---|---|
| `ArgParser::parse` | `CliCommand parse(int argc, char* argv[])` | normalises `/x` to `-x`, splits `--key=value` and `--key value`, fills `category`/`verb`/`options`/`prefill`/`error` |
| `ArgParser::printHelp` | `void printHelp() const`; `void printHelp(CliCategory category) const` | full help, or help for one category |
| `ArgParser::printVersion` | `void printVersion() const` | external, internal, full version and git commit |
| `ArgParser::version` / `buildVersion` / `gitCommit` | `static std::string version()`; `static std::string buildVersion()`; `static std::string gitCommit()` | values injected by the root CMake (`NSUM_EXTERNAL_VERSION`, `NSUM_INTERNAL_VERSION`, `NSUM_GIT_COMMIT`) |
| `ArgParser::categoryFromName` / `categoryName` | `static CliCategory categoryFromName(const std::string& name)`; `static std::string categoryName(CliCategory category)` | `info`/`flow`/`exec` conversion |
| `ArgParser::isHelpToken` / `isVersionToken` | `static bool isHelpToken(const std::string& token)`; `static bool isVersionToken(const std::string& token)` | `-h`, `--help`, `-help`, `help`, `/h`, `/?`, `-?` and `-v`, `--version`, `/v` |
| `ArgParser::verbExists` | `static bool verbExists(CliCategory category, const std::string& verb)` | validates a verb against the per-category table |
| `CliCommand::has` / `get` | `bool has(const std::string& key) const`; `std::string get(const std::string& key) const` | option access (`get` returns an empty string when absent) |
| `CliDispatcher::dispatch` | `int dispatch(const CliCommand& cmd)` | runs the command and returns the exit code |
| `CliDispatcher::setGitConfig` | `void setGitConfig(const std::string& gitPath, bool useSystemGit)` | injects the git executable resolved by the host |
| `CliDispatcher::cancel` / `isCancelled` | `void cancel()`; `bool isCancelled() const` | console interrupt handling (Ctrl+C) |
| `CliOutput::info` / `success` / `warning` / `error` | `static void info(const std::string& msg)` and friends | human output; `info`/`success` follow the JSON mode, `warning`/`error` always go to stderr |
| `CliOutput::jsonBlock` | `static void jsonBlock(const nlohmann::json& value)` | writes the `=====JSON-BEGIN=====` block on stdout |
| `CliOutput::setJsonMode` / `isJsonMode` | `static void setJsonMode(bool on)`; `static bool isJsonMode()` | switch the stdout/stderr split |
| `CliOutput::setQuiet` / `setVerbose` / `isQuiet` | `static void setQuiet(bool quiet)` etc. | `--quiet` / `--verbose` |
| `CliOutput::table` / `title` / `separator` / `progress` | `static void table(const std::vector<std::string>& headers, const std::vector<std::vector<std::string>>& rows)` etc. | formatting helpers |

## Typical usage

### Adding an `info` command

1. Add the verb to `infoVerbs()` in `src/arg_parser.cpp` - `verbExists` and the help text both derive from that list;
2. Add a `if (cmd.verb == "your-verb") return cmdYourVerb(cmd);` branch in `CliDispatcher::dispatchInfo` and declare `int cmdYourVerb(const CliCommand& cmd);` in `cli_dispatcher.h`;
3. Implement the handler; collect data from `NeoWorkspace` / `NeoBuild`, then emit `{category, command, data}`:

```cpp
int CliDispatcher::cmdYourVerb(const CliCommand& cmd)
{
    nlohmann::json data = {{"branch", "base"}};
    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", cmd.verb}, {"data", data}});
    } else {
        CliOutput::info("Branch: base");
    }
    return 0;
}
```

4. Extend `ArgParser::printHelp` with the new verb so `--help` stays truthful;
5. Update the user-facing [CLI reference](../../../user/cli.md) and this page.

### Running a build from code

The dispatcher is a normal in-process API, so a test or a wrapper can drive it without a shell:

```cpp
NeoCLI::CliCommand cmd;
cmd.category = NeoCLI::CliCategory::Exec;
cmd.verb = "build";
cmd.options["repo"] = "https://example.com/repo.git";
cmd.options["modpack"] = "base";
cmd.options["git-branch"] = "main";
cmd.json = true;

int code = dispatcher.dispatch(cmd);
// 0 = built, 1 = build/export failure, 2 = usage error
```

`exec build` accepts `--repo`, `--modpack`, `--git-branch`, `--format` (`mcbbs` | `modrinth` | `hmcl` | `nsum`), `--export`, and `--embed-patterns`. `exec export` requires `--export` and `--format`; when `--repo` and `--modpack` are also given it builds first and exports the result, otherwise it looks for a previously built output under the current workspace (`hmcl` stops after the working-directory sync).

### The JSON protocol

With `--json` (or `cmd.json = true`) stdout carries exactly one block and nothing else:

```
=====JSON-BEGIN=====
{
  "category": "info",
  "command": "version",
  "data": { "version": "...", "internal_version": "...", "full_version": "...",
            "git_commit": "...", "build_type": "release" }
}
=====JSON-END=====
```

Every human line goes to stderr in that mode, so a consumer can split stdout at the markers and parse it directly. `CliOutput::info` and `CliOutput::success` follow this split; `CliOutput::warning` and `CliOutput::error` always write to stderr, even without `--json`.

## Notes

- **`--json` means "stdout is data only"**: never print diagnostics with `std::cout` in a handler, and never call `CLogger` with a sink that writes to stdout - the JSON block is the only payload.
- **Exit codes are part of the contract**: `0` success (including a cancelled build that reported a warning), `1` runtime failure (git, workspace, build, export), `2` usage error (unknown category/verb, missing required option, unimplemented verb).
- **A build always needs a Git branch**: `exec build` passes `--git-branch` into `BuildTarget::git_branch`; without it checkout falls back to the modpack branch, which usually is not a Git branch.
- **Registering a verb takes three places**: the verb table in `arg_parser.cpp`, the dispatch branch, and the help text. Skipping the first makes the parser reject the command before any handler runs.
- **The library initialises its own logger**: `dispatch` calls `CLogger::Init("builder.log", "builder")`, which is idempotent, so a host that already initialized it keeps its own file and logger name.
- **`flow gui` is not part of this library**: `dispatchFlow` implements `console` only; the host executable intercepts `flow gui`, builds a `GUIWorker::WizardWindow`, and prints the JSON block itself. Adding a GUI verb here would break the "engine only" boundary.
- **`CliOutput` is a static class** (`private` constructor deleted): do not try to instantiate it, and remember that `jsonMode`/`quiet`/`verbose` are process-wide flags.

## Related

- [README.md](./README.md)
- [CLI reference](../../../user/cli.md)
