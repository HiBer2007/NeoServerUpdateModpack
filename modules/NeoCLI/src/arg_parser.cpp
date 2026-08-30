#include "arg_parser.h"

#include <iostream>
#include <algorithm>

namespace NeoCLI {

static const char* VERSION = "1.0.0";

ArgParser::ArgParser() = default;

std::string ArgParser::version()
{
    return VERSION;
}

CliCategory ArgParser::categoryFromName(const std::string& name)
{
    if (name == "info") return CliCategory::Info;
    if (name == "flow") return CliCategory::Flow;
    if (name == "exec") return CliCategory::Exec;
    return CliCategory::None;
}

std::string ArgParser::categoryName(CliCategory category)
{
    switch (category) {
    case CliCategory::Info:   return "info";
    case CliCategory::Flow:   return "flow";
    case CliCategory::Exec:   return "exec";
    default:                  return "unknown";
    }
}

bool ArgParser::isHelpToken(const std::string& token)
{
    static const char* HELP_TOKENS[] = {
        "-h", "--help", "-help", "help", "/h", "/?", "-?"
    };
    for (const auto* t : HELP_TOKENS) {
        if (token == t) return true;
    }
    return false;
}

bool ArgParser::isVersionToken(const std::string& token)
{
    static const char* VERSION_TOKENS[] = { "-v", "--version", "/v" };
    for (const auto* t : VERSION_TOKENS) {
        if (token == t) return true;
    }
    return false;
}

static std::vector<std::string> infoVerbs()
{
    return {
        "version", "system", "git", "git-branches", "modpacks", "status",
        "workspace", "preview", "plugins", "exporters", "pointers",
        "history", "debug"
    };
}

static std::vector<std::string> flowVerbs()
{
    return { "gui", "console" };
}

static std::vector<std::string> execVerbs()
{
    return {
        "build", "export", "sync-serverconfig", "verify-repo",
        "resolve-pointer", "crash-test", "git-update",
        "repo-trust", "repo-trust-check"
    };
}

bool ArgParser::verbExists(CliCategory category, const std::string& verb)
{
    auto verbs = infoVerbs();
    if (category == CliCategory::Flow) verbs = flowVerbs();
    if (category == CliCategory::Exec) verbs = execVerbs();
    return std::find(verbs.begin(), verbs.end(), verb) != verbs.end();
}

CliCommand ArgParser::parse(int argc, char* argv[])
{
    CliCommand cmd;

    std::vector<std::string> tokens;
    for (int i = 1; i < argc; ++i) {
        tokens.emplace_back(argv[i]);
    }

    bool helpTok = false;
    bool verTok = false;
    for (const auto& t : tokens) {
        if (isHelpToken(t)) helpTok = true;
        else if (isVersionToken(t)) verTok = true;
    }

    if (verTok && !helpTok) {
        cmd.category = CliCategory::Version;
        return cmd;
    }

    if (helpTok) {
        cmd.help = true;
        if (!tokens.empty()) {
            auto c = categoryFromName(tokens[0]);
            if (c != CliCategory::None) cmd.category = c;
        }
        return cmd;
    }

    if (tokens.empty()) {
        cmd.error = "(empty)";
        return cmd;
    }

    cmd.category = categoryFromName(tokens[0]);
    if (cmd.category == CliCategory::None) {
        cmd.error = tokens[0];
        return cmd;
    }

    if (tokens.size() < 2) {
        cmd.error = tokens[0];
        return cmd;
    }

    cmd.verb = tokens[1];
    if (!verbExists(cmd.category, cmd.verb)) {
        cmd.error = cmd.verb;
        return cmd;
    }

    for (size_t i = 2; i < tokens.size(); ++i) {
        std::string t = tokens[i];

        if (!t.empty() && t[0] == '/') t[0] = '-';

        bool isOption = t.size() >= 2 && t[0] == '-';
        if (!isOption) {
            if (categoryFromName(t) != CliCategory::None) {
                cmd.error = t;
                return cmd;
            }
            cmd.positional.push_back(t);
            continue;
        }

        std::string key = t.substr(1);
        if (!key.empty() && key[0] == '-') key = key.substr(1);

        std::string val;
        auto eqPos = key.find('=');
        if (eqPos != std::string::npos) {
            val = key.substr(eqPos + 1);
            key = key.substr(0, eqPos);
        } else if (i + 1 < tokens.size()) {
            std::string next = tokens[i + 1];
            std::string normNext = next;
            if (!normNext.empty() && normNext[0] == '/') normNext[0] = '-';
            bool nextIsOption = (normNext.size() >= 2 && normNext[0] == '-')
                || isHelpToken(next) || isVersionToken(next);
            if (!nextIsOption) {
                val = next;
                ++i;
            }
        }

        if (key == "json") { cmd.json = true; continue; }
        if (key == "verbose") { cmd.verbose = true; continue; }
        if (key == "silent" || key == "quiet") { cmd.silent = true; continue; }
        if (key == "prefill") {
            if (!val.empty()) cmd.prefill.push_back(val);
            continue;
        }
        cmd.options[key] = val;
    }

    return cmd;
}

void ArgParser::printHelp() const
{
    std::cout
        << "NeoServerUpdateModpack CLI v" << VERSION << "\n"
        << "Usage: NeoServerUpdateModpack.exe <category> <verb> [options]\n\n"
        << "Categories (mutually exclusive):\n"
        << "  info    Information queries (read-only, structured output)\n"
        << "  flow    Guided wizard flows (GUI/console, reuses the GUI wizard)\n"
        << "  exec    Operational actions (build/export/sync/tools)\n\n"
        << "info commands:\n"
        << "  version                 Show software version\n"
        << "  system                  Show platform/directories/disk/git info\n"
        << "  git                     Show git executable path and version\n"
        << "  git-branches --repo <url>   List Git branches in the remote repository\n"
        << "  modpacks --repo <url>       List modpack branches from workspace.json\n"
        << "  status --repo <url> [--modpack <b>]   Show workspace status\n"
        << "  workspace --repo <url>       Show workspace.json metadata + inheritance chain\n"
        << "  preview --repo <url> --modpack <b> --format <f>   Preview build structure\n"
        << "  plugins                 List parser/pointer/exporter plugins\n"
        << "  exporters               List export formats and extra fields\n"
        << "  pointers --repo <url>   List pointer files\n"
        << "  history [--type local|remote|cache]   Show recent repository history\n"
        << "  debug                   Aggregated diagnostics\n\n"
        << "flow commands:\n"
        << "  gui [--from <page> --to <page> --prefill k=v ...]   Guided GUI wizard flow\n"
        << "  console [--from <page> --to <page> --prefill k=v ...]  Guided text wizard flow\n\n"
        << "exec commands:\n"
        << "  build --repo <url> --modpack <b> [--git-branch <b>] [--format <f>] [--export <p>]\n"
        << "  export --export <path> --format <f> [--repo <url> --modpack <b>]\n"
        << "  sync-serverconfig --save <world> [--repo <url> --git-branch <b>]\n"
        << "  verify-repo --repo <url>    Verify repository integrity\n"
        << "  resolve-pointer <file>      Resolve pointer file to download URL\n"
        << "  crash-test                  Trigger a test crash (crash handler validation)\n"
        << "  git-update                  Install bundled Git and write install config\n"
        << "  repo-trust --repo <path>    Trust a repository (safe.directory)\n"
        << "  repo-trust-check --repo <path>   Check whether a repository is trusted\n\n"
        << "Global options:\n"
        << "  --json                  Emit result as a JSON block (BEGIN/END markers)\n"
        << "  --verbose               Verbose output\n"
        << "  --silent                Silent mode (errors only)\n\n"
        << "Help & version:\n"
        << "  -h, --help, -help, help, /h, /?, -?   Show this help\n"
        << "  -v, --version, /v                     Show version\n\n"
        << "Note: <page> in: repo|branch|modpack|export-type|export-dir|extra-info|"
        << "checklist|build|done\n"
        << std::endl;
}

void ArgParser::printHelp(CliCategory category) const
{
    if (category == CliCategory::Info) {
        std::cout
            << "NeoServerUpdateModpack CLI v" << VERSION << " - info\n\n"
            << "info commands:\n"
            << "  version                 Show software version\n"
            << "  system                  Show platform/directories/disk/git info\n"
            << "  git                     Show git executable path and version\n"
            << "  git-branches --repo <url>   List Git branches in the remote repository\n"
            << "  modpacks --repo <url>       List modpack branches from workspace.json\n"
            << "  status --repo <url> [--modpack <b>]   Show workspace status\n"
            << "  workspace --repo <url>       Show workspace.json metadata + inheritance chain\n"
            << "  preview --repo <url> --modpack <b> --format <f>   Preview build structure\n"
            << "  plugins                 List parser/pointer/exporter plugins\n"
            << "  exporters               List export formats and extra fields\n"
            << "  pointers --repo <url>   List pointer files\n"
            << "  history [--type local|remote|cache]   Show recent repository history\n"
            << "  debug                   Aggregated diagnostics\n\n"
            << "Use --json for structured output. See docs/CLI/ for JSON schema.\n"
            << std::endl;
    } else if (category == CliCategory::Flow) {
        std::cout
            << "NeoServerUpdateModpack CLI v" << VERSION << " - flow\n\n"
            << "flow commands:\n"
            << "  gui [--from <page> --to <page> --prefill k=v ...]   Guided GUI wizard flow\n"
            << "  console [--from <page> --to <page> --prefill k=v ...]  Guided text wizard flow\n\n"
            << "Pages: repo|branch|modpack|export-type|export-dir|extra-info|checklist|build|done\n"
            << "See docs/CLI/ for flow semantics and JSON output.\n"
            << std::endl;
    } else if (category == CliCategory::Exec) {
        std::cout
            << "NeoServerUpdateModpack CLI v" << VERSION << " - exec\n\n"
            << "exec commands:\n"
            << "  build --repo <url> --modpack <b> [--git-branch <b>] [--format <f>] [--export <p>]\n"
            << "  export --export <path> --format <f> [--repo <url> --modpack <b>]\n"
            << "  sync-serverconfig --save <world> [--repo <url> --git-branch <b>]\n"
            << "  verify-repo --repo <url>    Verify repository integrity\n"
            << "  resolve-pointer <file>      Resolve pointer file to download URL\n"
            << "  crash-test                  Trigger a test crash (crash handler validation)\n"
            << "  git-update                  Install bundled Git and write install config\n"
            << "  repo-trust --repo <path>    Trust a repository (safe.directory)\n"
            << "  repo-trust-check --repo <path>   Check whether a repository is trusted\n\n"
            << "See docs/CLI/ for details.\n"
            << std::endl;
    } else {
        printHelp();
    }
}

void ArgParser::printVersion() const
{
    std::cout << "NeoServerUpdateModpack CLI v" << VERSION << std::endl;
}

} // namespace NeoCLI
