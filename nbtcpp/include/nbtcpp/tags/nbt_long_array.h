#ifndef NBTCPP_TAGS_NBT_LONG_ARRAY_H
#define NBTCPP_TAGS_NBT_LONG_ARRAY_H

#include "nbtcpp/tags/nbt_tag.h"
#include <vector>

namespace nbtcpp {

/**
 * @brief A tag containing a length-prefixed array of 64-bit integers (TAG_Long_Array, ID 12).
 */
class NbtLongArray final : public NbtTag {
public:
    NbtLongArray() = default;
    explicit NbtLongArray(std::vector<int64_t> value) : value_(std::move(value)) {}
    NbtLongArray(std::string name, std::vector<int64_t> value)
        : NbtTag(std::move(name)), value_(std::move(value)) {}
    NbtLongArray(const NbtLongArray& other) : NbtTag(other), value_(other.value_) {}

    NbtTagType tag_type() const noexcept override { return NbtTagType::LongArray; }

    const std::vector<int64_t>& value() const noexcept { return value_; }
    std::vector<int64_t>& value() noexcept { return value_; }
    void set_value(const std::vector<int64_t>& v) { value_ = v; fire_changed(); }

    int32_t size() const noexcept { return static_cast<int32_t>(value_.size()); }

    std::unique_ptr<NbtTag> clone() const override { return std::make_unique<NbtLongArray>(*this); }

    bool read_tag(NbtBinaryReader& reader) override;
    void skip_tag(NbtBinaryReader& reader) override;
    void write_tag(NbtBinaryWriter& writer) const override;
    void write_data(NbtBinaryWriter& writer) const override;
    void pretty_print(std::string& out, const std::string& indent_string, int indent_level) const override;

private:
    std::vector<int64_t> value_;
};

} // namespace nbtcpp

#endif // NBTCPP_TAGS_NBT_LONG_ARRAY_H
