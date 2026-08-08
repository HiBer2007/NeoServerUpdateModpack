#ifndef NBTCPP_SNBT_MAKER_H
#define NBTCPP_SNBT_MAKER_H

/**
 * @file snbt_maker.h
 * @brief SNBT (String NBT) serializer.
 *
 * Converts an NbtTag tree back to SNBT text format with configurable
 * formatting options.
 *
 * Supports:
 * - Minified (compact) or expanded (pretty-printed) output
 * - JSON-like output (quoted keys, no number suffixes)
 * - Preview mode (short values, no quotes)
 * - Customizable quoting, indentation, and newline handling
 */

#include "nbtcpp/tags/nbt_tag.h"
#include <cstdint>
#include <functional>
#include <string>

namespace nbtcpp {
namespace snbt {

/**
 * @brief Formatting options for SNBT serialization.
 */
struct Options {
    /** @brief Whether to produce minified (compact) output. */
    bool minified = true;

    /** @brief Whether to produce JSON-like output. */
    bool is_json_like = false;

    /** @brief Whether to include number type suffixes (e.g., 42b, 3.14f). */
    bool number_suffixes = true;

    /** @brief Whether to include array prefixes (e.g., [B;, [I;, [L;). */
    bool array_prefixes = true;

    /** @brief Whether to render bytes 0/1 as false/true. */
    bool bytes_as_bools = false;

    /** @brief Function predicate: should a key be quoted? */
    std::function<bool(const std::string&)> should_quote_keys =
        [](const std::string& s) { return s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._+-") != std::string::npos || s.empty(); };

    /** @brief Function predicate: should a string value be quoted? */
    std::function<bool(const std::string&)> should_quote_strings =
        [](const std::string&) { return true; };

    /** @brief How to handle newlines in string values. */
    std::function<std::string()> newline_handler = []() { return "\\n"; };

    // ─── Presets ────────────────────────────────────────────────────────

    /** @brief Default compact format. */
    static Options default_options();

    /** @brief Default expanded (pretty-print) format. */
    static Options default_expanded();

    /** @brief JSON-like format. */
    static Options json_like();

    /** @brief JSON-like expanded format. */
    static Options json_like_expanded();

    /** @brief Preview format (no quotes, no prefixes). */
    static Options preview();
};

/**
 * @brief Serialize an NbtTag to SNBT text.
 *
 * @param tag    The tag to serialize.
 * @param opts   Formatting options.
 * @param include_name  Whether to include the tag's name as a prefix.
 * @return SNBT text string.
 */
std::string to_snbt(const NbtTag& tag, const Options& opts = Options::default_options(),
                    bool include_name = false);

/**
 * @brief Serialize an NbtTag to SNBT text (convenience overload).
 * @param tag    The tag to serialize (by unique pointer).
 */
inline std::string to_snbt(const NbtTagPtr& tag, const Options& opts = Options::default_options(),
                           bool include_name = false) {
    return tag ? to_snbt(*tag, opts, include_name) : "null";
}

} // namespace snbt
} // namespace nbtcpp

#endif // NBTCPP_SNBT_MAKER_H
