#include "nbtcpp/tags/nbt_byte.h"
#include "nbtcpp/nbt_exception.h"

namespace nbtcpp {

bool NbtByte::read_tag(NbtBinaryReader& reader) {
    value_ = reader.read_byte();
    return true;
}

void NbtByte::skip_tag(NbtBinaryReader& reader) {
    reader.read_byte();
}

void NbtByte::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::Byte);
    writer.write(name());
    writer.write(value_);
}

void NbtByte::write_data(NbtBinaryWriter& writer) const {
    writer.write(value_);
}

void NbtByte::pretty_print(std::string& out, const std::string& indent,
                           int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += std::to_string(value_) + "b\n";
}

} // namespace nbtcpp
