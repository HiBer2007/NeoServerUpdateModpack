/**
 * @file snbt_maker.cpp
 * @brief SNBT serializer implementation.
 */

#include "nbtcpp/snbt/snbt_maker.h"
#include "nbtcpp/snbt/snbt_parser.h"   // for Constants
#include "nbtcpp/tags/nbt_byte.h"
#include "nbtcpp/tags/nbt_short.h"
#include "nbtcpp/tags/nbt_int.h"
#include "nbtcpp/tags/nbt_long.h"
#include "nbtcpp/tags/nbt_float.h"
#include "nbtcpp/tags/nbt_double.h"
#include "nbtcpp/tags/nbt_string.h"
#include "nbtcpp/tags/nbt_byte_array.h"
#include "nbtcpp/tags/nbt_int_array.h"
#include "nbtcpp/tags/nbt_long_array.h"
#include "nbtcpp/tags/nbt_list.h"
#include "nbtcpp/tags/nbt_compound.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace nbtcpp {
namespace snbt {

using Constants = snbt::Constants;

// ─── Presets ─────────────────────────────────────────────────────────────────

Options Options::default_options() {
    Options opts;
    opts.minified = true;
    opts.is_json_like = false;
    opts.number_suffixes = true;
    opts.array_prefixes = true;
    opts.bytes_as_bools = false;
    opts.should_quote_keys = [](const std::string& s) {
        return s.empty() || s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz0123456789._+-") != std::string::npos;
    };
    opts.should_quote_strings = [](const std::string&) { return true; };
    opts.newline_handler = []() { return "\\n"; };
    return opts;
}

Options Options::default_expanded() {
    auto opts = default_options();
    opts.minified = false;
    return opts;
}

Options Options::json_like() {
    Options opts;
    opts.minified = true;
    opts.is_json_like = true;
    opts.number_suffixes = false;
    opts.array_prefixes = false;
    opts.bytes_as_bools = true;
    opts.should_quote_keys = [](const std::string&) { return true; };
    opts.should_quote_strings = [](const std::string& s) { return s != "null"; };
    opts.newline_handler = []() { return "\\n"; };
    return opts;
}

Options Options::json_like_expanded() {
    auto opts = json_like();
    opts.minified = false;
    return opts;
}

Options Options::preview() {
    Options opts;
    opts.minified = true;
    opts.number_suffixes = false;
    opts.array_prefixes = false;
    opts.should_quote_keys = [](const std::string&) { return false; };
    opts.should_quote_strings = [](const std::string&) { return false; };
    opts.newline_handler = []() { return "\\n"; };
    return opts;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::string optional_suffix(const Options& opts, char suffix) {
    return opts.number_suffixes ? std::string(1, suffix) : "";
}

static std::string quote_and_escape(const std::string& input, const Options& opts) {
    std::string result;
    result += Constants::kPrimaryQuote;
    for (char c : input) {
        if (c == '\\') {
            result += "\\\\";
        } else if (c == Constants::kPrimaryQuote) {
            result += "\\\"";
        } else if (c == '\n') {
            result += opts.newline_handler();
        } else if (c == '\t') {
            result += "\\t";
        } else if (c == '\r') {
            result += "\\r";
        } else {
            result += c;
        }
    }
    result += Constants::kPrimaryQuote;
    return result;
}

static std::string quote_if_requested(const std::string& str,
                                       const std::function<bool(const std::string&)>& should_quote,
                                       const Options& opts) {
    if (should_quote(str)) return quote_and_escape(str, opts);
    // Replace newlines
    std::string result = str;
    for (size_t i = 0; i < result.size(); ++i) {
        if (result[i] == '\n') {
            result.replace(i, 1, opts.newline_handler());
            i += opts.newline_handler().size() - 1;
        }
    }
    return result;
}

static std::string get_name_before_value(const NbtTag& tag, const Options& opts) {
    if (tag.name().empty()) return {};
    std::string name_str = quote_if_requested(tag.name(), opts.should_quote_keys, opts);
    return name_str + Constants::kNameValueSep + (opts.minified ? "" : " ");
}

template<typename T>
static std::string list_to_string(const std::string& prefix,
                                   const std::function<std::string(const T&)>& func,
                                   const std::vector<T>& values,
                                   const Options& opts) {
    std::string actual_prefix = opts.array_prefixes ? prefix : "";
    std::string spacing = opts.minified ? "" : " ";
    std::string prefix_sep = (!opts.minified && !actual_prefix.empty() && !values.empty()) ? " " : "";

    std::string result;
    result += Constants::kListOpen;
    result += actual_prefix;
    result += prefix_sep;

    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            result += Constants::kValueSep;
            result += spacing;
        }
        result += func(values[i]);
    }
    result += Constants::kListClose;
    return result;
}

// ─── Tag-specific serializers ───────────────────────────────────────────────

static std::string to_snbt_byte(const NbtByte& tag, const Options& opts) {
    if (opts.bytes_as_bools) {
        if (tag.value() == 0) return "false";
        if (tag.value() == 1) return "true";
    }
    return std::to_string(static_cast<int8_t>(tag.value())) + optional_suffix(opts, Constants::kByteSuffix);
}

