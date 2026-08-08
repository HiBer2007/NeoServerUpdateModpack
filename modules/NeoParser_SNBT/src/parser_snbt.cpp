#include <IConfigParser.h>
#include <plugin_log_sink.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <nbtcpp/nbt_tag_type.h>
#include <nbtcpp/snbt/snbt_parser.h>
#include <nbtcpp/snbt/snbt_maker.h>
#include <nbtcpp/tags/nbt_compound.h>
#include <nbtcpp/tags/nbt_string.h>
#include <nbtcpp/tags/nbt_byte.h>
#include <nbtcpp/tags/nbt_short.h>
#include <nbtcpp/tags/nbt_int.h>
#include <nbtcpp/tags/nbt_long.h>
#include <nbtcpp/tags/nbt_float.h>
#include <nbtcpp/tags/nbt_double.h>
#include <nbtcpp/tags/nbt_list.h>
#include <nbtcpp/tags/nbt_byte_array.h>
#include <nbtcpp/tags/nbt_int_array.h>
#include <nbtcpp/tags/nbt_long_array.h>
#include <nbtcpp/tags/nbt_container_tag.h>

namespace SnbtPreserving {
std::string merge(const std::string& localContent,
                  const std::string& remoteContent,
                  const std::vector<std::string>& trackedKeys);
}

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

static std::string tag_value_to_string(const nbtcpp::NbtTag& tag)
{
    switch (tag.tag_type()) {
    case nbtcpp::NbtTagType::String: {
        auto* s = dynamic_cast<const nbtcpp::NbtString*>(&tag);
        return s ? "\"" + s->value() + "\"" : "";
    }
    case nbtcpp::NbtTagType::Byte: {
        auto* b = dynamic_cast<const nbtcpp::NbtByte*>(&tag);
        return b ? std::to_string(b->value()) + "b" : "0b";
    }
    case nbtcpp::NbtTagType::Short: {
        auto* s = dynamic_cast<const nbtcpp::NbtShort*>(&tag);
        return s ? std::to_string(s->value()) + "s" : "0s";
    }
    case nbtcpp::NbtTagType::Int: {
        auto* i = dynamic_cast<const nbtcpp::NbtInt*>(&tag);
        return i ? std::to_string(i->value()) : "0";
    }
    case nbtcpp::NbtTagType::Long: {
        auto* l = dynamic_cast<const nbtcpp::NbtLong*>(&tag);
        return l ? std::to_string(l->value()) + "L" : "0L";
    }
    case nbtcpp::NbtTagType::Float: {
        auto* f = dynamic_cast<const nbtcpp::NbtFloat*>(&tag);
        return f ? std::to_string(f->value()) + "f" : "0.0f";
    }
    case nbtcpp::NbtTagType::Double: {
        auto* d = dynamic_cast<const nbtcpp::NbtDouble*>(&tag);
        return d ? std::to_string(d->value()) + "d" : "0.0d";
    }
    default: {
        nbtcpp::snbt::Options opts = nbtcpp::snbt::Options::default_options();
        return nbtcpp::snbt::to_snbt(tag, opts);
    }
    }
}

static void flatten_nbt(const nbtcpp::NbtCompound& comp,
                        const std::string& prefix,
                        std::vector<NeoCore::ConfigEntry>& entries)
{
    for (size_t i = 0; i < comp.size(); ++i) {
        auto* child = comp.at(i);
        if (!child) {
            continue;
        }

        std::string path = prefix.empty()
                               ? child->name()
                               : prefix + "." + child->name();

        if (child->tag_type() == nbtcpp::NbtTagType::Compound) {
            auto* sub = dynamic_cast<const nbtcpp::NbtCompound*>(child);
            if (sub) {
                flatten_nbt(*sub, path, entries);
            }
        } else {
            NeoCore::ConfigEntry entry;
            entry.key_path = path;
            entry.remote_value = tag_value_to_string(*child);
            entry.local_value.clear();
            entry.is_tracked = false;
            entries.push_back(entry);
        }
    }
}

class SnbtConfigParser : public NeoCore::IConfigParser {
public:
    NeoCore::ParserCapability capability() const override
    {
        NeoCore::ParserCapability cap;
        cap.name = "SNBT";
        cap.extensions = {".snbt"};
        cap.supported_modes = {
            NeoCore::TrackingMode::FullSync,
            NeoCore::TrackingMode::ConfigMerge,
            NeoCore::TrackingMode::NoSync
        };
        cap.supports_line_tracking = false;
        cap.priority = 70;
        return cap;
    }

    bool can_handle(const std::string& filepath) const override
    {
        auto pos = filepath.rfind('.');
        if (pos == std::string::npos) {
            return false;
        }
        return filepath.substr(pos) == ".snbt";
    }

    std::vector<NeoCore::ConfigEntry> parse_entries(
        const std::string& filepath) override
    {
        std::vector<NeoCore::ConfigEntry> entries;

        std::string content = read_file_content(filepath);
        if (content.empty()) {
            return entries;
        }

        nbtcpp::NbtTagPtr nbt;
        try {
            nbt = nbtcpp::snbt::parse(content);
        } catch (const std::exception&) {
            return entries;
        }

        if (!nbt) {
            return entries;
        }

        if (nbt->tag_type() != nbtcpp::NbtTagType::Compound) {
            return entries;
        }

        auto* comp = dynamic_cast<nbtcpp::NbtCompound*>(nbt.get());
        if (!comp) {
            return entries;
        }

        flatten_nbt(*comp, "", entries);
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

        return SnbtPreserving::merge(local_content, remote_content, tracked_keys);
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
    return new SnbtConfigParser();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_SNBT")

