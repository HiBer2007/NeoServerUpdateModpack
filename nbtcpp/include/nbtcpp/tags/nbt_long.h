#ifndef NBTCPP_TAGS_NBT_LONG_H
#define NBTCPP_TAGS_NBT_LONG_H

#include "nbtcpp/tags/nbt_tag.h"

namespace nbtcpp {

/**
 * @brief A tag containing a signed 64-bit integer (TAG_Long, ID 4).
 */
class NbtLong final : public NbtTag {
public:
    NbtLong() = default;
    explicit NbtLong(int64_t value) : value_(value) {}
    NbtLong(std::string name, int64_t value) : NbtTag(std::move(name)), value_(value) {}
    NbtLong(const NbtLong& other) : NbtTag(other), value_(other.value_) {}

    NbtTagType tag_type() const noexcept override { return NbtTagType::Long; }

    int64_t value() const noexcept { return value_; }
    void set_value(int64_t v) { value_ = v; fire_changed(); }
    int64_t long_value() const override { return value_; }

    std::unique_ptr<NbtTag> clone() const override { return std::make_unique<NbtLong>(*this); }

    bool read_tag(NbtBinaryReader& reader) override;
    void skip_tag(NbtBinaryReader& reader) override;
    void write_tag(NbtBinaryWriter& writer) const override;
    void write_data(NbtBinaryWriter& writer) const override;
    void pretty_print(std::string& out, const std::string& indent_string, int indent_level) const override;

private:
    int64_t value_ = 0;
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_LONG_H