static std::string to_snbt_short(const NbtShort& tag, const Options& opts) {
    return std::to_string(tag.value()) + optional_suffix(opts, Constants::kShortSuffix);
}

static std::string to_snbt_int(const NbtInt& tag, const Options&) {
    return std::to_string(tag.value());
}

static std::string to_snbt_long(const NbtLong& tag, const Options& opts) {
    return std::to_string(tag.value()) + optional_suffix(opts, Constants::kLongSuffix);
}

static std::string to_snbt_float(const NbtFloat& tag, const Options& opts) {
    std::string result;
    float v = tag.value();
    if (std::isinf(v)) {
        result = (v > 0) ? "Infinity" : "-Infinity";
    } else if (std::isnan(v)) {
        result = "NaN";
    } else {
        // Use enough precision
        std::ostringstream ss;
        ss.precision(std::numeric_limits<float>::max_digits10);
        ss << v;
        result = ss.str();
    }
    return result + optional_suffix(opts, Constants::kFloatSuffix);
}

static std::string to_snbt_double(const NbtDouble& tag, const Options& opts) {
    std::string result;
    double v = tag.value();
    if (std::isinf(v)) {
        result = (v > 0) ? "Infinity" : "-Infinity";
    } else if (std::isnan(v)) {
        result = "NaN";
    } else {
        std::ostringstream ss;
        ss.precision(std::numeric_limits<double>::max_digits10);
        ss << v;
        result = ss.str();
    }
    return result + optional_suffix(opts, Constants::kDoubleSuffix);
}

static std::string to_snbt_string(const NbtString& tag, const Options& opts) {
    return quote_if_requested(tag.value(), opts.should_quote_strings, opts);
}

static std::string to_snbt_byte_array(const NbtByteArray& tag, const Options& opts) {
    return list_to_string<uint8_t>(
        std::string(1, Constants::kByteArrayPrefix) + Constants::kArrayDelim,
        [&](uint8_t v) { return std::to_string(static_cast<int8_t>(v)) + (opts.number_suffixes ? "b" : ""); },
        tag.value(), opts);
}

static std::string to_snbt_int_array(const NbtIntArray& tag, const Options& opts) {
    return list_to_string<int32_t>(
        std::string(1, Constants::kIntArrayPrefix) + Constants::kArrayDelim,
        [](int32_t v) { return std::to_string(v); },
        tag.value(), opts);
}

static std::string to_snbt_long_array(const NbtLongArray& tag, const Options& opts) {
    return list_to_string<int64_t>(
        std::string(1, Constants::kLongArrayPrefix) + Constants::kArrayDelim,
        [&](int64_t v) { return std::to_string(v) + (opts.number_suffixes ? "L" : ""); },
        tag.value(), opts);
}

// ─── Container serializers ──────────────────────────────────────────────────

static void add_indents(std::string& out, const std::string& indent_str, int level) {
    for (int i = 0; i < level; ++i) out += indent_str;
}

static void add_snbt_list(const NbtList& tag, const Options& opts,
                          std::string& out, const std::string& indent_str,
                          int level, bool include_name);

static void add_snbt_compound(const NbtCompound& tag, const Options& opts,
                              std::string& out, const std::string& indent_str,
                              int level, bool include_name);

static void add_snbt(const NbtTag& tag, const Options& opts,
                     std::string& out, const std::string& indent_str,
                     int level, bool include_name) {
    if (auto* c = dynamic_cast<const NbtCompound*>(&tag)) {
        add_snbt_compound(*c, opts, out, indent_str, level, include_name);
    } else if (auto* l = dynamic_cast<const NbtList*>(&tag)) {
        add_snbt_list(*l, opts, out, indent_str, level, include_name);
    } else {
        add_indents(out, indent_str, level);
        out += to_snbt(tag, opts, include_name);
    }
}

static bool should_compress_list_of(NbtTagType type) {
    return type == NbtTagType::Byte || type == NbtTagType::Short ||
           type == NbtTagType::Int || type == NbtTagType::Long ||
           type == NbtTagType::Float || type == NbtTagType::Double ||
           type == NbtTagType::String;
}

static void add_snbt_compound(const NbtCompound& tag, const Options& opts,
                              std::string& out, const std::string& indent_str,
                              int level, bool include_name) {
    add_indents(out, indent_str, level);
    if (include_name) out += get_name_before_value(tag, opts);
    out += Constants::kCompoundOpen;

    if (tag.size() > 0) {
        if (!opts.minified) out += '\n';
        auto children = tag.names();
        for (size_t i = 0; i < children.size(); ++i) {
            const NbtTag* child = tag.get(children[i]);
            if (!child) continue;
            add_snbt(*child, opts, out, indent_str, level + 1, true);
            if (i < children.size() - 1) {
                out += Constants::kValueSep;
            }
            if (!opts.minified) out += '\n';
        }
        add_indents(out, indent_str, level);
    }
    out += Constants::kCompoundClose;
}

