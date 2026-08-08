#ifndef NBTCPP_TAGS_NBT_INT_ARRAY_H
#define NBTCPP_TAGS_NBT_INT_ARRAY_H

#include "nbtcpp/tags/nbt_tag.h"
#include <vector>

namespace nbtcpp {

/**
 * @brief A tag containing a length-prefixed array of 32-bit integers (TAG_Int_Array, ID 11).
 */
class NbtIntArray final : public NbtTag {
public:
    NbtIntArray() = default;
    explicit NbtIntArray(std::vector<int32_t> value) : value_(std::move(value)) {}
    NbtIntArray(std::string name, std::vector<int32_t> value)
        : NbtTag(std::move(name)), value_(std::move(value)) {}
    NbtIntArray(const NbtIntArray& other) : NbtTag(other), value_(other.value_) {}

    NbtTagType tag_type() const noexcept override { return NbtTagType::IntArray; }

    const std::vector<int32_t>& value() const noexcept { return value_; }
    std::vector<int32_t>& value() noexcept { return value_; }
    void set_value(const std::vector<int32_t>& v) { value_ = v; fire_changed(); }

    int32_t size() const noexcept { return static_cast<int32_t>(value_.size()); }

    std::unique_ptr<NbtTag> clone() const override { return std::make_unique<NbtIntArray>(*this); }

    bool read_tag(NbtBinaryReader& reader) override;
    void skip_tag(NbtBinaryReader& reader) override;
    void write_tag(NbtBinaryWriter& writer) const override;
    void write_data(NbtBinaryWriter& writer) const override;
    void pretty_print(std::string& out, const std::string& indent_string, int indent_level) const override;

private:
    std::vector<int32_t> value_;
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_INT_ARRAY_H
