#pragma once

#include <string>
#include <vector>
#include <memory>

namespace NeoCore {

enum class TrackingMode {
    FullSync,
    ConfigMerge,
    LineByLine,
    NoSync
};

struct ParserCapability {
    std::string name;
    std::vector<std::string> extensions;
    std::vector<TrackingMode> supported_modes;
    bool supports_line_tracking;
    int priority;
};

struct ConfigEntry {
    std::string key_path;
    std::string remote_value;
    std::string local_value;
    bool is_tracked;
};

struct LineEntry {
    int line_number;
    std::string remote_text;
    std::string local_text;
    bool is_tracked;
};

class IConfigParser {
public:
    virtual ~IConfigParser() = default;

    virtual ParserCapability capability() const = 0;

    virtual bool can_handle(const std::string& filepath) const = 0;

    virtual std::vector<ConfigEntry> parse_entries(
        const std::string& filepath) = 0;

    virtual std::string merge_entries(
        const std::string& filepath,
        const std::vector<std::string>& tracked_keys,
        const std::string& remote_content,
        const std::string& local_content) = 0;

    virtual std::vector<LineEntry> parse_lines(
        const std::string& filepath) { return {}; }

    virtual std::string merge_lines(
        const std::string& filepath,
        const std::vector<int>& tracked_lines,
        const std::string& remote_content,
        const std::string& local_content) { return ""; }

    virtual std::vector<std::string> list_keys(
        const std::string& filepath) = 0;
};

using CreateParserFunc = IConfigParser* (*)();

} // namespace NeoCore
