#ifndef NBTCPP_NBT_COMPRESSION_H
#define NBTCPP_NBT_COMPRESSION_H

#include <cstdint>
#include <stdexcept>
#include <string>

namespace nbtcpp {

/**
 * @brief Compression methods used when reading/writing NBT files.
 *
 * The NBT format supports three compression states: uncompressed, GZip, and
 * ZLib (Deflate with a 2-byte header and 4-byte Adler32 trailer).
 * AutoDetect is only valid for reading and inspects the first bytes of the
 * stream to determine the compression.
 */
enum class NbtCompression : uint8_t {
    /// Automatically detect compression from stream header bytes (read-only).
    AutoDetect = 0,

    /// No compression; raw NBT data.
    None = 1,

    /// GZip compression with standard GZip header/trailer.
    GZip = 2,

    /// ZLib compression (RFC-1950): 2-byte header + Deflate + 4-byte Adler32.
    ZLib = 3
};

/**
 * @brief Convert an NbtCompression value to its human-readable name.
 */
inline std::string to_string(NbtCompression comp) {
    switch (comp) {
        case NbtCompression::AutoDetect: return "AutoDetect";
        case NbtCompression::None:       return "None";
        case NbtCompression::GZip:       return "GZip";
        case NbtCompression::ZLib:       return "ZLib";
        default:                         return "Unknown";
    }
}

/**
 * @brief Validate that a compression value is usable for saving.
 *
 * AutoDetect is not valid for saving because the encoder needs to know
 * the target format.
 *
 * @throws std::invalid_argument if comp is AutoDetect.
 */
inline void validate_for_save(NbtCompression comp) {
    if (comp == NbtCompression::AutoDetect) {
        throw std::invalid_argument(
            "AutoDetect is not a valid NbtCompression for saving. "
            "Specify None, GZip, or ZLib explicitly.");
    }
}

} // namespace nbtcpp

#endif // NBTCPP_NBT_COMPRESSION_H
