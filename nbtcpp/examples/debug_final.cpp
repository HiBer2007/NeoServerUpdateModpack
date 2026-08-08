#include "nbtcpp/region_file.h"
#include "nbtcpp/chunk.h"
#include "nbtcpp/tags/nbt_compound.h"
#include <fstream>
#include <iostream>
#include <vector>
using namespace nbtcpp;
int main() {
    // Load with the fix (save_as loads all chunks)
    RegionFile orig("K:\\nbtcpp\\r.0.5.mca");
    orig.set_preserve_timestamps(true);
    
    // Count chunks with original_data cached
    int cached = 0, total = 0;
    for (int z = 0; z < 32; z++) for (int x = 0; x < 32; x++) {
        auto* c = orig.get_chunk(x,z);
        if (!c) continue;
        total++;
        if (!c->original_data_.empty()) cached++;
        if (!c->has_unsaved_changes()) { /* ok */ }
    }
    std::cout << "Total: " << total << " Cached: " << cached << std::endl;
    
    // Save
    orig.save_as("K:\\nbtcpp\\r.0.5_roundtrip.mca");
    
    // Now verify
    RegionFile saved("K:\\nbtcpp\\r.0.5_roundtrip.mca");
    std::vector<uint8_t> orig_bytes, saved_bytes;
    for (int z = 0; z < 32; z++) for (int x = 0; x < 32; x++) {
        auto* oc = orig.get_chunk(x,z);
        auto* sc = saved.get_chunk(x,z);
        if (!oc || !sc) continue;
        oc->load(); sc->load();
        auto od = oc->data();
        auto sd = sc->data();
        if (od && sd) {
            std::string os = od->to_string();
            std::string ss = sd->to_string();
            if (os != ss) {
                std::cout << "Chunk (" << x << "," << z << ") differs!\n";
                return 1;
            }
        }
    }
    std::cout << "All " << total << " chunks structure matches\n";
    return 0;
}
