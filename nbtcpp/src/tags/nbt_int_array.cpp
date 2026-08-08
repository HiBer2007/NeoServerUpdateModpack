#include "nbtcpp/tags/nbt_int_array.h"
#include "nbtcpp/nbt_exception.h"

namespace nbtcpp {

bool NbtIntArray::read_tag(NbtBinaryReader& reader) {
    int32_t len = reader.read_int32();
    if (len < 0) throw NbtFormatException("Negative IntArray length: " + std::to_string(len));
    value_.resize(static_cast<size_t>(len));
    for (int32_t i = 0; i < len; ++i) value_[i] = reader.read_int32();
    return true;
}

void NbtIntArray::skip_tag(NbtBinaryReader& reader) {
    int32_t len = reader.read_int32();
    if (len > 0) reader.skip(static_cast<int64_t>(len) * 4);
}

void NbtIntArray::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::IntArray);
    writer.write(name());
    writer.write(static_cast<int32_t>(value_.size()));
    for (auto v : value_) writer.write(v);
}

void NbtIntArray::write_data(NbtBinaryWriter& writer) const {
    writer.write(static_cast<int32_t>(value_.size()));
    for (auto v : value_) writer.write(v);
}

void NbtIntArray::pretty_print(std::string& out, const std::string& indent,
                               int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += "[I; ";
    for (size_t i = 0; i < value_.size() && i < 16; ++i) {
        if (i > 0) out += ", ";
        out += std::to_string(value_[i]);
    }
    if (value_.size() > 16) out += ", ...";
    out += " (" + std::to_string(value_.size()) + " ints)\n";
}

} // namespace nbtcpp
