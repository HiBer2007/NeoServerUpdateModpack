#pragma once

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace NeoCLI {

class CliOutput {
public:
    static void info(const std::string& msg);
    static void success(const std::string& msg);
    static void warning(const std::string& msg);
    static void error(const std::string& msg);
    static void progress(int percent, const std::string& msg);
    static void table(const std::vector<std::string>& headers,
        const std::vector<std::vector<std::string>>& rows);
    static void separator(char c = '-', int width = 60);
    static void title(const std::string& title);

    static void jsonBlock(const nlohmann::json& value);

    static void setQuiet(bool quiet);
    static void setVerbose(bool verbose);
    static bool isQuiet();

    static void setJsonMode(bool on);
    static bool isJsonMode();

private:
    CliOutput() = delete;

    static bool quiet_;
    static bool verbose_;
    static bool jsonMode_;
    static bool useColors();
};

} // namespace NeoCLI
