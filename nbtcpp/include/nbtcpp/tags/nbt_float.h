#ifndef NBTCPP_TAGS_NBT_FLOAT_H
#define NBTCPP_TAGS_NBT_FLOAT_H

#include "nbtcpp/tags/nbt_tag.h"

namespace nbtcpp {

/**
 * @brief A tag containing a 32-bit IEEE-754 single-precision float (TAG_Float, ID 5).
 */
class NbtFloat final : public NbtTag {
public:
    NbtFloat() = default;
    explicit NbtFloat(float value) : value_(value) {}
    NbtFloat(std::string name, float value) : NbtTag(std::move(name)), value_(value) {}
    NbtFloat(const NbtFloat& other) : NbtTag(other), value_(other.value_) {}

    NbtTagType tag_type() const noexcept override { return NbtTagType::Float; }

    float value() const noexcept { return value_; }
    void set_value(float v) { value_ = v; fire_changed(); }
    float float_value() const override { return value_; }

    std::unique_ptr<NbtTag> clone() const override { return std::make_unique<NbtFloat>(*this); }

    bool read_tag(NbtBinaryReader& reader) override;
    void skip_tag(NbtBinaryReader& reader) override;
    void write_tag(NbtBinaryWriter& writer) const override;
    void write_data(NbtBinaryWriter& writer) const override;
    void pretty_print(std::string& out, const std::string& indent_string, int indent_level) const override;

private:
    float value_ = 0.0f;
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_FLOAT_H
