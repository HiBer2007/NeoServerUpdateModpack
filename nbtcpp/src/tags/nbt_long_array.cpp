#include "nbtcpp/tags/nbt_long_array.h"
#include "nbtcpp/nbt_exception.h"

namespace nbtcpp {

bool NbtLongArray::read_tag(NbtBinaryReader& reader) {
    int32_t len = reader.read_int32();
    if (len < 0) throw NbtFormatException("Negative LongArray length: " + std::to_string(len));
    value_.resize(static_cast<size_t>(len));
    for (int32_t i = 0; i < len; ++i) value_[i] = reader.read_int64();
    return true;
}

void NbtLongArray::skip_tag(NbtBinaryReader& reader) {
    int32_t len = reader.read_int32();
    if (len > 0) reader.skip(static_cast<int64_t>(len) * 8);
}

void NbtLongArray::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::LongArray);
    writer.write(name());
    writer.write(static_cast<int32_t>(value_.size()));
    for (auto v : value_) writer.write(v);
}

void NbtLongArray::write_data(NbtBinaryWriter& writer) const {
    writer.write(static_cast<int32_t>(value_.size()));
    for (auto v : value_) writer.write(v);
}

void NbtLongArray::pretty_print(std::string& out, const std::string& indent,
                                int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += "[L; ";
    for (size_t i = 0; i < value_.size() && i < 16; ++i) {
        if (i > 0) out += ", ";
        out += std::to_string(value_[i]) + "L";
    }
    if (value_.size() > 16) out += ", ...";
    out += " (" + std::to_string(value_.size()) + " longs)\n";
}

} // namespace nbtcpp
