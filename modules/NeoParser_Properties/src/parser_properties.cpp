#include <IConfigParser.h>
#include <plugin_log_sink.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace {

static std::string read_file_content(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {};
    }
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

static std::vector<std::string> split_lines(const std::string& content)
{
    std::vector<std::string> lines;
    if (content.empty()) {
        return lines;
    }
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

static bool is_blank_or_comment(const std::string& line)
{
    auto start = line.find_first_not_of(" \t\r");
    if (start == std::string::npos) {
        return true;
    }
    char first = line[start];
    return first == '#' || first == '!';
}

// 解析 key=value / key:value / key value (空格分隔), 键 trim, 值去行内注释
static bool parse_property_line(const std::string& line,
                                std::string& out_key,
                                std::string& out_value)
{
    if (is_blank_or_comment(line)) {
        return false;
    }

    size_t eq_pos = line.find('=');
    size_t col_pos = line.find(':');

    size_t sep_pos = std::string::npos;
    if (eq_pos != std::string::npos && col_pos != std::string::npos) {
        sep_pos = std::min(eq_pos, col_pos);
    } else if (eq_pos != std::string::npos) {
        sep_pos = eq_pos;
    } else if (col_pos != std::string::npos) {
        sep_pos = col_pos;
    } else {
        // 空格分隔: 第一个空白后的首个非空白之前为键
        auto first_space = line.find_first_of(" \t");
        if (first_space == std::string::npos) {
            return false;
        }
        std::string key = line.substr(0, first_space);
        auto key_start = key.find_first_not_of(" \t");
        if (key_start == std::string::npos) {
            return false;
        }
        out_key = key.substr(key_start);
        auto val_start = line.find_first_not_of(" \t", first_space);
        out_value = (val_start == std::string::npos)
            ? std::string() : line.substr(val_start);
        return true;
    }

    std::string key = line.substr(0, sep_pos);
    std::string value = line.substr(sep_pos + 1);

    auto key_start = key.find_first_not_of(" \t");
    if (key_start == std::string::npos) {
        return false;
    }
    auto key_end = key.find_last_not_of(" \t");
    key = key.substr(key_start, key_end - key_start + 1);

    auto val_start = value.find_first_not_of(" \t");
    if (val_start == std::string::npos) {
        out_key = key;
        out_value.clear();
        return true;
    }
    value = value.substr(val_start);

    // 行内注释: 未转义且不在引号内的 '#' (前置空白)
    size_t comment_pos = std::string::npos;
    bool in_quotes = false;
    bool escaped = false;
    for (size_t i = 0; i < value.size(); ++i) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (value[i] == '\\') {
            escaped = true;
            continue;
        }
        if (value[i] == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (!in_quotes && value[i] == '#') {
            if (i == 0 || value[i - 1] == ' ' || value[i - 1] == '\t') {
                comment_pos = i;
                break;
            }
        }
    }
    if (comment_pos != std::string::npos) {
        value = value.substr(0, comment_pos);
    }

    auto val_end = value.find_last_not_of(" \t");
    if (val_end != std::string::npos) {
        value = value.substr(0, val_end + 1);
    } else {
        value.clear();
    }

    out_key = key;
    out_value = value;
    return true;
}

class PropertiesConfigParser : public NeoCore::IConfigParser {
public:
    NeoCore::ParserCapability capability() const override
    {
        NeoCore::ParserCapability cap;
        cap.name = "Properties";
        cap.extensions = {".properties"};
        cap.supported_modes = {
            NeoCore::TrackingMode::FullSync,
            NeoCore::TrackingMode::ConfigMerge,
            NeoCore::TrackingMode::NoSync
        };
        cap.supports_line_tracking = false;
        cap.priority = 80;
        return cap;
    }

    bool can_handle(const std::string& filepath) const override
    {
        auto pos = filepath.rfind('.');
        if (pos == std::string::npos) {
            return false;
        }
        std::string ext = filepath.substr(pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".properties";
    }

    std::vector<NeoCore::ConfigEntry> parse_entries(
        const std::string& filepath) override
    {
        std::vector<NeoCore::ConfigEntry> entries;

        std::string content = read_file_content(filepath);
        if (content.empty()) {
            return entries;
        }

        auto lines = split_lines(content);
        for (size_t i = 0; i < lines.size(); ++i) {
            std::string key;
            std::string value;
            if (parse_property_line(lines[i], key, value)) {
                NeoCore::ConfigEntry entry;
                entry.key_path = key;
                entry.remote_value = value;
                entry.local_value.clear();
                entry.is_tracked = false;
                entries.push_back(entry);
            }
        }

        return entries;
    }

    std::string merge_entries(
        const std::string& /*filepath*/,
        const std::vector<std::string>& tracked_keys,
        const std::string& remote_content,
        const std::string& local_content) override
    {
        if (local_content.empty() && remote_content.empty()) {
            return {};
        }
        if (local_content.empty()) {
            return remote_content;
        }
        if (remote_content.empty()) {
            return local_content;
        }

        std::unordered_set<std::string> tracked(
            tracked_keys.begin(), tracked_keys.end());

        auto remote_lines = split_lines(remote_content);
        auto local_lines = split_lines(local_content);

        std::unordered_map<std::string, size_t> remote_key_line;
        for (size_t i = 0; i < remote_lines.size(); ++i) {
            std::string key;
            std::string value;
            if (parse_property_line(remote_lines[i], key, value)) {
                remote_key_line[key] = i;
            }
        }

        std::ostringstream result;

        for (size_t i = 0; i < local_lines.size(); ++i) {
            std::string key;
            std::string value;

            if (parse_property_line(local_lines[i], key, value) && tracked.count(key)) {
                auto it = remote_key_line.find(key);
                if (it != remote_key_line.end()) {
                    result << remote_lines[it->second];
                } else {
                    result << local_lines[i];
                }
            } else {
                result << local_lines[i];
            }

            if (i + 1 < local_lines.size()) {
                result << "\n";
            }
        }

        if (!local_content.empty() && local_content.back() == '\n') {
            result << "\n";
        }

        return result.str();
    }

    std::vector<std::string> list_keys(
        const std::string& filepath) override
    {
        std::vector<std::string> keys;
        auto entries = parse_entries(filepath);
        keys.reserve(entries.size());
        for (auto& e : entries) {
            keys.push_back(std::move(e.key_path));
        }
        return keys;
    }
};

} // anonymous namespace

extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser()
{
    return new PropertiesConfigParser();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_Properties")
