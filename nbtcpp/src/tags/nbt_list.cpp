/**
 * @file nbt_list.cpp
 * @brief Implementation of NbtList.
 */

#include "nbtcpp/tags/nbt_list.h"
#include "nbtcpp/tags/nbt_compound.h"  // for create_tag factory
#include "nbtcpp/nbt_exception.h"
#include <algorithm>
#include <sstream>

namespace nbtcpp {

// ─── Construction ────────────────────────────────────────────────────────────

NbtList::NbtList(const NbtList& other)
    : NbtContainerTag(other)
    , list_type_(other.list_type_)
{
    for (const auto& tag : other.tags_) {
        auto clone = tag->clone();
        adopt(clone.get());
        tags_.push_back(std::move(clone));
    }
    rebuild_raw_ptrs();
}

// ─── List type ───────────────────────────────────────────────────────────────

void NbtList::set_list_type(NbtTagType type) {
    if (type == NbtTagType::End && !tags_.empty()) {
        throw std::invalid_argument("Only empty lists may have TagType of End");
    }
    if (type < NbtTagType::Byte || (type > NbtTagType::LongArray && type != NbtTagType::Unknown)) {
        throw std::invalid_argument("Invalid list type: " + std::to_string(static_cast<int>(type)));
    }
    if (!tags_.empty()) {
        NbtTagType actual = tags_[0]->tag_type();
        if (actual != type) {
            throw std::invalid_argument("Given list type (" + nbtcpp::to_string(type) +
                                        ") does not match element type (" + nbtcpp::to_string(actual) + ")");
        }
    }
    list_type_ = type;
}

// ─── Container interface ─────────────────────────────────────────────────────

NbtTag* NbtList::at(size_t index) {
    if (index >= tags_.size()) throw std::out_of_range("NbtList index out of range");
    return tags_[index].get();
}

const NbtTag* NbtList::at(size_t index) const {
    if (index >= tags_.size()) throw std::out_of_range("NbtList index out of range");
    return tags_[index].get();
}

void NbtList::add(NbtTagPtr tag) {
    if (!tag) throw std::invalid_argument("Cannot add null tag to NbtList");
    if (tags_.empty() && list_type_ == NbtTagType::Unknown) {
        list_type_ = tag->tag_type();
    } else {
        check_type(tag->tag_type());
    }
    adopt(tag.get());
    tags_.push_back(std::move(tag));
    rebuild_raw_ptrs();
    fire_changed();
}

void NbtList::insert(size_t index, NbtTagPtr tag) {
    if (!tag) throw std::invalid_argument("Cannot insert null tag into NbtList");
    if (index > tags_.size()) throw std::out_of_range("NbtList insert index out of range");
    if (tags_.empty() && list_type_ == NbtTagType::Unknown) {
        list_type_ = tag->tag_type();
    } else {
        check_type(tag->tag_type());
    }
    adopt(tag.get());
    tags_.insert(tags_.begin() + static_cast<ptrdiff_t>(index), std::move(tag));
    rebuild_raw_ptrs();
    fire_changed();
}

NbtTagPtr NbtList::remove(NbtTag* tag) {
    for (size_t i = 0; i < tags_.size(); ++i) {
        if (tags_[i].get() == tag) {
            return remove_at(i);
        }
    }
    return nullptr;
}

NbtTagPtr NbtList::remove_at(size_t index) {
    if (index >= tags_.size()) throw std::out_of_range("NbtList remove_at index out of range");
    auto tag = std::move(tags_[index]);
    tags_.erase(tags_.begin() + static_cast<ptrdiff_t>(index));
    tag->set_parent(nullptr);
    rebuild_raw_ptrs();
    fire_changed();
    return tag;
}

NbtTagPtr NbtList::remove(const std::string&) {
    throw std::runtime_error("NbtList does not support removal by name");
}

std::vector<NbtTagPtr> NbtList::clear() {
    std::vector<NbtTagPtr> result;
    for (auto& tag : tags_) tag->set_parent(nullptr);
    result.swap(tags_);
    rebuild_raw_ptrs();
    fire_changed();
    return result;
}

bool NbtList::can_add(NbtTagType type) const noexcept {
    if (tags_.empty()) return true;
    return list_type_ == type;
}

// ─── Iteration ───────────────────────────────────────────────────────────────

void NbtList::rebuild_raw_ptrs() {
    raw_ptrs_.clear();
    raw_ptrs_.reserve(tags_.size());
    for (auto& tag : tags_) raw_ptrs_.push_back(tag.get());
}

std::vector<NbtTag*>::iterator NbtList::begin() { return raw_ptrs_.begin(); }
std::vector<NbtTag*>::iterator NbtList::end()   { return raw_ptrs_.end(); }
std::vector<NbtTag*>::const_iterator NbtList::begin() const { return raw_ptrs_.begin(); }
std::vector<NbtTag*>::const_iterator NbtList::end() const   { return raw_ptrs_.end(); }

// ─── Type check ──────────────────────────────────────────────────────────────

void NbtList::check_type(NbtTagType type) const {
    if (tags_.empty()) return;
    if (list_type_ != type) {
        throw std::invalid_argument(
            "Cannot add " + nbtcpp::to_string(type) +
            " to list of " + nbtcpp::to_string(list_type_));
    }
}

// ─── Serialization ───────────────────────────────────────────────────────────

bool NbtList::read_tag(NbtBinaryReader& reader) {
    list_type_ = reader.read_tag_type();
    int32_t length = reader.read_int32();
    if (length < 0) throw NbtFormatException("Negative list length: " + std::to_string(length));

    for (int32_t i = 0; i < length; ++i) {
        auto tag = NbtCompound::create_tag(list_type_);
        adopt(tag.get());
        tag->read_tag(reader);
        tags_.push_back(std::move(tag));
    }
    rebuild_raw_ptrs();
    return true;
}

void NbtList::skip_tag(NbtBinaryReader& reader) {
    NbtTagType elem_type = reader.read_tag_type();
    int32_t length = reader.read_int32();
    if (length < 0) throw NbtFormatException("Negative list length: " + std::to_string(length));

    // Create a temporary tag just to call skip_tag
    auto dummy = NbtCompound::create_tag(elem_type);
    if (dummy) {
        for (int32_t i = 0; i < length; ++i) dummy->skip_tag(reader);
    } else {
        // Unknown type, skip a reasonable amount
        int64_t elem_size = 0;
        switch (elem_type) {
            case NbtTagType::Byte:    elem_size = 1; break;
            case NbtTagType::Short:   elem_size = 2; break;
            case NbtTagType::Int:     elem_size = 4; break;
            case NbtTagType::Long:    elem_size = 8; break;
            case NbtTagType::Float:   elem_size = 4; break;
            case NbtTagType::Double:  elem_size = 8; break;
            default:                  elem_size = 1; break; // best-effort
        }
        reader.skip(elem_size * length);
    }
}

void NbtList::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::List);
    writer.write(name());

    NbtTagType elem_type = list_type_;
    if (elem_type == NbtTagType::Unknown && !tags_.empty()) {
        elem_type = tags_[0]->tag_type();
    }
    writer.write(elem_type);
    writer.write(static_cast<int32_t>(tags_.size()));
    for (const auto& tag : tags_) tag->write_data(writer);
}

void NbtList::write_data(NbtBinaryWriter& writer) const {
    NbtTagType elem_type = list_type_;
    if (elem_type == NbtTagType::Unknown && !tags_.empty()) {
        elem_type = tags_[0]->tag_type();
    }
    writer.write(elem_type);
    writer.write(static_cast<int32_t>(tags_.size()));
    for (const auto& tag : tags_) tag->write_data(writer);
}

void NbtList::pretty_print(std::string& out, const std::string& indent,
                          int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += "[\n";
    for (size_t i = 0; i < tags_.size(); ++i) {
        tags_[i]->pretty_print(out, indent, level + 1);
        if (i < tags_.size() - 1) out.back() = ',';
    }
    for (int i = 0; i < level; ++i) out += indent;
    out += "]\n";
}

std::unique_ptr<NbtTag> NbtList::clone() const {
    return std::make_unique<NbtList>(*this);
}

} // namespace nbtcpp
