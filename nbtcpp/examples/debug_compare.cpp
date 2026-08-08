#include "nbtcpp/region_file.h"
#include "nbtcpp/chunk.h"
#include <iostream>
int main() {
    // Load original and save with preserved timestamps
    RegionFile region("K:\\nbtcpp\\r.0.5.mca");
    region.set_preserve_timestamps(true);
    region.save_as("K:\\nbtcpp\\r.0.5_roundtrip.mca");
    std::cout << "Saved with preserved timestamps\n";

    // Load back and compare
    RegionFile reloaded("K:\\nbtcpp\\r.0.5_roundtrip.mca");
    std::cout << "Original chunks: " << region.chunk_count() << "\n";
    std::cout << "Reloaded chunks: " << reloaded.chunk_count() << "\n";
    return 0;
}
