#pragma once

#include <string>
#include <vector>
#include <map>

namespace NeoCLI {

enum class CliCategory {
    None,
    Help,
    Version,
    Info,
    Flow,
    Exec
};

struct CliCommand {
    CliCategory category = CliCategory::None;
    std::string verb;
    std::map<std::string, std::string> options;
    std::vector<std::string> positional;
    std::vector<std::string> prefill;
    bool json = false;
    bool verbose = false;
    bool silent = false;
    bool help = false;
    std::string error;

    bool has(const std::string& key) const
    {
        return options.count(key) > 0;
    }

    std::string get(const std::string& key) const
    {
        auto it = options.find(key);
        return it == options.end() ? std::string() : it->second;
    }
};

class ArgParser {
public:
    ArgParser();

    CliCommand parse(int argc, char* argv[]);

    void printHelp() const;
    void printHelp(CliCategory category) const;
    void printVersion() const;

    static std::string version();
    static CliCategory categoryFromName(const std::string& name);
    static std::string categoryName(CliCategory category);
    static bool isHelpToken(const std::string& token);
    static bool isVersionToken(const std::string& token);
    static bool verbExists(CliCategory category, const std::string& verb);
};

} // namespace NeoCLI
