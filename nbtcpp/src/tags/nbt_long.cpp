#include "nbtcpp/tags/nbt_long.h"

namespace nbtcpp {

bool NbtLong::read_tag(NbtBinaryReader& reader) {
    value_ = reader.read_int64();
    return true;
}

void NbtLong::skip_tag(NbtBinaryReader& reader) {
    reader.skip(8);
}

void NbtLong::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::Long);
    writer.write(name());
    writer.write(value_);
}

void NbtLong::write_data(NbtBinaryWriter& writer) const {
    writer.write(value_);
}

void NbtLong::pretty_print(std::string& out, const std::string& indent,
                           int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += std::to_string(value_) + "L\n";
}

} // namespace nbtcpp