static void add_snbt_list(const NbtList& tag, const Options& opts,
                          std::string& out, const std::string& indent_str,
                          int level, bool include_name) {
    add_indents(out, indent_str, level);
    if (include_name) out += get_name_before_value(tag, opts);
    bool compressed = should_compress_list_of(tag.list_type());

    if (compressed) {
        out += Constants::kListOpen;
        for (size_t i = 0; i < tag.size(); ++i) {
            if (i > 0) {
                out += Constants::kValueSep;
                if (!opts.minified) out += ' ';
            }
            out += to_snbt(*tag.at(i), opts, false);
        }
        out += Constants::kListClose;
    } else {
        out += Constants::kListOpen;
        if (tag.size() > 0) {
            if (!opts.minified) out += '\n';
            for (size_t i = 0; i < tag.size(); ++i) {
                add_snbt(*tag.at(i), opts, out, indent_str, level + 1, false);
                if (i < tag.size() - 1) out += Constants::kValueSep;
                if (!opts.minified) out += '\n';
            }
            add_indents(out, indent_str, level);
        }
        out += Constants::kListClose;
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

std::string to_snbt(const NbtTag& tag, const Options& opts, bool include_name) {
    // Dispatch to type-specific serializers
    if (auto* b = dynamic_cast<const NbtByte*>(&tag)) {
        std::string name_part = include_name ? get_name_before_value(tag, opts) : "";
        return name_part + to_snbt_byte(*b, opts);
    }
    if (auto* s = dynamic_cast<const NbtShort*>(&tag)) {
        std::string name_part = include_name ? get_name_before_value(tag, opts) : "";
        return name_part + to_snbt_short(*s, opts);
    }
    if (auto* i = dynamic_cast<const NbtInt*>(&tag)) {
        std::string name_part = include_name ? get_name_before_value(tag, opts) : "";
        return name_part + to_snbt_int(*i, opts);
    }
    if (auto* l = dynamic_cast<const NbtLong*>(&tag)) {
        std::string name_part = include_name ? get_name_before_value(tag, opts) : "";
        return name_part + to_snbt_long(*l, opts);
    }
    if (auto* f = dynamic_cast<const NbtFloat*>(&tag)) {
        std::string name_part = include_name ? get_name_before_value(tag, opts) : "";
        return name_part + to_snbt_float(*f, opts);
    }
    if (auto* d = dynamic_cast<const NbtDouble*>(&tag)) {
        std::string name_part = include_name ? get_name_before_value(tag, opts) : "";
        return name_part + to_snbt_double(*d, opts);
    }
    if (auto* st = dynamic_cast<const NbtString*>(&tag)) {
        std::string name_part = include_name ? get_name_before_value(tag, opts) : "";
        return name_part + to_snbt_string(*st, opts);
    }
    if (auto* ba = dynamic_cast<const NbtByteArray*>(&tag)) {
        std::string name_part = include_name ? get_name_before_value(tag, opts) : "";
        return name_part + to_snbt_byte_array(*ba, opts);
    }
    if (auto* ia = dynamic_cast<const NbtIntArray*>(&tag)) {
        std::string name_part = include_name ? get_name_before_value(tag, opts) : "";
        return name_part + to_snbt_int_array(*ia, opts);
    }
    if (auto* la = dynamic_cast<const NbtLongArray*>(&tag)) {
        std::string name_part = include_name ? get_name_before_value(tag, opts) : "";
        return name_part + to_snbt_long_array(*la, opts);
    }

    // Containers
    if (opts.minified) {
        if (auto* compound = dynamic_cast<const NbtCompound*>(&tag)) {
            std::string result;
            if (include_name) result += get_name_before_value(tag, opts);
            result += Constants::kCompoundOpen;
            auto names = compound->names();
            for (size_t i = 0; i < names.size(); ++i) {
                if (i > 0) result += Constants::kValueSep;
                auto* child = compound->get(names[i]);
                if (child) result += to_snbt(*child, opts, true);
            }
            result += Constants::kCompoundClose;
            return result;
        }
        if (auto* list = dynamic_cast<const NbtList*>(&tag)) {
            std::string result;
            if (include_name) result += get_name_before_value(tag, opts);
            result += Constants::kListOpen;
            for (size_t i = 0; i < list->size(); ++i) {
                if (i > 0) result += Constants::kValueSep;
                result += to_snbt(*list->at(i), opts, false);
            }
            result += Constants::kListClose;
            return result;
        }
    } else {
        // Expanded (pretty-print)
        std::string result;
        if (auto* compound = dynamic_cast<const NbtCompound*>(&tag)) {
            add_snbt_compound(*compound, opts, result, "    ", 0, include_name);
            return result;
        }
        if (auto* list = dynamic_cast<const NbtList*>(&tag)) {
            add_snbt_list(*list, opts, result, "    ", 0, include_name);
            return result;
        }
    }

    return include_name ? get_name_before_value(tag, opts) + "?" : "?";
}

} // namespace snbt
} // namespace nbtcpp
