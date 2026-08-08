#include "nbtcpp/tags/nbt_int.h"

namespace nbtcpp {

bool NbtInt::read_tag(NbtBinaryReader& reader) {
    value_ = reader.read_int32();
    return true;
}

void NbtInt::skip_tag(NbtBinaryReader& reader) {
    reader.skip(4);
}

void NbtInt::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::Int);
    writer.write(name());
    writer.write(value_);
}

void NbtInt::write_data(NbtBinaryWriter& writer) const {
    writer.write(value_);
}

void NbtInt::pretty_print(std::string& out, const std::string& indent,
                           int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += std::to_string(value_) + "\n";
}

} // namespace nbtcpp
