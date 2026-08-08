#ifndef NBTCPP_NBT_BINARY_READER_H
#define NBTCPP_NBT_BINARY_READER_H

/**
 * @file nbt_binary_reader.h
 * @brief Low-level binary reader for NBT streams.
 *
 * NbtBinaryReader wraps an input stream and provides methods for reading
 * NBT primitives (tag types, integers, floating-point values, strings)
 * with automatic byte-order correction (big-endian for Java Edition,
 * little-endian for Bedrock Edition).
 *
 * This class is internal to the library; most users will prefer NbtReader
 * (stream-based traversal) or NbtFile (file-level I/O).
 */

#include "nbtcpp/nbt_tag_type.h"
#include "nbtcpp/endian_utils.h"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace nbtcpp {

/**
 * @brief Low-level binary reader for NBT data streams.
 *
 * Reads primitives from a std::istream while handling:
 * - Byte-order conversion (big-endian Java vs little-endian Bedrock)
 * - Length-prefixed UTF-8 strings
 * - Raw byte skipping for non-seekable streams
 * - Optional tag selection callback (TagSelector)
 */
class NbtBinaryReader {
public:
    /**
     * @brief Construct a reader bound to an input stream.
     *
     * @param stream    The input stream to read from.  Must be readable.
     * @param big_endian  true for Java Edition (big-endian), false for Bedrock (little-endian).
     */
    explicit NbtBinaryReader(std::istream& stream, bool big_endian = true) noexcept;

    // ─── Read API ───────────────────────────────────────────────────────

    /** @brief Read a single byte as an NbtTagType. */
    NbtTagType read_tag_type();

    /** @brief Read a raw byte (used for tag type IDs). */
    uint8_t read_byte();

    /** @brief Read a signed 16-bit integer (with byte-order correction). */
    int16_t read_int16();

    /** @brief Read a signed 32-bit integer (with byte-order correction). */
    int32_t read_int32();

    /** @brief Read a signed 64-bit integer (with byte-order correction). */
    int64_t read_int64();

    /** @brief Read a 32-bit float (with byte-order correction). */
    float read_float();

    /** @brief Read a 64-bit double (with byte-order correction). */
    double read_double();

    /**
     * @brief Read a length-prefixed UTF-8 string.
     *
     * The wire format is: [int16 byte_count][UTF-8 bytes].
     * The byte_count includes the length prefix itself (Minecraft convention).
     * Both int16 and bytes are subject to the configured byte order.
     */
    std::string read_string();

    /**
     * @brief Skip a length-prefixed string without allocating.
     *
     * Equivalent to reading but discarding the data.  Useful during
     * selective parsing when skipping unwanted tags.
     */
    void skip_string();

    /**
     * @brief Skip a given number of bytes.
     *
     * If the underlying stream is seekable, advances the position directly.
     * Otherwise, reads and discards the bytes in chunks.
     *
     * @param count  Number of bytes to skip.  Must be non-negative.
     */
    void skip(int64_t count);

    /**
     * @brief Read a fixed number of bytes into a vector.
     */
    std::vector<uint8_t> read_bytes(int32_t count);

    /**
     * @brief Read bytes directly into a pre-allocated buffer.
     *
     * @param buffer  Destination buffer.
     * @param count   Number of bytes to read.
     * @return The number of bytes actually read.
     */
    int32_t read_raw(uint8_t* buffer, int32_t count);

    // ─── Stream access ──────────────────────────────────────────────────

    /** @brief Access the underlying input stream. */
    std::istream& stream() const noexcept { return stream_; }

    /** @brief Whether the underlying stream supports seeking. */
    bool can_seek() const noexcept { return can_seek_; }

    // ─── Tag selector support ───────────────────────────────────────────

    /**
     * @brief Callback type for selective tag loading.
     *
     * The callback receives a tag name and type before the tag value is
     * read.  Return true to include the tag, false to skip it.
     */
    using TagSelector = bool (*)(const std::string& name, NbtTagType type);

    /** @brief Set an optional tag selector callback. */
    void set_selector(TagSelector selector) noexcept { selector_ = selector; }

    /** @brief Get the current tag selector, or nullptr. */
    TagSelector selector() const noexcept { return selector_; }

private:
    std::istream& stream_;
    detail::EndianConverter conv_;
    bool can_seek_;

    /** Optional tag selector. */
    TagSelector selector_ = nullptr;

    /** Reusable buffer for small reads. */
    uint8_t buffer_[8];

    /** Internal buffer used for seeking on non-seekable streams. */
    static constexpr int64_t kSeekBufferSize = 8192;

    /** Helper: fill buffer_ with exactly N bytes from stream. */
    void fill_buffer(int num_bytes);
};

} // namespace nbtcpp

#endif // NBTCPP_NBT_BINARY_READER_H
