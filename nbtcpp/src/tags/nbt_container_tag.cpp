/**
 * @file nbt_container_tag.cpp
 * @brief Implementation of NbtContainerTag base class.
 */

#include "nbtcpp/tags/nbt_container_tag.h"

namespace nbtcpp {

// ─── Adopt ───────────────────────────────────────────────────────────────────

void NbtContainerTag::adopt(NbtTag* tag) {
    if (!tag) return;
    tag->set_parent(this);
    // Propagate change callback
    tag->set_change_callback(change_callback());
}

// ─── Recursive collection ────────────────────────────────────────────────────

std::vector<NbtTag*> NbtContainerTag::get_all_tags() {
    std::vector<NbtTag*> result;
    for (auto* child : *this) {
        result.push_back(child);
        if (auto* container = dynamic_cast<NbtContainerTag*>(child)) {
            auto sub = container->get_all_tags();
            result.insert(result.end(), sub.begin(), sub.end());
        }
    }
    return result;
}

std::vector<const NbtTag*> NbtContainerTag::get_all_tags() const {
    std::vector<const NbtTag*> result;
    for (auto* child : *this) {
        result.push_back(child);
        if (auto* container = dynamic_cast<const NbtContainerTag*>(child)) {
            auto sub = container->get_all_tags();
            result.insert(result.end(), sub.begin(), sub.end());
        }
    }
    return result;
}

} // namespace nbtcpp
