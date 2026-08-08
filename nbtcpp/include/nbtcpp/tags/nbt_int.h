#ifndef NBTCPP_TAGS_NBT_INT_H
#define NBTCPP_TAGS_NBT_INT_H

#include "nbtcpp/tags/nbt_tag.h"

namespace nbtcpp {

/**
 * @brief A tag containing a signed 32-bit integer (TAG_Int, ID 3).
 */
class NbtInt final : public NbtTag {
public:
    NbtInt() = default;
    explicit NbtInt(int32_t value) : value_(value) {}
    NbtInt(std::string name, int32_t value) : NbtTag(std::move(name)), value_(value) {}
    NbtInt(const NbtInt& other) : NbtTag(other), value_(other.value_) {}

    NbtTagType tag_type() const noexcept override { return NbtTagType::Int; }

    int32_t value() const noexcept { return value_; }
    void set_value(int32_t v) { value_ = v; fire_changed(); }
    int32_t int_value() const override { return value_; }

    std::unique_ptr<NbtTag> clone() const override { return std::make_unique<NbtInt>(*this); }

    bool read_tag(NbtBinaryReader& reader) override;
    void skip_tag(NbtBinaryReader& reader) override;
    void write_tag(NbtBinaryWriter& writer) const override;
    void write_data(NbtBinaryWriter& writer) const override;
    void pretty_print(std::string& out, const std::string& indent_string, int indent_level) const override;

private:
    int32_t value_ = 0;
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_INT_H
