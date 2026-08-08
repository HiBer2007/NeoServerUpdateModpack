#ifndef NBTCPP_TAGS_NBT_DOUBLE_H
#define NBTCPP_TAGS_NBT_DOUBLE_H

#include "nbtcpp/tags/nbt_tag.h"

namespace nbtcpp {

/**
 * @brief A tag containing a 64-bit IEEE-754 double-precision float (TAG_Double, ID 6).
 */
class NbtDouble final : public NbtTag {
public:
    NbtDouble() = default;
    explicit NbtDouble(double value) : value_(value) {}
    NbtDouble(std::string name, double value) : NbtTag(std::move(name)), value_(value) {}
    NbtDouble(const NbtDouble& other) : NbtTag(other), value_(other.value_) {}

    NbtTagType tag_type() const noexcept override { return NbtTagType::Double; }

    double value() const noexcept { return value_; }
    void set_value(double v) { value_ = v; fire_changed(); }
    double double_value() const override { return value_; }

    std::unique_ptr<NbtTag> clone() const override { return std::make_unique<NbtDouble>(*this); }

    bool read_tag(NbtBinaryReader& reader) override;
    void skip_tag(NbtBinaryReader& reader) override;
    void write_tag(NbtBinaryWriter& writer) const override;
    void write_data(NbtBinaryWriter& writer) const override;
    void pretty_print(std::string& out, const std::string& indent_string, int indent_level) const override;

private:
    double value_ = 0.0;
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_DOUBLE_H
