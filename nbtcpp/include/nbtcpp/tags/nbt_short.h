#ifndef NBTCPP_TAGS_NBT_SHORT_H
#define NBTCPP_TAGS_NBT_SHORT_H

#include "nbtcpp/tags/nbt_tag.h"

namespace nbtcpp {

/**
 * @brief A tag containing a signed 16-bit integer (TAG_Short, ID 2).
 */
class NbtShort final : public NbtTag {
public:
    NbtShort() = default;
    explicit NbtShort(int16_t value) : value_(value) {}
    NbtShort(std::string name, int16_t value) : NbtTag(std::move(name)), value_(value) {}
    NbtShort(const NbtShort& other) : NbtTag(other), value_(other.value_) {}

    NbtTagType tag_type() const noexcept override { return NbtTagType::Short; }

    int16_t value() const noexcept { return value_; }
    void set_value(int16_t v) { value_ = v; fire_changed(); }
    int16_t short_value() const override { return value_; }

    std::unique_ptr<NbtTag> clone() const override { return std::make_unique<NbtShort>(*this); }

    bool read_tag(NbtBinaryReader& reader) override;
    void skip_tag(NbtBinaryReader& reader) override;
    void write_tag(NbtBinaryWriter& writer) const override;
    void write_data(NbtBinaryWriter& writer) const override;
    void pretty_print(std::string& out, const std::string& indent_string, int indent_level) const override;

private:
    int16_t value_ = 0;
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_SHORT_H
