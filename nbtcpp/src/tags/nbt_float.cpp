#include "nbtcpp/tags/nbt_float.h"
#include <sstream>

namespace nbtcpp {

bool NbtFloat::read_tag(NbtBinaryReader& reader) {
    value_ = reader.read_float();
    return true;
}

void NbtFloat::skip_tag(NbtBinaryReader& reader) {
    reader.skip(4);
}

void NbtFloat::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::Float);
    writer.write(name());
    writer.write(value_);
}

void NbtFloat::write_data(NbtBinaryWriter& writer) const {
    writer.write(value_);
}

void NbtFloat::pretty_print(std::string& out, const std::string& indent,
                           int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += std::to_string(value_) + "f\n";
}

} // namespace nbtcpp
