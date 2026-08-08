#ifndef NBTCPP_SNBT_PARSER_H
#define NBTCPP_SNBT_PARSER_H

/**
 * @file snbt_parser.h
 * @brief SNBT (String NBT) parser.
 *
 * Parses the Minecraft SNBT text format into NbtTag objects.
 * The SNBT format is used in commands like /give and /summon,
 * and in .snbt files.
 *
 * Supported syntax:
 * - Compounds: { key: value, ... }
 * - Lists: [ value, value, ... ]
 * - Arrays: [B; byte, ...], [I; int, ...], [L; long, ...]
 * - Typed scalars: 42b (byte), 42s (short), 42L (long), 4.2f (float), 4.2d (double)
 * - Untyped integers: 42 → int
 * - Untyped decimals: 4.2 → double
 * - Booleans: true → 1b, false → 0b
 * - Strings: "quoted" or unquoted (certain chars)
 * - Special floats: Infinity, -Infinity, NaN
 */

#include "nbtcpp/tags/nbt_tag.h"
#include "nbtcpp/tags/nbt_compound.h"

#include <cstdint>
#include <memory>
#include <string>

namespace nbtcpp {
namespace snbt {

/**
 * @brief Parse an SNBT string into an NbtTag tree.
 *
 * @param snbt   The SNBT text input.
 * @param named  If true, expects a named value (key: value at top level).
 * @return Unique pointer to the parsed tag.
 * @throws NbtFormatException on parse errors.
 */
NbtTagPtr parse(const std::string& snbt, bool named = false);

/**
 * @brief Attempt to parse SNBT, returning nullptr on failure.
 */
NbtTagPtr try_parse(const std::string& snbt, bool named = false) noexcept;

/**
 * @brief SNBT-specific constants.
 */
struct Constants {
    static constexpr char kByteSuffix        = 'b';
    static constexpr char kShortSuffix       = 's';
    static constexpr char kLongSuffix        = 'L';
    static constexpr char kFloatSuffix       = 'f';
    static constexpr char kDoubleSuffix      = 'd';
    static constexpr char kByteArrayPrefix   = 'B';
    static constexpr char kIntArrayPrefix    = 'I';
    static constexpr char kLongArrayPrefix   = 'L';
    static constexpr char kNameValueSep      = ':';
    static constexpr char kValueSep          = ',';
    static constexpr char kArrayDelim        = ';';
    static constexpr char kListOpen          = '[';
    static constexpr char kListClose         = ']';
    static constexpr char kCompoundOpen      = '{';
    static constexpr char kCompoundClose     = '}';
    static constexpr char kEscape            = '\\';
    static constexpr char kPrimaryQuote      = '"';
    static constexpr char kSecondaryQuote    = '\'';
};

} // namespace snbt
} // namespace nbtcpp

#endif // NBTCPP_SNBT_PARSER_H
