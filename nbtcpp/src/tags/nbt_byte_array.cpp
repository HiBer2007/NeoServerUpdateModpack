#include "nbtcpp/tags/nbt_byte_array.h"
#include "nbtcpp/nbt_exception.h"
#include <sstream>

namespace nbtcpp {

bool NbtByteArray::read_tag(NbtBinaryReader& reader) {
    int32_t len = reader.read_int32();
    if (len < 0) throw NbtFormatException("Negative ByteArray length: " + std::to_string(len));
    value_.resize(static_cast<size_t>(len));
    if (len > 0) reader.read_raw(value_.data(), len);
    return true;
}

void NbtByteArray::skip_tag(NbtBinaryReader& reader) {
    int32_t len = reader.read_int32();
    if (len > 0) reader.skip(len);
}

void NbtByteArray::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::ByteArray);
    writer.write(name());
    writer.write(static_cast<int32_t>(value_.size()));
    writer.write_raw(value_.data(), static_cast<int32_t>(value_.size()));
}

void NbtByteArray::write_data(NbtBinaryWriter& writer) const {
    writer.write(static_cast<int32_t>(value_.size()));
    writer.write_raw(value_.data(), static_cast<int32_t>(value_.size()));
}

void NbtByteArray::pretty_print(std::string& out, const std::string& indent,
                               int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += "[B; ";
    for (size_t i = 0; i < value_.size() && i < 16; ++i) {
        if (i > 0) out += ", ";
        out += std::to_string(static_cast<int8_t>(value_[i])) + "b";
    }
    if (value_.size() > 16) out += ", ...";
    out += " (" + std::to_string(value_.size()) + " bytes)\n";
}

} // namespace nbtcpp
