#include "nbtcpp/tags/nbt_double.h"

namespace nbtcpp {

bool NbtDouble::read_tag(NbtBinaryReader& reader) {
    value_ = reader.read_double();
    return true;
}

void NbtDouble::skip_tag(NbtBinaryReader& reader) {
    reader.skip(8);
}

void NbtDouble::write_tag(NbtBinaryWriter& writer) const {
    writer.write(NbtTagType::Double);
    writer.write(name());
    writer.write(value_);
}

void NbtDouble::write_data(NbtBinaryWriter& writer) const {
    writer.write(value_);
}

void NbtDouble::pretty_print(std::string& out, const std::string& indent,
                           int level) const {
    for (int i = 0; i < level; ++i) out += indent;
    if (!name().empty()) out += name() + ": ";
    out += std::to_string(value_) + "d\n";
}

} // namespace nbtcpp
