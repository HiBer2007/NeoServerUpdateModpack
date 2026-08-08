#include <IConfigParser.h>
#include <plugin_log_sink.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

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

static std::vector<std::string> split_path(const std::string& path)
{
    std::vector<std::string> result;
    if (path.empty()) {
        return result;
    }
    size_t start = 0;
    size_t end = 0;
    while ((end = path.find('.', start)) != std::string::npos) {
        result.push_back(path.substr(start, end - start));
        start = end + 1;
    }
    result.push_back(path.substr(start));
    return result;
}

static std::string yaml_node_to_string(const YAML::Node& node)
{
    if (!node.IsDefined() || node.IsNull()) {
        return {};
    }

    switch (node.Type()) {
    case YAML::NodeType::Scalar: {
        std::string val = node.as<std::string>();
        return val;
    }
    case YAML::NodeType::Sequence:
    case YAML::NodeType::Map: {
        YAML::Emitter em;
        em.SetStringFormat(YAML::EMITTER_MANIP::Flow);
        em << node;
        return std::string(em.c_str());
    }
    default:
        return {};
    }
}

static void flatten_yaml(const YAML::Node& node,
                         const std::string& prefix,
                         std::vector<NeoCore::ConfigEntry>& entries)
{
    if (!node.IsDefined() || node.IsNull()) {
        return;
    }

    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            std::string key = it->first.as<std::string>();
            std::string path = prefix.empty() ? key : prefix + "." + key;

            const YAML::Node& val = it->second;
            if (val.IsDefined() && !val.IsNull() && val.IsMap()) {
                flatten_yaml(val, path, entries);
            } else if (val.IsDefined() && !val.IsNull() && val.IsSequence()) {
                NeoCore::ConfigEntry entry;
                entry.key_path = path;
                entry.remote_value = yaml_node_to_string(val);
                entry.local_value.clear();
                entry.is_tracked = false;
                entries.push_back(entry);
            } else {
                NeoCore::ConfigEntry entry;
                entry.key_path = path;
                entry.remote_value = yaml_node_to_string(val);
                entry.local_value.clear();
                entry.is_tracked = false;
                entries.push_back(entry);
            }
        }
    } else if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); ++i) {
            std::string path = prefix.empty()
                                   ? "[" + std::to_string(i) + "]"
                                   : prefix + "[" + std::to_string(i) + "]";
            if (node[i].IsMap()) {
                flatten_yaml(node[i], path, entries);
            } else {
                NeoCore::ConfigEntry entry;
                entry.key_path = path;
                entry.remote_value = yaml_node_to_string(node[i]);
                entry.local_value.clear();
                entry.is_tracked = false;
                entries.push_back(entry);
            }
        }
    }
}

static YAML::Node navigate_yaml(YAML::Node root,
                                const std::vector<std::string>& segments)
{
    YAML::Node current = root;
    for (const auto& seg : segments) {
        if (!current.IsMap()) {
            return YAML::Node();
        }
        current = current[seg];
        if (!current.IsDefined()) {
            return YAML::Node();
        }
    }
    return current;
}

static void set_yaml_at_path(YAML::Node root,
                             const std::vector<std::string>& segments,
                             const YAML::Node& value)
{
    if (segments.empty()) {
        return;
    }

    YAML::Node current = root;
    for (size_t i = 0; i < segments.size() - 1; ++i) {
        if (!current.IsMap()) {
            return;
        }
        current = current[segments[i]];
        if (!current.IsDefined()) {
            return;
        }
    }

    if (current.IsMap()) {
        current[segments.back()] = value;
    }
}

class YamlConfigParser : public NeoCore::IConfigParser {
public:
    NeoCore::ParserCapability capability() const override
    {
        NeoCore::ParserCapability cap;
        cap.name = "YAML";
        cap.extensions = {".yml", ".yaml"};
        cap.supported_modes = {
            NeoCore::TrackingMode::FullSync,
            NeoCore::TrackingMode::ConfigMerge,
            NeoCore::TrackingMode::NoSync
        };
        cap.supports_line_tracking = false;
        cap.priority = 90;
        return cap;
    }

    bool can_handle(const std::string& filepath) const override
    {
        auto pos = filepath.rfind('.');
        if (pos == std::string::npos) {
            return false;
        }
        std::string ext = filepath.substr(pos);
        return ext == ".yml" || ext == ".yaml";
    }

    std::vector<NeoCore::ConfigEntry> parse_entries(
        const std::string& filepath) override
    {
        std::vector<NeoCore::ConfigEntry> entries;

        std::string content = read_file_content(filepath);
        if (content.empty()) {
            return entries;
        }

        YAML::Node root;
        try {
            root = YAML::Load(content);
        } catch (const std::exception&) {
            return entries;
        }

        if (!root.IsDefined() || root.IsNull()) {
            return entries;
        }

        flatten_yaml(root, "", entries);
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

        YAML::Node remote;
        YAML::Node local;
        try {
            remote = YAML::Load(remote_content);
        } catch (const std::exception&) {
            return local_content;
        }
        try {
            local = YAML::Load(local_content);
        } catch (const std::exception&) {
            return remote_content;
        }

        if (!remote.IsDefined() || !local.IsDefined()) {
            return local_content;
        }

        YAML::Node result = local;

        for (const auto& key : tracked_keys) {
            if (key.empty()) {
                continue;
            }

            auto segs = split_path(key);
            if (segs.empty()) {
                continue;
            }

            YAML::Node remote_val = navigate_yaml(remote, segs);
            if (!remote_val.IsDefined()) {
                continue;
            }

            set_yaml_at_path(result, segs, remote_val);
        }

        YAML::Emitter emitter;
        emitter << result;
        return std::string(emitter.c_str());
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
    return new YamlConfigParser();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_YAML")

