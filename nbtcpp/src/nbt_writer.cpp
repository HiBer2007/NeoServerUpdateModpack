/**
 * @file nbt_writer.cpp
 * @brief Implementation of NbtWriter.
 */

#include "nbtcpp/nbt_writer.h"
#include "nbtcpp/nbt_exception.h"

namespace nbtcpp {

// ─── Construction ────────────────────────────────────────────────────────────

NbtWriter::NbtWriter(std::ostream& stream, const std::string& root_tag_name,
                     bool big_endian)
    : writer_(stream, big_endian)
{
    writer_.write(static_cast<uint8_t>(NbtTagType::Compound));
    writer_.write(root_tag_name);
    parent_type_ = NbtTagType::Compound;
}

// ─── Finish ──────────────────────────────────────────────────────────────────

void NbtWriter::finish() {
    while (!is_done_) {
        if (parent_type_ == NbtTagType::Compound) {
            end_compound();
        } else if (parent_type_ == NbtTagType::List) {
            end_list();
        } else {
            break;
        }
    }
}

// ─── Compounds ───────────────────────────────────────────────────────────────

void NbtWriter::begin_compound() {
    enforce_constraints(nullptr, NbtTagType::Compound);
    go_down(NbtTagType::Compound);
}

void NbtWriter::begin_compound(const std::string& tag_name) {
    enforce_constraints(&tag_name, NbtTagType::Compound);
    go_down(NbtTagType::Compound);
    writer_.write(static_cast<uint8_t>(NbtTagType::Compound));
    writer_.write(tag_name);
}

void NbtWriter::end_compound() {
    if (is_done_ || parent_type_ != NbtTagType::Compound) {
        throw NbtFormatException("Not currently in a compound");
    }
    go_up();
    writer_.write(NbtTagType::End);
}

// ─── Lists ───────────────────────────────────────────────────────────────────

void NbtWriter::begin_list(NbtTagType element_type, int32_t size) {
    if (size < 0) throw std::invalid_argument("List size may not be negative");
    if (element_type < NbtTagType::Byte || element_type > NbtTagType::LongArray) {
        throw std::invalid_argument("Invalid list element type");
    }
    enforce_constraints(nullptr, NbtTagType::List);
    go_down(NbtTagType::List);
    list_type_ = element_type;
    list_size_ = size;
    writer_.write(static_cast<uint8_t>(element_type));
    writer_.write(size);
}

void NbtWriter::begin_list(const std::string& tag_name, NbtTagType element_type, int32_t size) {
    if (size < 0) throw std::invalid_argument("List size may not be negative");
    if (element_type < NbtTagType::Byte || element_type > NbtTagType::LongArray) {
        throw std::invalid_argument("Invalid list element type");
    }
    enforce_constraints(&tag_name, NbtTagType::List);
    go_down(NbtTagType::List);
    list_type_ = element_type;
    list_size_ = size;
    writer_.write(static_cast<uint8_t>(NbtTagType::List));
    writer_.write(tag_name);
    writer_.write(static_cast<uint8_t>(element_type));
    writer_.write(size);
}

void NbtWriter::end_list() {
    if (parent_type_ != NbtTagType::List || is_done_) {
        throw NbtFormatException("Not currently in a list");
    }
    if (list_index_ < list_size_) {
        throw NbtFormatException("Cannot end list: expected " + std::to_string(list_size_) +
                                 " elements but wrote " + std::to_string(list_index_));
    }
    go_up();
}

// ─── Value tags (unnamed) ────────────────────────────────────────────────────

void NbtWriter::write_byte(uint8_t value) {
    enforce_constraints(nullptr, NbtTagType::Byte);
    writer_.write(value);
}

void NbtWriter::write_short(int16_t value) {
    enforce_constraints(nullptr, NbtTagType::Short);
    writer_.write(value);
}

void NbtWriter::write_int(int32_t value) {
    enforce_constraints(nullptr, NbtTagType::Int);
    writer_.write(value);
}

void NbtWriter::write_long(int64_t value) {
    enforce_constraints(nullptr, NbtTagType::Long);
    writer_.write(value);
}

void NbtWriter::write_float(float value) {
    enforce_constraints(nullptr, NbtTagType::Float);
    writer_.write(value);
}

void NbtWriter::write_double(double value) {
    enforce_constraints(nullptr, NbtTagType::Double);
    writer_.write(value);
}

void NbtWriter::write_string(const std::string& value) {
    enforce_constraints(nullptr, NbtTagType::String);
    writer_.write(value);
}

// ─── Value tags (named) ──────────────────────────────────────────────────────

void NbtWriter::write_byte(const std::string& name, uint8_t value) {
    enforce_constraints(&name, NbtTagType::Byte);
    writer_.write(static_cast<uint8_t>(NbtTagType::Byte));
    writer_.write(name);
    writer_.write(value);
}

void NbtWriter::write_short(const std::string& name, int16_t value) {
    enforce_constraints(&name, NbtTagType::Short);
    writer_.write(static_cast<uint8_t>(NbtTagType::Short));
    writer_.write(name);
    writer_.write(value);
}

void NbtWriter::write_int(const std::string& name, int32_t value) {
    enforce_constraints(&name, NbtTagType::Int);
    writer_.write(static_cast<uint8_t>(NbtTagType::Int));
    writer_.write(name);
    writer_.write(value);
}

void NbtWriter::write_long(const std::string& name, int64_t value) {
    enforce_constraints(&name, NbtTagType::Long);
    writer_.write(static_cast<uint8_t>(NbtTagType::Long));
    writer_.write(name);
    writer_.write(value);
}

void NbtWriter::write_float(const std::string& name, float value) {
    enforce_constraints(&name, NbtTagType::Float);
    writer_.write(static_cast<uint8_t>(NbtTagType::Float));
    writer_.write(name);
    writer_.write(value);
}

void NbtWriter::write_double(const std::string& name, double value) {
    enforce_constraints(&name, NbtTagType::Double);
    writer_.write(static_cast<uint8_t>(NbtTagType::Double));
    writer_.write(name);
    writer_.write(value);
}

void NbtWriter::write_string(const std::string& name, const std::string& value) {
    enforce_constraints(&name, NbtTagType::String);
    writer_.write(static_cast<uint8_t>(NbtTagType::String));
    writer_.write(name);
    writer_.write(value);
}

// ─── Raw array tags ──────────────────────────────────────────────────────────

void NbtWriter::write_byte_array(const std::string& name, const uint8_t* data, int32_t length) {
    enforce_constraints(&name, NbtTagType::ByteArray);
    writer_.write(static_cast<uint8_t>(NbtTagType::ByteArray));
    writer_.write(name);
    writer_.write(length);
    writer_.write_raw(data, length);
}

void NbtWriter::write_int_array(const std::string& name, const int32_t* data, int32_t length) {
    enforce_constraints(&name, NbtTagType::IntArray);
    writer_.write(static_cast<uint8_t>(NbtTagType::IntArray));
    writer_.write(name);
    writer_.write(length);
    for (int32_t i = 0; i < length; ++i) writer_.write(data[i]);
}

void NbtWriter::write_long_array(const std::string& name, const int64_t* data, int32_t length) {
    enforce_constraints(&name, NbtTagType::LongArray);
    writer_.write(static_cast<uint8_t>(NbtTagType::LongArray));
    writer_.write(name);
    writer_.write(length);
    for (int32_t i = 0; i < length; ++i) writer_.write(data[i]);
}

void NbtWriter::write_byte_array_raw(const uint8_t* data, int32_t length) {
    enforce_constraints(nullptr, NbtTagType::ByteArray);
    writer_.write(length);
    writer_.write_raw(data, length);
}

void NbtWriter::write_int_array_raw(const int32_t* data, int32_t length) {
    enforce_constraints(nullptr, NbtTagType::IntArray);
    writer_.write(length);
    for (int32_t i = 0; i < length; ++i) writer_.write(data[i]);
}

void NbtWriter::write_long_array_raw(const int64_t* data, int32_t length) {
    enforce_constraints(nullptr, NbtTagType::LongArray);
    writer_.write(length);
    for (int32_t i = 0; i < length; ++i) writer_.write(data[i]);
}

// ─── Internal helpers ────────────────────────────────────────────────────────

void NbtWriter::enforce_constraints(const std::string* name, NbtTagType type) {
    if (is_done_) throw NbtFormatException("Cannot write: root tag already closed");

    if (parent_type_ == NbtTagType::Compound) {
        // In a compound: named tags expected
        if (!name) throw NbtFormatException("Expected a named tag in compound context");
    } else if (parent_type_ == NbtTagType::List) {
        // In a list: unnamed tags expected, type must match
        if (name) throw NbtFormatException("Expected an unnamed tag in list context");
        if (type != list_type_) {
            throw NbtFormatException("List element type mismatch: expected " +
                                     to_string(list_type_) + ", got " + to_string(type));
        }
        if (list_index_ >= list_size_) {
            throw NbtFormatException("List size exceeded: wrote " + std::to_string(list_index_ + 1) +
                                     " but declared " + std::to_string(list_size_));
        }
        list_index_++;
    }
}

void NbtWriter::go_down(NbtTagType type) {
    NbtWriterNode node;
    node.parent_type = parent_type_;
    node.list_type   = list_type_;
    node.list_index  = list_index_;
    node.list_size   = list_size_;
    nodes_.push(std::move(node));

    parent_type_ = type;
    if (type == NbtTagType::List) {
        list_index_ = 0;
    }
}

void NbtWriter::go_up() {
    if (nodes_.empty()) {
        is_done_ = true;
        return;
    }
    auto& node = nodes_.top();
    parent_type_ = node.parent_type;
    list_type_   = node.list_type;
    list_index_  = node.list_index;
    list_size_   = node.list_size;
    nodes_.pop();
}

} // namespace nbtcpp
