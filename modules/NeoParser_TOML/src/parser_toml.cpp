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

#include <toml++/toml.hpp>

namespace {

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

static std::string node_to_string(const toml::node& node)
{
    std::ostringstream oss;

    if (auto* v = node.as_string()) {
        oss << v->get();
    } else if (auto* v = node.as_integer()) {
        oss << v->get();
    } else if (auto* v = node.as_floating_point()) {
        oss << v->get();
    } else if (auto* v = node.as_boolean()) {
        oss << (v->get() ? "true" : "false");
    } else {
        oss << "<complex>";
    }

    return oss.str();
}

static void flatten_toml(const toml::table& tbl,
                         const std::string& prefix,
                         std::vector<NeoCore::ConfigEntry>& entries)
{
    for (auto&& [key, node] : tbl) {
        std::string key_str(key);
        std::string path = prefix.empty()
                               ? key_str
                               : prefix + "." + key_str;

        if (node.is_table()) {
            flatten_toml(*node.as_table(), path, entries);
        } else if (node.is_array_of_tables()) {
            auto* arr = node.as_array();
            if (arr) {
                for (size_t i = 0; i < arr->size(); ++i) {
                    auto& elem = (*arr)[i];
                    if (elem.is_table()) {
                        std::string arr_path = path + "[" + std::to_string(i) + "]";
                        flatten_toml(*elem.as_table(), arr_path, entries);
                    }
                }
            }
        } else {
            NeoCore::ConfigEntry entry;
            entry.key_path = path;
            entry.remote_value = node_to_string(node);
            entry.local_value.clear();
            entry.is_tracked = false;
            entries.push_back(entry);
        }
    }
}

static toml::node* navigate_toml(toml::table& root,
                                 const std::vector<std::string>& segments)
{
    toml::node* current = &root;
    for (const auto& seg : segments) {
        auto* tbl = current->as_table();
        if (!tbl) {
            return nullptr;
        }
        auto it = tbl->find(seg);
        if (it == tbl->end()) {
            return nullptr;
        }
        current = &it->second;
    }
    return current;
}

static const toml::node* navigate_toml(const toml::table& root,
                                       const std::vector<std::string>& segments)
{
    const toml::node* current = &root;
    for (const auto& seg : segments) {
        auto* tbl = current->as_table();
        if (!tbl) {
            return nullptr;
        }
        auto it = tbl->find(seg);
        if (it == tbl->end()) {
            return nullptr;
        }
        current = &it->second;
    }
    return current;
}

class TomlConfigParser : public NeoCore::IConfigParser {
public:
    NeoCore::ParserCapability capability() const override
    {
        NeoCore::ParserCapability cap;
        cap.name = "TOML";
        cap.extensions = {".toml"};
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
        return filepath.substr(pos) == ".toml";
    }

    std::vector<NeoCore::ConfigEntry> parse_entries(
        const std::string& filepath) override
    {
        std::vector<NeoCore::ConfigEntry> entries;

        try {
            auto root = toml::parse_file(filepath);
            flatten_toml(root, "", entries);
        } catch (const toml::parse_error&) {
            return entries;
        } catch (const std::exception&) {
            return entries;
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

        toml::table remote;
        toml::table local;
        try {
            remote = toml::parse(remote_content);
        } catch (const toml::parse_error&) {
            return local_content;
        } catch (const std::exception&) {
            return local_content;
        }
        try {
            local = toml::parse(local_content);
        } catch (const toml::parse_error&) {
            return remote_content;
        } catch (const std::exception&) {
            return remote_content;
        }

        toml::table result = local;

        for (const auto& key : tracked_keys) {
            if (key.empty()) {
                continue;
            }

            auto segs = split_path(key);
            if (segs.empty()) {
                continue;
            }

            const auto* src = navigate_toml(remote, segs);
            if (!src) {
                continue;
            }

            const std::string& last = segs.back();

            toml::table* parent = &result;
            for (size_t i = 0; i + 1 < segs.size(); ++i) {
                auto it = parent->find(segs[i]);
                if (it == parent->end() || !it->second.is_table()) {
                    parent = nullptr;
                    break;
                }
                parent = it->second.as_table();
            }
            if (!parent) {
                continue;
            }

            if (src->is_string() && src->as_string()) {
                parent->insert_or_assign(last,
                    std::string(src->as_string()->get()));
            } else if (src->is_integer() && src->as_integer()) {
                parent->insert_or_assign(last,
                    src->as_integer()->get());
            } else if (src->is_floating_point() && src->as_floating_point()) {
                parent->insert_or_assign(last,
                    src->as_floating_point()->get());
            } else if (src->is_boolean() && src->as_boolean()) {
                parent->insert_or_assign(last,
                    src->as_boolean()->get());
            } else if (src->is_table() && src->as_table()) {
                parent->insert_or_assign(last,
                    *src->as_table());
            } else if (src->is_array() && src->as_array()) {
                parent->insert_or_assign(last,
                    *src->as_array());
            }
        }

        std::ostringstream oss;
        oss << result;
        return oss.str();
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
    return new TomlConfigParser();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_TOML")

