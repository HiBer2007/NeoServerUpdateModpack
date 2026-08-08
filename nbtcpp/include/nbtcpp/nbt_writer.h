#ifndef NBTCPP_NBT_WRITER_H
#define NBTCPP_NBT_WRITER_H

/**
 * @file nbt_writer.h
 * @brief Stream-based NBT writer for building NBT trees incrementally.
 *
 * NbtWriter provides a forward-only way to write NBT data to a stream
 * without constructing an in-memory tag tree.  It enforces all NBT format
 * constraints except duplicate name checking in compounds.
 *
 * Typical usage:
 * @code
 *   NbtWriter writer(stream, "RootName", true);  // big-endian
 *   writer.begin_compound("Level");
 *   writer.write_int("x", 42);
 *   writer.write_int("y", 64);
 *   writer.write_int("z", -128);
 *   writer.end_compound();
 *   writer.finish();
 * @endcode
 */

#include "nbtcpp/nbt_tag_type.h"
#include "nbtcpp/nbt_binary_writer.h"

#include <cstdint>
#include <memory>
#include <stack>
#include <string>

namespace nbtcpp {

/// @brief Internal node tracking for NbtWriter hierarchy.
struct NbtWriterNode {
    NbtTagType parent_type = NbtTagType::Unknown;
    NbtTagType list_type   = NbtTagType::Unknown;
    int32_t    list_index  = 0;
    int32_t    list_size   = 0;
};

/**
 * @brief Forward-only, stream-based NBT writer.
 *
 * Writes tags one at a time to a binary stream.  Enforces:
 * - Root must be a named compound.
 * - Lists must contain elements of a single type.
 * - List size must match the declared count.
 */
class NbtWriter {
public:
    /**
     * @brief Construct a writer bound to a stream.
     *
     * The root compound tag is written immediately.
     *
     * @param stream        Output stream.
     * @param root_tag_name Name of the root tag.
     * @param big_endian    true for Java Edition, false for Bedrock.
     */
    explicit NbtWriter(std::ostream& stream, const std::string& root_tag_name,
                       bool big_endian = true);

    /** @brief Destructor calls finish() to auto-close (no-throw). */
    ~NbtWriter() noexcept { try { if (!is_done_) finish(); } catch (...) {} }

    /**
     * @brief Finish writing.  Closes all open compounds/lists and verifies
     *        that lists have the expected number of elements.
     */
    void finish();

    /** @brief Whether the root has been closed (no more tags can be written). */
    bool is_done() const noexcept { return is_done_; }

    /** @brief Access the underlying stream. */
    std::ostream& base_stream() { return writer_.stream(); }

    // ─── Compounds ──────────────────────────────────────────────────────

    /** @brief Begin an unnamed compound (for list elements). */
    void begin_compound();
    /** @brief Begin a named compound. */
    void begin_compound(const std::string& tag_name);
    /** @brief End the current compound. */
    void end_compound();

    // ─── Lists ──────────────────────────────────────────────────────────

    /** @brief Begin an unnamed list. */
    void begin_list(NbtTagType element_type, int32_t size);
    /** @brief Begin a named list. */
    void begin_list(const std::string& tag_name, NbtTagType element_type, int32_t size);
    /** @brief End the current list. Must have written exactly `size` elements. */
    void end_list();

    // ─── Value tags ─────────────────────────────────────────────────────

    // Unnamed (for list elements)
    void write_byte(uint8_t value);
    void write_short(int16_t value);
    void write_int(int32_t value);
    void write_long(int64_t value);
    void write_float(float value);
    void write_double(double value);
    void write_string(const std::string& value);

    // Named (for compounds)
    void write_byte(const std::string& name, uint8_t value);
    void write_short(const std::string& name, int16_t value);
    void write_int(const std::string& name, int32_t value);
    void write_long(const std::string& name, int64_t value);
    void write_float(const std::string& name, float value);
    void write_double(const std::string& name, double value);
    void write_string(const std::string& name, const std::string& value);

    // Raw data tags (named)
    void write_byte_array(const std::string& name, const uint8_t* data, int32_t length);
    void write_int_array(const std::string& name, const int32_t* data, int32_t length);
    void write_long_array(const std::string& name, const int64_t* data, int32_t length);

    // Unnamed raw data (for list elements)
    void write_byte_array_raw(const uint8_t* data, int32_t length);
    void write_int_array_raw(const int32_t* data, int32_t length);
    void write_long_array_raw(const int64_t* data, int32_t length);

private:
    NbtBinaryWriter writer_;
    bool is_done_ = false;
    NbtTagType list_type_ = NbtTagType::Unknown;
    NbtTagType parent_type_ = NbtTagType::Compound;
    int32_t list_index_ = 0;
    int32_t list_size_ = 0;
    std::stack<NbtWriterNode> nodes_;

    void enforce_constraints(const std::string* name, NbtTagType type);
    void go_down(NbtTagType type);
    void go_up();
};

} // namespace nbtcpp

#endif // NBTCPP_NBT_WRITER_H
