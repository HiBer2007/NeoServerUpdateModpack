#include "editor_cli.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <fcntl.h>
#include <io.h>
#endif

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <workspace_manager.h>

namespace fs = std::filesystem;
using nlohmann::json;

namespace editor_cli {

namespace {

constexpr int kMaxTreeEntries = 5000;

struct CliFailure {
    std::string message;
    int code = 1;
};

[[noreturn]] void fail(const std::string& message, int code = 1)
{
    throw CliFailure{message, code};
}

fs::path u8path(const std::string& value)
{
    return fs::u8path(value);
}

std::string toU8(const fs::path& path)
{
    return path.u8string();
}

bool endsWithI(const std::string& value, const std::string& suffix)
{
    if (value.size() < suffix.size()) return false;
    for (size_t i = 0; i < suffix.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(
            value[value.size() - suffix.size() + i]);
        const unsigned char b = static_cast<unsigned char>(suffix[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}

std::string readTextFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) fail("cannot open file: " + toU8(path));
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (in.bad()) fail("cannot read file: " + toU8(path));
    return buffer.str();
}

void writeTextFile(const fs::path& path, const std::string& content)
{
    std::error_code ec;
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) fail("cannot write file: " + toU8(path));
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
    if (out.bad()) fail("cannot write file: " + toU8(path));
}

json loadJson(const fs::path& path)
{
    const std::string raw = readTextFile(path);
    try {
        return json::parse(raw);
    } catch (const json::exception& e) {
        fail("invalid JSON in " + toU8(path) + ": " + e.what());
    }
}

void writeJson(const fs::path& path, const json& value)
{
    writeTextFile(path, value.dump(2) + "\n");
}

json defaultBranchConfig()
{
    json j;
    j["file_manifest"] = json::object();
    j["pointer_files"] = json::object();
    j["serverconfig_sync"] = {
        {"enabled", false},
        {"scan_paths", json::array()},
        {"rules", json::array()}
    };
    j["custom_mods"] = {
        {"enabled", false},
        {"path", ""},
        {"merge_order", "post"}
    };
    return j;
}

struct ParsedArgs {
    std::string command;
    std::string verb;
    std::vector<std::string> positional;
    std::map<std::string, std::string> options;
    std::set<std::string> flags;

    bool has(const std::string& key) const
    {
        return options.count(key) != 0;
    }

    std::string get(const std::string& key, const std::string& fallback = "") const
    {
        auto it = options.find(key);
        return it == options.end() ? fallback : it->second;
    }

