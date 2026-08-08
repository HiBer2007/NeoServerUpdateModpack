/**
 * @file nbt_reader.cpp
 * @brief Implementation of NbtReader (forward-only stream parser).
 */

#include "nbtcpp/nbt_reader.h"
#include "nbtcpp/nbt_exception.h"
#include <sstream>
#include <string>

namespace nbtcpp {

// ─── Construction ────────────────────────────────────────────────────────────

static const char* kInvalidParentTagError = "NbtReader encountered invalid parent tag state";
static const char* kErroneousStateError   = "NbtReader is in an erroneous state";
static const char* kNonValueTagError      = "Cannot get the value of a non-value tag";

NbtReader::NbtReader(std::istream& stream, bool big_endian)
    : reader_(stream, big_endian)
{
    if (stream.tellg() >= 0) stream_start_offset_ = static_cast<int64_t>(stream.tellg());
}

// ─── Navigation ─────────────────────────────────────────────────────────────

bool NbtReader::read_to_following() {
    // Use iterative approach: retry loop handles state transitions that
    // need to immediately re-enter a different state (e.g. CompoundEnd → InCompound).
    for (;;) {
        switch (state_) {
            case State::AtStreamBeginning: {
                state_ = State::Error;
                NbtTagType first = reader_.read_tag_type();
                if (first != NbtTagType::Compound) {
                    state_ = State::Error;
                    throw NbtFormatException("Given NBT stream does not start with a TAG_Compound");
                }
                depth_ = 1;
                tag_type_ = NbtTagType::Compound;
                read_tag_header(true);
                root_name_ = tag_name_;
                return true;
            }

            case State::AtCompoundBeginning:
                go_down();
                state_ = State::InCompound;
                // Fall through to InCompound
                continue;

            case State::InCompound:
                if (at_value_) skip_value_internal();
                if (reader_.can_seek()) {
                    auto pos = static_cast<int64_t>(reader_.stream().tellg());
                    if (pos >= 0) tag_start_offset_ = pos - stream_start_offset_;
                }
                tag_type_ = reader_.read_tag_type();
                if (tag_type_ == NbtTagType::End) {
                    tag_name_.clear();
                    tags_read_++;
                    state_ = State::AtCompoundEnd;
                    if (skip_end_tags_) {
                        tags_read_--;
                        continue; // re-enter AtCompoundEnd
                    }
                    return true;
                }
                read_tag_header(true);
                return true;

            case State::AtListBeginning:
                go_down();
                list_index_ = -1;
                tag_type_ = list_type_;
                state_ = State::InList;
                // Fall through to InList
                continue;

            case State::InList:
                if (at_value_) skip_value_internal();
                list_index_++;
                if (list_index_ >= parent_tag_length_) {
                    go_up();
                    if (parent_tag_type_ == NbtTagType::List) {
                        state_ = State::InList;
                        tag_type_ = NbtTagType::List;
                        continue;
                    } else if (parent_tag_type_ == NbtTagType::Compound) {
                        state_ = State::InCompound;
                        continue;
                    } else {
                        state_ = State::Error;
                        throw NbtFormatException(kInvalidParentTagError);
                    }
                }
                if (reader_.can_seek()) {
                    auto pos = static_cast<int64_t>(reader_.stream().tellg());
                    if (pos >= 0) tag_start_offset_ = pos - stream_start_offset_;
                }
                read_tag_header(false);
                return true;

            case State::AtCompoundEnd:
                go_up();
                if (parent_tag_type_ == NbtTagType::List) {
                    state_ = State::InList;
                    tag_type_ = NbtTagType::Compound;
                    continue;
                } else if (parent_tag_type_ == NbtTagType::Compound) {
                    state_ = State::InCompound;
                    continue;
                } else if (parent_tag_type_ == NbtTagType::Unknown) {
                    state_ = State::AtStreamEnd;
                    return false;
                } else {
                    state_ = State::Error;
                    throw NbtFormatException(kInvalidParentTagError);
                }

            case State::AtStreamEnd:
                return false;

            default:
                throw InvalidReaderStateException(kErroneousStateError);
        }
    }
}

bool NbtReader::read_to_following(const std::string& tag_name) {
    while (read_to_following()) {
        if (tag_name_ == tag_name || (tag_name.empty() && tag_name_.empty())) {
            return true;
        }
    }
    return false;
}

bool NbtReader::read_to_descendant(const std::string& tag_name) {
    if (state_ == State::Error) {
        throw InvalidReaderStateException(kErroneousStateError);
    }
    if (state_ == State::AtStreamEnd) return false;

    int current_depth = depth_;
    while (read_to_following()) {
        if (depth_ <= current_depth) return false;
        if (tag_name_ == tag_name) return true;
    }
    return false;
}

// ─── Tag header reading ─────────────────────────────────────────────────────

void NbtReader::read_tag_header(bool read_name) {
    tags_read_++;
    tag_name_ = read_name ? reader_.read_string() : "";
    at_value_ = false;
    tag_length_ = 0;
    list_type_ = NbtTagType::Unknown;

    switch (tag_type_) {
        case NbtTagType::Byte:
        case NbtTagType::Short:
        case NbtTagType::Int:
        case NbtTagType::Long:
        case NbtTagType::Float:
        case NbtTagType::Double:
        case NbtTagType::String:
            at_value_ = true;
            break;

        case NbtTagType::IntArray:
        case NbtTagType::ByteArray:
        case NbtTagType::LongArray:
            tag_length_ = reader_.read_int32();
            at_value_ = true;
            break;

        case NbtTagType::List:
            state_ = State::Error;
            list_type_ = reader_.read_tag_type();
            tag_length_ = reader_.read_int32();
            if (tag_length_ < 0) {
                throw NbtFormatException("Negative list length: " + std::to_string(tag_length_));
            }
            state_ = State::AtListBeginning;
            break;

        case NbtTagType::Compound:
            state_ = State::AtCompoundBeginning;
            break;

        default:
            state_ = State::Error;
            throw NbtFormatException("Unknown tag type: " + std::to_string(static_cast<int>(tag_type_)));
    }
}

// ─── Hierarchy ──────────────────────────────────────────────────────────────

void NbtReader::go_down() {
    NbtReaderNode node;
    node.parent_name       = parent_name_;
    node.parent_tag_type   = parent_tag_type_;
    node.list_type         = list_type_;
    node.parent_tag_length = parent_tag_length_;
    node.list_index        = list_index_;
    nodes_.push(std::move(node));

    parent_name_       = tag_name_;
    parent_tag_type_   = tag_type_;
    parent_tag_length_ = tag_length_;
    list_index_        = 0;
    tag_length_        = 0;
    depth_++;
}

void NbtReader::go_up() {
    if (nodes_.empty()) {
        throw InvalidReaderStateException("Cannot go up: no parent node");
    }
    auto& node = nodes_.top();
    parent_name_       = node.parent_name;
    parent_tag_type_   = node.parent_tag_type;
    list_type_         = node.list_type;
    parent_tag_length_ = node.parent_tag_length;
    list_index_        = node.list_index;
    tag_length_        = 0;
    depth_--;
    nodes_.pop();
}

// ─── Value reading ──────────────────────────────────────────────────────────

int8_t NbtReader::read_byte_value() {
    if (!has_value() || !at_value_) throw InvalidReaderStateException(kNonValueTagError);
    at_value_ = false;
    return static_cast<int8_t>(reader_.read_byte());
}

int16_t NbtReader::read_short_value() {
    if (!has_value() || !at_value_) throw InvalidReaderStateException(kNonValueTagError);
    at_value_ = false;
    return reader_.read_int16();
}

int32_t NbtReader::read_int_value() {
    if (!has_value() || !at_value_) throw InvalidReaderStateException(kNonValueTagError);
    at_value_ = false;
    return reader_.read_int32();
}

int64_t NbtReader::read_long_value() {
    if (!has_value() || !at_value_) throw InvalidReaderStateException(kNonValueTagError);
    at_value_ = false;
    return reader_.read_int64();
}

float NbtReader::read_float_value() {
    if (!has_value() || !at_value_) throw InvalidReaderStateException(kNonValueTagError);
    at_value_ = false;
    return reader_.read_float();
}

double NbtReader::read_double_value() {
    if (!has_value() || !at_value_) throw InvalidReaderStateException(kNonValueTagError);
    at_value_ = false;
    return reader_.read_double();
}

std::string NbtReader::read_string_value() {
    if (!has_value() || !at_value_) throw InvalidReaderStateException(kNonValueTagError);
    at_value_ = false;
    return reader_.read_string();
}

// ─── Skip ───────────────────────────────────────────────────────────────────

void NbtReader::skip_value_internal() {
    switch (tag_type_) {
        case NbtTagType::Byte:    reader_.read_byte(); break;
        case NbtTagType::Short:   reader_.read_int16(); break;
        case NbtTagType::Float:
        case NbtTagType::Int:     reader_.read_int32(); break;
        case NbtTagType::Double:
        case NbtTagType::Long:    reader_.read_int64(); break;
        case NbtTagType::ByteArray:    reader_.skip(tag_length_); break;
        case NbtTagType::IntArray:     reader_.skip(static_cast<int64_t>(tag_length_) * 4); break;
        case NbtTagType::LongArray:    reader_.skip(static_cast<int64_t>(tag_length_) * 8); break;
        case NbtTagType::String:  reader_.skip_string(); break;
        default: throw InvalidReaderStateException(kNonValueTagError);
    }
    at_value_ = false;
}

void NbtReader::skip_value() {
    skip_value_internal();
}

bool NbtReader::has_length() const noexcept {
    switch (tag_type_) {
        case NbtTagType::List:
        case NbtTagType::ByteArray:
        case NbtTagType::IntArray:
        case NbtTagType::LongArray:
            return true;
        default:
            return false;
    }
}

} // namespace nbtcpp
