#include "nbtcpp/tags/nbt_short.h"

namespace nbtcpp {

bool NbtShort::read_tag(NbtBinaryReader& reader) {
    value_ = reader.read_int16();
    return true;
}

void NbtShort::skip_tag(NbtBinaryReader& reader) {
    reader.skip(2);
}

void NbtShort::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::Short);
    writer.write(name());
    writer.write(value_);
}

void NbtShort::write_data(NbtBinaryWriter& writer) const {
    writer.write(value_);
}

void NbtShort::pretty_print(std::string& out, const std::string& indent,
                           int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += std::to_string(value_) + "s\n";
}

} // namespace nbtcpp