    bool flag(const std::string& key) const
    {
        return flags.count(key) != 0;
    }
};

ParsedArgs parseArgs(const std::vector<std::string>& tokens)
{
    ParsedArgs parsed;
    size_t index = 0;
    if (!tokens.empty() && tokens[0] != "") {
        parsed.command = tokens[0];
        index = 1;
    }
    if (index < tokens.size() && tokens[index].rfind("--", 0) != 0) {
        parsed.verb = tokens[index];
        ++index;
    }

    for (; index < tokens.size(); ++index) {
        const std::string& token = tokens[index];
        if (token.rfind("--", 0) != 0) {
            parsed.positional.push_back(token);
            continue;
        }
        std::string name = token.substr(2);
        std::string value;
        const size_t eq = name.find('=');
        if (eq != std::string::npos) {
            value = name.substr(eq + 1);
            name = name.substr(0, eq);
        }
        if (name.empty()) continue;
        if (!value.empty()) {
            parsed.options[name] = value;
            continue;
        }
        if (index + 1 < tokens.size() && tokens[index + 1].rfind("--", 0) != 0) {
            parsed.options[name] = tokens[++index];
        } else {
            parsed.flags.insert(name);
        }
    }
    return parsed;
}

std::string currentDirectory()
{
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (ec) fail("cannot resolve current directory");
    return toU8(cwd);
}

fs::path resolveRepo(const ParsedArgs& args)
{
    const std::string value = args.get("repo", currentDirectory());
    fs::path repo = u8path(value);
    std::error_code ec;
    repo = fs::absolute(repo, ec);
    if (ec || !fs::is_directory(repo)) fail("repository directory not found: " + value);
    const fs::path workspaceJson = repo / "workspace.json";
    if (!fs::is_regular_file(workspaceJson))
        fail("workspace.json not found in repository: " + toU8(repo));
    return repo;
}

fs::path resolveInside(const fs::path& repo, const std::string& relative)
{
    if (relative.empty()) fail("path must not be empty");
    fs::path part = u8path(relative);
    if (part.is_absolute()) fail("absolute path is not allowed: " + relative);
    for (const auto& component : part) {
        if (component == "..") fail("path must not contain '..': " + relative);
    }
    return (repo / part).lexically_normal();
}

bool isUtf8Valid(const std::string& data)
{
    size_t i = 0;
    while (i < data.size()) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        size_t extra = 0;
        if (c < 0x80) { ++i; continue; }
        else if ((c & 0xE0) == 0xC0) extra = 1;
        else if ((c & 0xF0) == 0xE0) extra = 2;
        else if ((c & 0xF8) == 0xF0) extra = 3;
        else return false;
        if (i + extra >= data.size()) return false;
        for (size_t k = 1; k <= extra; ++k) {
            const unsigned char cc = static_cast<unsigned char>(data[i + k]);
            if ((cc & 0xC0) != 0x80) return false;
        }
        i += extra + 1;
    }
    return true;
}

std::string base64Encode(const std::string& input)
{
    static const char* table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((input.size() + 2) / 3 * 4);
    size_t i = 0;
    while (i + 2 < input.size()) {
        const unsigned int block =
            (static_cast<unsigned char>(input[i]) << 16)
            | (static_cast<unsigned char>(input[i + 1]) << 8)
            | static_cast<unsigned char>(input[i + 2]);
        out.push_back(table[(block >> 18) & 0x3F]);
        out.push_back(table[(block >> 12) & 0x3F]);
        out.push_back(table[(block >> 6) & 0x3F]);
        out.push_back(table[block & 0x3F]);
        i += 3;
    }
    if (i + 1 == input.size()) {
        const unsigned int block = static_cast<unsigned char>(input[i]) << 16;
        out.push_back(table[(block >> 18) & 0x3F]);
        out.push_back(table[(block >> 12) & 0x3F]);
        out += "==";
    } else if (i + 2 == input.size()) {
        const unsigned int block =
            (static_cast<unsigned char>(input[i]) << 16)
            | (static_cast<unsigned char>(input[i + 1]) << 8);
        out.push_back(table[(block >> 18) & 0x3F]);
        out.push_back(table[(block >> 12) & 0x3F]);
        out.push_back(table[(block >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

json branchesOf(const json& workspace)
{
    if (workspace.contains("branches") && workspace["branches"].is_array())
        return workspace["branches"];
    return json::array();
}

int findBranch(const json& workspace, const std::string& name)
{
    const json branches = branchesOf(workspace);
    for (size_t i = 0; i < branches.size(); ++i) {
        if (branches[i].value("name", "") == name) return static_cast<int>(i);
    }
    return -1;
}

std::string branchParent(const json& workspace, const std::string& name)
{
    const int index = findBranch(workspace, name);
    if (index < 0) return "";
    const json& branch = workspace["branches"][static_cast<size_t>(index)];
    if (branch.contains("parent") && branch["parent"].is_string())
        return branch["parent"].get<std::string>();
    return "";
}

void validateParentChain(const json& workspace, const std::string& name,
                         const std::string& parent)
{
    if (parent.empty()) return;
    if (parent == name) fail("branch cannot be its own parent: " + name);
    if (findBranch(workspace, parent) < 0)
        fail("parent branch not found: " + parent);
    std::string cursor = parent;
    std::set<std::string> seen{name};
    while (!cursor.empty()) {
        if (!seen.insert(cursor).second)
            fail("parent chain forms a cycle at branch: " + cursor);
        cursor = branchParent(workspace, cursor);
    }
}

std::vector<std::string> childrenOf(const json& workspace, const std::string& name)
{
    std::vector<std::string> children;
    for (const auto& branch : branchesOf(workspace)) {
        if (branch.value("parent", "") == name)
            children.push_back(branch.value("name", ""));
    }
    return children;
}

fs::path workspacePath(const fs::path& repo)
{
    return repo / "workspace.json";
}

fs::path branchConfigPath(const fs::path& repo, const std::string& branch)
{
    return repo / "branch_config" / (branch + ".json");
}

fs::path branchDataPath(const fs::path& repo, const std::string& branch)
{
    return repo / "branches" / branch;
}

json loadBranchConfigOrDefault(const fs::path& repo, const std::string& branch)
{
    const fs::path path = branchConfigPath(repo, branch);
    if (!fs::is_regular_file(path)) return defaultBranchConfig();
    json config = loadJson(path);
    if (!config.is_object()) config = json::object();
    for (const auto& key : {"file_manifest", "pointer_files"}) {
        if (!config.contains(key) || !config[key].is_object())
            config[key] = json::object();
    }
    if (!config.contains("serverconfig_sync") || !config["serverconfig_sync"].is_object())
        config["serverconfig_sync"] = defaultBranchConfig()["serverconfig_sync"];
    if (!config.contains("custom_mods") || !config["custom_mods"].is_object())
        config["custom_mods"] = defaultBranchConfig()["custom_mods"];
    return config;
}

void saveBranchConfig(const fs::path& repo, const std::string& branch, const json& config)
{
    const fs::path path = branchConfigPath(repo, branch);
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    writeJson(path, config);
}

json branchSummary(const fs::path& repo, const json& workspace, const json& branch)
{
    json out = branch;
    const std::string name = branch.value("name", "");
    out["has_data_dir"] = fs::is_directory(branchDataPath(repo, name));
    out["has_config"] = fs::is_regular_file(branchConfigPath(repo, name));
    return out;
}

json workspaceSummary(const fs::path& repo, const json& workspace)
{
    json branches = json::array();
    for (const auto& branch : branchesOf(workspace))
        branches.push_back(branchSummary(repo, workspace, branch));

    json git = json::object();
    if (workspace.contains("git") && workspace["git"].is_object()) {
        git = workspace["git"];
    }

    json data;
    const json workspaceInfo =
        (workspace.contains("workspace") && workspace["workspace"].is_object())
        ? workspace["workspace"] : json::object();
    data["workspace_file"] = toU8(workspacePath(repo));
    data["repo"] = toU8(repo);
    data["name"] = workspaceInfo.value("name", workspace.value("name", ""));
    data["minecraft"] = workspaceInfo.value("minecraft", workspace.value("minecraft", ""));
    data["modloader"] = workspaceInfo.value("modloader", workspace.value("modloader", ""));
    data["modloader_version"] =
        workspaceInfo.value("modloader_version", workspace.value("modloader_version", ""));
    data["git"] = git;
    data["branches"] = branches;
    data["branch_count"] = branches.size();
    if (workspace.contains("sync_policies")) data["sync_policies"] = workspace["sync_policies"];
    if (workspace.contains("export")) data["export"] = workspace["export"];
    return data;
}

json treeOf(const fs::path& repo, const fs::path& root, int depth, bool includeAll)
{
    json entries = json::array();
    bool truncated = false;
    if (!fs::is_directory(root)) fail("directory not found: " + toU8(root));

    std::error_code ec;
    fs::recursive_directory_iterator iterator(
        root, fs::directory_options::skip_permission_denied, ec);
    if (ec) fail("cannot scan directory: " + toU8(root));

    for (fs::recursive_directory_iterator end; iterator != end; iterator.increment(ec)) {
        if (ec) break;
        const fs::path path = iterator->path();
        if (path.filename() == ".git") {
            iterator.disable_recursion_pending();
            continue;
        }
        if (path.filename() == ".NSUM") {
            iterator.disable_recursion_pending();
            continue;
        }
        const std::string relative = toU8(fs::relative(path, repo, ec));
        if (ec) continue;
        const int currentDepth = static_cast<int>(
            std::distance(path.begin(), path.end()))
            - static_cast<int>(std::distance(root.begin(), root.end()));
        json entry;
        entry["path"] = toU8(relative);
        entry["type"] = fs::is_directory(path) ? "dir" : "file";
        if (fs::is_regular_file(path)) entry["size"] = static_cast<long long>(fs::file_size(path, ec));
        entries.push_back(entry);
        if (entries.size() >= static_cast<size_t>(kMaxTreeEntries)) {
            truncated = true;
            break;
        }
        if (fs::is_directory(path) && currentDepth >= depth)
            iterator.disable_recursion_pending();
    }

    json data;
    data["root"] = toU8(fs::relative(root, repo, ec));
    if (data["root"] == ".") data["root"] = "";
    data["depth"] = depth;
    data["entries"] = entries;
    data["truncated"] = truncated;
    return data;
}

std::string requireOption(const ParsedArgs& args, const std::string& key)
{
    if (!args.has(key)) fail("missing option --" + key, 2);
    return args.get(key);
}

json jsonPointerSet(json& root, const std::string& pointer, const json& value, bool remove)
{
    if (pointer.empty() || pointer == "/") fail("empty JSON pointer is not allowed");
    std::vector<std::string> parts;
    size_t start = 0;
    if (pointer[0] != '/') fail("JSON pointer must start with '/': " + pointer);
    for (size_t i = 1; i <= pointer.size(); ++i) {
        if (i == pointer.size() || pointer[i] == '/') {
            std::string part = pointer.substr(start + 1, i - start - 1);
            std::string decoded;
            for (size_t k = 0; k < part.size(); ++k) {
                if (part[k] == '~' && k + 1 < part.size()) {
                    if (part[k + 1] == '0') { decoded.push_back('~'); ++k; continue; }
                    if (part[k + 1] == '1') { decoded.push_back('/'); ++k; continue; }
                }
                decoded.push_back(part[k]);
            }
            parts.push_back(decoded);
            start = i;
        }
    }

    json* node = &root;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        const std::string& key = parts[i];
        if (node->is_array()) {
            bool numeric = !key.empty();
            for (char c : key) numeric = numeric && std::isdigit(static_cast<unsigned char>(c));
            if (!numeric) fail("array requires numeric index at: " + key);
            const size_t index = static_cast<size_t>(std::stoull(key));
            if (index >= node->size()) fail("array index out of range: " + key);
            node = &(*node)[index];
            continue;
        }
        if (!node->is_object()) *node = json::object();
        if (!node->contains(key)) {
            bool nextNumeric = !parts[i + 1].empty();
            for (char c : parts[i + 1])
                nextNumeric = nextNumeric && std::isdigit(static_cast<unsigned char>(c));
            (*node)[key] = nextNumeric ? json::array() : json::object();
        }
        node = &(*node)[key];
    }

    const std::string& last = parts.back();
    if (remove) {
        if (node->is_array()) {
            const size_t index = static_cast<size_t>(std::stoull(last));
            if (index >= node->size()) fail("array index out of range: " + last);
            node->erase(index);
        } else if (node->is_object()) {
            node->erase(last);
        } else {
            fail("cannot delete from non-container value");
        }
        return json{{"removed", true}};
    }

    if (node->is_array()) {
        const size_t index = static_cast<size_t>(std::stoull(last));
        if (index >= node->size()) node->push_back(value);
        else (*node)[index] = value;
    } else {
        if (!node->is_object()) *node = json::object();
        (*node)[last] = value;
    }
    return json{{"removed", false}, {"value", value}};
}

json jsonPointerGet(const json& root, const std::string& pointer)
{
    if (pointer.empty() || pointer == "/") return root;
    json current = root;
    size_t start = 0;
    if (pointer[0] != '/') fail("JSON pointer must start with '/': " + pointer);
    for (size_t i = 1; i <= pointer.size(); ++i) {
        if (i != pointer.size() && pointer[i] != '/') continue;
        const std::string part = pointer.substr(start + 1, i - start - 1);
        if (current.is_array()) {
            const size_t index = static_cast<size_t>(std::stoull(part));
            if (index >= current.size()) fail("array index out of range: " + part);
            current = current[index];
        } else if (current.is_object()) {
            if (!current.contains(part)) fail("JSON pointer not found: " + pointer);
            current = current[part];
        } else {
            fail("JSON pointer not found: " + pointer);
        }
        start = i;
    }
    return current;
}

std::vector<std::string> listPointerFiles(const fs::path& repo)
{
    std::vector<std::string> files;
    const fs::path root = repo / "branch_config";
    if (!fs::is_directory(root)) return files;
    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        if (!endsWithI(toU8(entry.path().filename()), ".pointer")) continue;
        files.push_back(toU8(entry.path()));
    }
    return files;
}

json pointersInfo(const fs::path& repo, const json& workspace)
{
    json items = json::array();
    std::set<std::string> seen;

    for (const auto& file : listPointerFiles(repo)) {
        json parsed;
        try {
            parsed = loadJson(u8path(file));
        } catch (const CliFailure&) {
            continue;
        }
        const std::string sha = parsed.value("sha256", "");
        if (sha.empty()) continue;
        json item;
        item["file"] = toU8(fs::relative(u8path(file), repo));
        item["sha256"] = sha;
        item["resolver"] = parsed.value("resolver", "");
        if (parsed.contains("metadata")) item["metadata"] = parsed["metadata"];
        item["targets"] = json::array();
        items.push_back(item);
        seen.insert(sha);
    }

    for (const auto& branch : branchesOf(workspace)) {
        const std::string name = branch.value("name", "");
        if (name.empty()) continue;
        const json config = loadBranchConfigOrDefault(repo, name);
        if (!config["file_manifest"].is_object()) continue;
        for (const auto& [rel, value] : config["file_manifest"].items()) {
            if (!value.is_string()) continue;
            const std::string sha = value.get<std::string>();
            json* target = nullptr;
            for (auto& item : items) {
                if (item.value("sha256", "") == sha) { target = &item; break; }
            }
            if (target == nullptr) {
                json item;
                item["file"] = "";
                item["sha256"] = sha;
                item["resolver"] = "";
                item["targets"] = json::array();
                items.push_back(item);
                target = &items.back();
            }
            target->operator[]("targets").push_back({{"branch", name}, {"path", rel}});
        }
    }

    json data;
    data["pointers"] = items;
    data["count"] = items.size();
    return data;
}

json syncInfo(const fs::path& repo, const json& workspace)
{
    json data;
    if (workspace.contains("sync_policies")) data["sync_policies"] = workspace["sync_policies"];
    json branches = json::array();
    for (const auto& branch : branchesOf(workspace)) {
        const std::string name = branch.value("name", "");
        if (name.empty()) continue;
        const json config = loadBranchConfigOrDefault(repo, name);
        json entry;
        entry["branch"] = name;
        entry["serverconfig_sync"] = config.value("serverconfig_sync", json::object());
        entry["custom_mods"] = config.value("custom_mods", json::object());
        branches.push_back(entry);
    }
    data["branches"] = branches;
    return data;
}

json fileInfo(const fs::path& repo, const std::string& relative, bool base64, bool force)
{
    const fs::path path = resolveInside(repo, relative);
    if (!fs::is_regular_file(path)) fail("file not found: " + relative);
    std::string raw = readTextFile(path);
    const bool binary = raw.find('\0') != std::string::npos || !isUtf8Valid(raw);
    json data;
    data["path"] = toU8(fs::relative(path, repo));
    data["size"] = static_cast<long long>(raw.size());
    data["binary"] = binary;
    if (binary && !base64 && !force) {
        data["content"] = nullptr;
        data["hint"] = "binary file; pass --base64 or --force to read bytes";
        return data;
    }
    if (binary) {
        data["encoding"] = "base64";
        data["content"] = base64Encode(raw);
    } else {
        data["encoding"] = "utf-8";
        data["content"] = raw;
    }
    return data;
}

std::string commandJsonName(const ParsedArgs& args)
{
    if (args.command == "info")
        return "info." + (args.verb.empty() ? std::string("workspace") : args.verb);
    if (args.command == "exec")
        return "exec." + args.verb;
    return args.command;
}

void printUsage()
{
    std::cout <<
        "NeoWorkspaceEditor CLI (AI/headless)\n"
        "\n"
        "Usage:\n"
        "  NeoWorkspaceEditor.exe info workspace [--repo <dir>] [--text]\n"
        "  NeoWorkspaceEditor.exe info branches  [--repo <dir>] [--text]\n"
        "  NeoWorkspaceEditor.exe info branch <name> [--repo <dir>]\n"
        "  NeoWorkspaceEditor.exe info tree [--path <rel>] [--depth <n>] [--repo <dir>]\n"
        "  NeoWorkspaceEditor.exe info pointers [--repo <dir>]\n"
        "  NeoWorkspaceEditor.exe info sync [--repo <dir>]\n"
        "  NeoWorkspaceEditor.exe info file <rel> [--base64] [--force]\n"
        "  NeoWorkspaceEditor.exe exec branch-add --name <n> [--parent <p>] [--type universal|client|server]\n"
        "        [--core <id>] [--description <s>] [--modloader-version <v>] [--game-version <v>]\n"
        "  NeoWorkspaceEditor.exe exec branch-set --name <n> [--parent <p>|--parent \"\"] [--type ...]\n"
        "        [--core <id>] [--description <s>] [--modloader-version <v>] [--hidden true|false] [--game-version <v>]\n"
        "  NeoWorkspaceEditor.exe exec branch-rename --name <n> --to <new>\n"
        "  NeoWorkspaceEditor.exe exec branch-remove --name <n> [--force] [--purge]\n"
        "  NeoWorkspaceEditor.exe exec file-write --path <rel> (--content <text> | --from-file <src>) [--append]\n"
        "  NeoWorkspaceEditor.exe exec file-delete --path <rel> [--recursive]\n"
        "  NeoWorkspaceEditor.exe exec file-move --from <rel> --to <rel>\n"
        "  NeoWorkspaceEditor.exe exec json-get --file <rel> [--pointer /a/b]\n"
        "  NeoWorkspaceEditor.exe exec json-set --file <rel> --pointer /a/b (--value <json>|--value-string <s>|--delete)\n"
        "  NeoWorkspaceEditor.exe exec pointer-add --path <rel> --sha256 <hex> --resolver <name>\n"
        "        [--url <url>] [--branch <name>] [--metadata-json <json>]\n"
        "  NeoWorkspaceEditor.exe exec pointer-remove --path <rel> [--branch <name>]\n"
        "  NeoWorkspaceEditor.exe exec validate [--repo <dir>]\n"
        "  NeoWorkspaceEditor.exe help\n"
        "\n"
        "All commands print one JSON object on stdout (ok/command/data or error).\n"
        "--repo defaults to the current directory; --text prints a short human summary.\n";
}

int execute(const ParsedArgs& args, const std::string& commandName,
            json& outData, bool& textMode);

int runInfo(const ParsedArgs& args, json& outData)
{
    const fs::path repo = resolveRepo(args);
    const json workspace = loadJson(workspacePath(repo));

    if (args.verb.empty() || args.verb == "workspace") {
        outData = workspaceSummary(repo, workspace);
        return 0;
    }
    if (args.verb == "branches") {
        json branches = json::array();
        for (const auto& branch : branchesOf(workspace))
            branches.push_back(branchSummary(repo, workspace, branch));
        outData = {{"count", branches.size()}, {"branches", branches}};
        return 0;
    }
    if (args.verb == "branch") {
        if (args.positional.empty()) fail("info branch requires a branch name", 2);
        const std::string name = args.positional[0];
        const int index = findBranch(workspace, name);
        if (index < 0) fail("branch not found: " + name);
        const json branch = workspace["branches"][static_cast<size_t>(index)];
        const json config = loadBranchConfigOrDefault(repo, name);
        json data;
        data["branch"] = branchSummary(repo, workspace, branch);
        data["children"] = childrenOf(workspace, name);
        data["branch_config"] = {
            {"file", toU8(fs::relative(branchConfigPath(repo, name), repo))},
            {"exists", fs::is_regular_file(branchConfigPath(repo, name))},
            {"file_manifest_count", config["file_manifest"].size()},
            {"pointer_files_count", config["pointer_files"].size()},
            {"serverconfig_sync", config["serverconfig_sync"]},
            {"custom_mods", config["custom_mods"]}
        };
        long long fileCount = 0;
        const fs::path dataDir = branchDataPath(repo, name);
        if (fs::is_directory(dataDir)) {
            for (const auto& entry : fs::recursive_directory_iterator(dataDir)) {
                if (entry.is_regular_file()) ++fileCount;
            }
        }
        data["file_count"] = fileCount;
        outData = data;
        return 0;
    }
    if (args.verb == "tree") {
        const int depth = std::max(0, std::atoi(args.get("depth", "2").c_str()));
        fs::path root = repo;
        if (args.has("path")) root = resolveInside(repo, args.get("path"));
        outData = treeOf(repo, root, depth, args.flag("all"));
        return 0;
    }
    if (args.verb == "pointers") {
        outData = pointersInfo(repo, workspace);
        return 0;
    }
    if (args.verb == "sync") {
        outData = syncInfo(repo, workspace);
        return 0;
    }
    if (args.verb == "file") {
        if (args.positional.empty()) fail("info file requires a relative path", 2);
        outData = fileInfo(repo, args.positional[0], args.flag("base64"), args.flag("force"));
        return 0;
    }
    fail("unknown info subcommand: " + args.verb, 2);
}

int runExec(const ParsedArgs& args, json& outData)
{
    const fs::path repo = resolveRepo(args);
    const std::string verb = args.verb;

    if (verb == "validate") {
        NeoWorkspace::WorkspaceManager manager;
        const std::string path = toU8(workspacePath(repo));
        const bool loaded = manager.loadFromFile(path);
        const bool managerValid = loaded && manager.validate();
        json data;
        data["loaded"] = loaded;
        data["workspace_manager_valid"] = managerValid;
        data["valid"] = loaded;
        data["branches"] = json::array();
        if (loaded) {
            for (const auto& branch : manager.branches()) {
                json entry;
                entry["name"] = branch.name;
                entry["parent"] = branch.parent;
                entry["type"] = branch.type;
                entry["core"] = branch.core;
                entry["has_data_dir"] = fs::is_directory(branchDataPath(repo, branch.name));
                entry["has_config"] = fs::is_regular_file(branchConfigPath(repo, branch.name));
                data["branches"].push_back(entry);
            }
        }
        json workspace = loadJson(workspacePath(repo));
        json errors = json::array();
        json warnings = json::array();
        std::set<std::string> names;
        for (const auto& branch : branchesOf(workspace)) {
            const std::string name = branch.value("name", "");
            if (name.empty()) {
                errors.push_back("branch entry without name");
                continue;
            }
            if (!names.insert(name).second)
                errors.push_back("duplicate branch name: " + name);
            const std::string parent = branch.value("parent", "");
            if (!parent.empty() && findBranch(workspace, parent) < 0)
                errors.push_back("missing parent branch: " + parent);
            if (!fs::is_directory(branchDataPath(repo, name)))
                warnings.push_back("missing branch directory: " + name);
            if (!fs::is_regular_file(branchConfigPath(repo, name)))
                warnings.push_back("missing branch_config file: " + name);
        }
        for (const auto& branch : branchesOf(workspace)) {
            const std::string name = branch.value("name", "");
            if (name.empty()) continue;
            std::set<std::string> seen{name};
            std::string cursor = branch.value("parent", "");
            while (!cursor.empty()) {
                if (!seen.insert(cursor).second) {
                    errors.push_back("parent cycle involving branch: " + name);
                    break;
                }
                cursor = branchParent(workspace, cursor);
            }
        }
        data["errors"] = errors;
        data["warnings"] = warnings;
        data["valid"] = loaded && errors.empty();
        outData = data;
        return data["valid"].get<bool>() ? 0 : 1;
    }

    json workspace = loadJson(workspacePath(repo));

    if (verb == "branch-add") {
        const std::string name = requireOption(args, "name");
        if (findBranch(workspace, name) >= 0) fail("branch already exists: " + name);
        const std::string parent = args.get("parent", "");
        validateParentChain(workspace, name, parent);
        const std::string type = args.get("type", "universal");
        if (type != "universal" && type != "client" && type != "server")
            fail("invalid --type (universal|client|server): " + type);
        const std::string core = args.get("core", "");
        if (!core.empty() && type != "server")
            fail("--core is only valid for server branches");
        if (core.empty() && type == "server")
            fail("server branch requires --core", 2);

        json branch = json::object();
        branch["name"] = name;
        branch["parent"] = parent.empty() ? json(nullptr) : json(parent);
        branch["description"] = args.get("description", "");
        branch["hidden"] = args.flag("hidden");
        branch["type"] = type;
        if (type == "server" && !core.empty()) branch["core"] = core;
        if (args.has("modloader-version"))
            branch["modloader_version"] = args.get("modloader-version");
        if (args.has("game-version")) branch["game_version"] = args.get("game-version");

        if (!parent.empty()) {
            const int parentIndex = findBranch(workspace, parent);
            if (parentIndex >= 0) {
                const json& parentBranch = workspace["branches"][static_cast<size_t>(parentIndex)];
                for (const auto& key : {"game_version", "modloader", "modloader_version"}) {
                    if (!branch.contains(key) && parentBranch.contains(key))
                        branch[key] = parentBranch[key];
                }
            }
        }

        if (!workspace.contains("branches") || !workspace["branches"].is_array())
            workspace["branches"] = json::array();
        workspace["branches"].push_back(branch);
        writeJson(workspacePath(repo), workspace);

        std::error_code ec;
        fs::create_directories(branchDataPath(repo, name), ec);
        if (!fs::is_regular_file(branchConfigPath(repo, name)))
            saveBranchConfig(repo, name, defaultBranchConfig());

        outData = branchSummary(repo, workspace, branch);
        return 0;
    }

    if (verb == "branch-set") {
        const std::string name = requireOption(args, "name");
        const int index = findBranch(workspace, name);
        if (index < 0) fail("branch not found: " + name);
        json& branch = workspace["branches"][static_cast<size_t>(index)];

        if (args.has("parent")) {
            const std::string parent = args.get("parent");
            validateParentChain(workspace, name, parent);
            branch["parent"] = parent.empty() ? json(nullptr) : json(parent);
        }
        if (args.has("type")) {
            const std::string type = args.get("type");
            if (type != "universal" && type != "client" && type != "server")
                fail("invalid --type (universal|client|server): " + type);
            branch["type"] = type;
        }
        const std::string type = branch.value("type", "universal");
        if (args.has("core")) {
            if (type != "server") fail("--core is only valid for server branches");
            branch["core"] = args.get("core");
        }
        if (type != "server") branch.erase("core");
        if (args.has("description")) branch["description"] = args.get("description");
        if (args.has("modloader-version"))
            branch["modloader_version"] = args.get("modloader-version");
        if (args.has("game-version")) branch["game_version"] = args.get("game-version");
        if (args.has("hidden")) {
            const std::string value = args.get("hidden");
            branch["hidden"] = (value == "true" || value == "1" || value == "yes");
        }

        writeJson(workspacePath(repo), workspace);
        outData = branchSummary(repo, workspace, branch);
        return 0;
    }

    if (verb == "branch-rename") {
        const std::string name = requireOption(args, "name");
        const std::string to = requireOption(args, "to");
        if (name == to) fail("new branch name equals old name");
        const int index = findBranch(workspace, name);
        if (index < 0) fail("branch not found: " + name);
        if (findBranch(workspace, to) >= 0) fail("target branch already exists: " + to);

        workspace["branches"][static_cast<size_t>(index)]["name"] = to;
        for (auto& branch : workspace["branches"]) {
            if (branch.value("parent", "") == name) branch["parent"] = to;
        }
        writeJson(workspacePath(repo), workspace);

        std::error_code ec;
        if (fs::is_directory(branchDataPath(repo, name))) {
            fs::rename(branchDataPath(repo, name), branchDataPath(repo, to), ec);
            if (ec) fail("cannot rename branch directory: " + ec.message());
        }
        if (fs::is_regular_file(branchConfigPath(repo, name))) {
            fs::create_directories(branchConfigPath(repo, to).parent_path(), ec);
            fs::rename(branchConfigPath(repo, name), branchConfigPath(repo, to), ec);
            if (ec) fail("cannot rename branch_config: " + ec.message());
        }
        outData = {{"renamed", name}, {"to", to}};
        return 0;
    }

    if (verb == "branch-remove") {
        const std::string name = requireOption(args, "name");
        const int index = findBranch(workspace, name);
        if (index < 0) fail("branch not found: " + name);
        const std::vector<std::string> children = childrenOf(workspace, name);
        if (!children.empty() && !args.flag("force"))
            fail("branch has children: " + children[0] + " (use --force to reparent)");
        const std::string parent = args.flag("force")
            ? branchParent(workspace, name) : "";
        if (!children.empty()) {
            for (auto& branch : workspace["branches"]) {
                if (branch.value("parent", "") == name) {
                    branch["parent"] = parent.empty() ? json(nullptr) : json(parent);
                }
            }
        }
        workspace["branches"].erase(index);
        writeJson(workspacePath(repo), workspace);

        std::error_code ec;
        fs::remove(branchConfigPath(repo, name), ec);
        if (args.flag("purge")) {
            fs::remove_all(branchDataPath(repo, name), ec);
        }
        outData = {{"removed", name}, {"purged", args.flag("purge")}};
        return 0;
    }

    if (verb == "file-write") {
        const std::string relative = requireOption(args, "path");
        const fs::path path = resolveInside(repo, relative);
        std::string content;
        if (args.has("from-file")) {
            content = readTextFile(u8path(args.get("from-file")));
        } else if (args.has("content")) {
            content = args.get("content");
        } else {
            fail("file-write requires --content or --from-file", 2);
        }
        if (args.flag("append") && fs::is_regular_file(path))
            content = readTextFile(path) + content;
        writeTextFile(path, content);
        outData = {{"path", toU8(fs::relative(path, repo))},
                   {"size", static_cast<long long>(content.size())}};
        return 0;
    }

    if (verb == "file-delete") {
        const std::string relative = requireOption(args, "path");
        const fs::path path = resolveInside(repo, relative);
        std::error_code ec;
        bool removed = false;
        if (fs::is_directory(path)) {
            if (!args.flag("recursive"))
                fail("path is a directory; pass --recursive");
            removed = fs::remove_all(path, ec) > 0;
        } else {
            removed = fs::remove(path, ec);
        }
        if (ec) fail("cannot delete path: " + ec.message());
        outData = {{"path", relative}, {"removed", removed}};
        return 0;
    }

    if (verb == "file-move") {
        const std::string from = requireOption(args, "from");
        const std::string to = requireOption(args, "to");
        const fs::path source = resolveInside(repo, from);
        const fs::path target = resolveInside(repo, to);
        if (!fs::exists(source)) fail("source not found: " + from);
        if (fs::exists(target)) fail("target already exists: " + to);
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        fs::rename(source, target, ec);
        if (ec) fail("cannot move path: " + ec.message());
        outData = {{"from", from}, {"to", to}};
        return 0;
    }

    if (verb == "json-get") {
        const std::string file = requireOption(args, "file");
        const json document = loadJson(resolveInside(repo, file));
        const std::string pointer = args.get("pointer", "");
        outData = {{"file", file}, {"pointer", pointer},
                   {"value", jsonPointerGet(document, pointer)}};
        return 0;
    }

    if (verb == "json-set") {
        const std::string file = requireOption(args, "file");
        const std::string pointer = requireOption(args, "pointer");
        const fs::path path = resolveInside(repo, file);
        json document = loadJson(path);
        if (args.flag("delete")) {
            outData = jsonPointerSet(document, pointer, json(), true);
        } else if (args.has("value")) {
            json value;
            try {
                value = json::parse(args.get("value"));
            } catch (const json::exception&) {
                value = args.get("value");
            }
            outData = jsonPointerSet(document, pointer, value, false);
        } else if (args.has("value-string")) {
            outData = jsonPointerSet(document, pointer, args.get("value-string"), false);
        } else {
            fail("json-set requires --value, --value-string or --delete", 2);
        }
        writeJson(path, document);
        outData["file"] = file;
        outData["pointer"] = pointer;
        return 0;
    }

    if (verb == "pointer-add") {
        const std::string relative = requireOption(args, "path");
        const std::string sha = requireOption(args, "sha256");
        const std::string resolver = requireOption(args, "resolver");
        if (sha.size() != 64
            || !std::all_of(sha.begin(), sha.end(), [](unsigned char c) {
                return std::isxdigit(c) != 0;
            })) {
            fail("--sha256 must be 64 hex characters");
        }
        const std::string branch = args.get("branch", "");
        std::string branchName = branch;
        if (branchName.empty()) {
            const json branches = branchesOf(workspace);
            if (branches.empty()) fail("workspace has no branches; pass --branch");
            branchName = branches[0].value("name", "");
        }
        if (findBranch(workspace, branchName) < 0)
            fail("branch not found: " + branchName);

        json metadata = json::object();
        if (args.has("metadata-json")) {
            try {
                metadata = json::parse(args.get("metadata-json"));
            } catch (const json::exception& e) {
                fail(std::string("invalid --metadata-json: ") + e.what());
            }
            if (!metadata.is_object()) fail("--metadata-json must be an object");
        }
        if (args.has("url")) metadata["url"] = args.get("url");

        const fs::path pointerPath = repo / "branch_config" / (sha + ".pointer");
        json pointer;
        if (fs::is_regular_file(pointerPath)) {
            try { pointer = loadJson(pointerPath); } catch (const CliFailure&) { pointer = json::object(); }
        }
        if (!pointer.is_object()) pointer = json::object();
        pointer["sha256"] = sha;
        pointer["resolver"] = resolver;
        pointer["metadata"] = metadata;
        std::error_code ec;
        fs::create_directories(pointerPath.parent_path(), ec);
        writeJson(pointerPath, pointer);

        json config = loadBranchConfigOrDefault(repo, branchName);
        config["file_manifest"][relative] = sha;
        config["pointer_files"][sha] = {{"resolver", resolver}, {"metadata", metadata}};
        saveBranchConfig(repo, branchName, config);

        outData = {{"branch", branchName},
                   {"path", relative},
                   {"sha256", sha},
                   {"resolver", resolver},
                   {"pointer_file", toU8(fs::relative(pointerPath, repo))},
                   {"metadata", metadata}};
        return 0;
    }

    if (verb == "pointer-remove") {
        const std::string relative = requireOption(args, "path");
        const std::string branch = args.get("branch", "");
        std::string branchName = branch;
        if (branchName.empty()) {
            const json branches = branchesOf(workspace);
            if (branches.empty()) fail("workspace has no branches; pass --branch");
            branchName = branches[0].value("name", "");
        }
        json config = loadBranchConfigOrDefault(repo, branchName);
        if (!config["file_manifest"].contains(relative))
            fail("path is not a registered pointer: " + relative);
        const std::string sha = config["file_manifest"][relative].get<std::string>();
        config["file_manifest"].erase(relative);
        bool stillUsed = false;
        for (const auto& [rel, value] : config["file_manifest"].items()) {
            if (value.is_string() && value.get<std::string>() == sha) stillUsed = true;
        }
        if (!stillUsed) config["pointer_files"].erase(sha);
        saveBranchConfig(repo, branchName, config);
        if (!stillUsed) {
            std::error_code ec;
            fs::remove(repo / "branch_config" / (sha + ".pointer"), ec);
        }
        outData = {{"branch", branchName}, {"path", relative}, {"sha256", sha},
                   {"pointer_file_removed", !stillUsed}};
        return 0;
    }

    fail("unknown exec subcommand: " + verb, 2);
}

int emitResult(bool ok, const std::string& command, const fs::path& repo,
               const json& data, const std::string& error, int exitCode, bool textMode)
{
    if (textMode) {
        if (!ok) {
            std::cerr << "ERROR: " << error << std::endl;
            return exitCode;
        }
        std::cout << "OK: " << command << std::endl;
        std::cout << data.dump(2) << std::endl;
        return exitCode;
    }
    json out;
    out["ok"] = ok;
    out["command"] = command;
    if (!repo.empty()) out["repo"] = toU8(repo);
    if (ok) out["data"] = data;
    else out["error"] = error;
    std::cout << out.dump() << std::endl;
    return exitCode;
}

#ifdef _WIN32
std::vector<std::string> wideArgsToUtf8()
{
    std::vector<std::string> args;
    int count = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (wide == nullptr) return args;
    for (int i = 0; i < count; ++i) {
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) { args.emplace_back(); continue; }
        std::string value(static_cast<size_t>(bytes - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, value.data(), bytes, nullptr, nullptr);
        args.push_back(value);
    }
    LocalFree(wide);
    return args;
}
#endif

void setupConsole()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif
}

} // namespace

bool is_cli_invocation(int argc, char* argv[])
{
    if (argc < 2) return false;
    const std::string first = argv[1] == nullptr ? "" : argv[1];
    return first == "info" || first == "exec" || first == "help";
}

int run(int argc, char* argv[])
{
    setupConsole();
    {   // CLI: keep stdout clean (JSON only); library logs go to stderr.
        auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        spdlog::set_default_logger(std::make_shared<spdlog::logger>("editor-cli", sink));
    }
#ifdef _WIN32
    std::vector<std::string> tokens = wideArgsToUtf8();
    if (tokens.size() > 1) tokens.erase(tokens.begin());
#else
    std::vector<std::string> tokens;
    for (int i = 1; i < argc; ++i) tokens.emplace_back(argv[i]);
#endif
    if (tokens.empty()) {
        printUsage();
        return 2;
    }

    const ParsedArgs args = parseArgs(tokens);
    const bool textMode = args.flag("text");
    const std::string commandName = commandJsonName(args);

    if (args.command == "help") {
        printUsage();
        return 0;
    }
    if (args.command == "info") {
        try {
            fs::path repo = resolveRepo(args);
            json data;
            const int code = runInfo(args, data);
            return emitResult(code == 0, commandName, repo, data, "", code, textMode);
        } catch (const CliFailure& failure) {
            return emitResult(false, commandName, fs::path(), json(),
                              failure.message, failure.code, textMode);
        } catch (const std::exception& error) {
            return emitResult(false, commandName, fs::path(), json(),
                              error.what(), 1, textMode);
        }
    }
    if (args.command == "exec") {
        try {
            fs::path repo = resolveRepo(args);
            json data;
            const int code = runExec(args, data);
            return emitResult(code == 0, commandName, repo, data, "", code, textMode);
        } catch (const CliFailure& failure) {
            return emitResult(false, commandName, fs::path(), json(),
                              failure.message, failure.code, textMode);
        } catch (const std::exception& error) {
            return emitResult(false, commandName, fs::path(), json(),
                              error.what(), 1, textMode);
        }
    }

    printUsage();
    return 2;
}

} // namespace editor_cli
