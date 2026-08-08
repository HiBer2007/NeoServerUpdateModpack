#ifndef NBTCPP_NBT_READER_H
#define NBTCPP_NBT_READER_H

/**
 * @file nbt_reader.h
 * @brief Forward-only, stream-based NBT parser.
 *
 * NbtReader provides a fast, non-cached, forward-only way to traverse
 * NBT data streams without building the full tag tree.  It mirrors the
 * API of the C# NbtReader class in the fNbt library.
 *
 * Typical usage:
 * @code
 *   NbtReader reader(stream, true);  // big-endian
 *   while (reader.read_to_following()) {
 *       std::cout << reader.tag_name() << ": "
 *                 << reader.tag_type() << std::endl;
 *   }
 * @endcode
 */

#include "nbtcpp/nbt_tag_type.h"
#include "nbtcpp/nbt_binary_reader.h"

#include <cstdint>
#include <memory>
#include <stack>
#include <string>

namespace nbtcpp {

/// @brief Internal node state for tracking hierarchy during traversal.
struct NbtReaderNode {
    std::string parent_name;
    NbtTagType  parent_tag_type = NbtTagType::Unknown;
    NbtTagType  list_type       = NbtTagType::Unknown;
    int32_t     parent_tag_length = 0;
    int32_t     list_index        = 0;
};

/**
 * @brief Forward-only, non-cached reader for NBT data.
 *
 * Reads tags one at a time from a binary stream.  Supports skipping,
 * depth tracking, and all NBT tag types.
 */
class NbtReader {
public:
    /**
     * @brief Construct a reader bound to a stream.
     * @param stream     The input stream.
     * @param big_endian true for Java Edition, false for Bedrock Edition.
     */
    explicit NbtReader(std::istream& stream, bool big_endian = true);

    // ─── Navigation ─────────────────────────────────────────────────────

    /**
     * @brief Read the next tag from the stream.
     * @return true if another tag was read; false if end of stream.
     */
    bool read_to_following();

    /**
     * @brief Read until a tag with the given name is found.
     * @param tag_name  Tag name to search for (nullptr skips unnamed tags).
     * @return true if found, false if end of stream.
     */
    bool read_to_following(const std::string& tag_name);

    /**
     * @brief Read until a descendant tag with the given name is found.
     * @param tag_name  Tag name to search for.
     * @return true if found, false if not found in subtree.
     */
    bool read_to_descendant(const std::string& tag_name);

    // ─── Tag info ───────────────────────────────────────────────────────

    /** @brief Name of the current tag. */
    const std::string& tag_name() const noexcept { return tag_name_; }

    /** @brief Type of the current tag. */
    NbtTagType tag_type() const noexcept { return tag_type_; }

    /** @brief Name of the parent tag (empty if root). */
    const std::string& parent_name() const noexcept { return parent_name_; }

    /** @brief Type of the parent tag. */
    NbtTagType parent_tag_type() const noexcept { return parent_tag_type_; }

    /** @brief Depth in the tree (root is 1). */
    int depth() const noexcept { return depth_; }

    /** @brief Number of tags read so far. */
    int tags_read() const noexcept { return tags_read_; }

    /** @brief Whether the current tag is inside a list. */
    bool is_list_element() const noexcept { return parent_tag_type_ == NbtTagType::List; }

    /** @brief Whether the current tag has a value (opposite of Compound/List/End). */
    bool has_value() const noexcept { return nbtcpp::has_value(tag_type_); }

    /** @brief Whether the current tag has a name. */
    bool has_name() const noexcept { return !tag_name_.empty(); }

    /** @brief Whether we've reached end of stream. */
    bool is_at_stream_end() const noexcept { return state_ == State::AtStreamEnd; }

    /** @brief Whether current tag is Compound. */
    bool is_compound() const noexcept { return tag_type_ == NbtTagType::Compound; }

    /** @brief Whether current tag is List. */
    bool is_list() const noexcept { return tag_type_ == NbtTagType::List; }

    /** @brief Whether current tag has a length field (List, ByteArray, IntArray, LongArray). */
    bool has_length() const noexcept;

    /** @brief List element type (applicable if current tag is List). */
    NbtTagType list_type() const noexcept { return list_type_; }

    /** @brief Length of List / array current tag. */
    int32_t tag_length() const noexcept { return tag_length_; }

    /** @brief Length of parent list (if in list). */
    int32_t parent_tag_length() const noexcept { return parent_tag_length_; }

    /** @brief Index within parent list. */
    int32_t list_index() const noexcept { return list_index_; }

    /** @brief Byte offset of current tag from stream start. */
    int64_t tag_start_offset() const noexcept { return tag_start_offset_; }

    /** @brief Whether the reader is in an error state. */
    bool is_in_error_state() const noexcept {
        return state_ == State::Error;
    }

    /** @brief Name of the root tag. */
    const std::string& root_name() const noexcept { return root_name_; }

    // ─── Value reading ──────────────────────────────────────────────────

    /** @brief Read the value of the current scalar tag. */
    int8_t   read_byte_value();
    int16_t  read_short_value();
    int32_t  read_int_value();
    int64_t  read_long_value();
    float    read_float_value();
    double   read_double_value();
    std::string read_string_value();

    /** @brief Skip the current tag's value without constructing it. */
    void skip_value();

    // ─── Settings ───────────────────────────────────────────────────────

    /** @brief Whether to skip End tags when counting (default: true). */
    bool skip_end_tags() const noexcept { return skip_end_tags_; }
    void set_skip_end_tags(bool v) noexcept { skip_end_tags_ = v; }

    /** @brief Whether to cache tag values (default: false). */
    bool cache_tag_values() const noexcept { return cache_tag_values_; }
    void set_cache_tag_values(bool v) noexcept { cache_tag_values_ = v; }

    /** @brief Get the underlying binary reader (advanced use). */
    NbtBinaryReader& binary_reader() { return reader_; }

    /** @brief Get the underlying stream. */
    std::istream& base_stream() { return reader_.stream(); }

private:
    enum class State {
        AtStreamBeginning,
        AtCompoundBeginning,
        InCompound,
        AtCompoundEnd,
        AtListBeginning,
        InList,
        AtStreamEnd,
        Error
    };

    State state_ = State::AtStreamBeginning;

    // Internal state
    bool skip_end_tags_ = true;
    bool cache_tag_values_ = false;
    bool at_value_ = false;

    // Hierarchy tracking
    std::stack<NbtReaderNode> nodes_;
    std::string root_name_;
    std::string parent_name_;
    std::string tag_name_;
    NbtTagType  parent_tag_type_ = NbtTagType::Unknown;
    NbtTagType  tag_type_ = NbtTagType::Unknown;
    NbtTagType  list_type_ = NbtTagType::Unknown;
    int32_t parent_tag_length_ = 0;
    int32_t tag_length_ = 0;
    int32_t list_index_ = 0;
    int depth_ = 0;
    int tags_read_ = 0;
    int64_t tag_start_offset_ = 0;

    // Stream reading
    NbtBinaryReader reader_;
    int64_t stream_start_offset_ = 0;

    // Helpers
    void read_tag_header(bool read_name);
    void go_down();
    void go_up();
    void skip_value_internal();
};

} // namespace nbtcpp

#endif // NBTCPP_NBT_READER_H
