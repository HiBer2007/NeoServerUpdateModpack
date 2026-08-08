#include "nbtcpp/region_file.h"
#include "nbtcpp/chunk.h"
#include <iostream>
int main() {
    nbtcpp::RegionFile region("K:\\nbtcpp\\r.0.5.mca");
    region.set_preserve_timestamps(true);
    region.save_as("K:\\nbtcpp\\r.0.5_roundtrip.mca");
    std::cout << "Saved with timestamps preserved" << std::endl;
    nbtcpp::RegionFile reloaded("K:\\nbtcpp\\r.0.5_roundtrip.mca");
    std::cout << "Original: " << region.chunk_count() << " chunks" << std::endl;
    std::cout << "Reloaded: " << reloaded.chunk_count() << " chunks" << std::endl;
    return 0;
}
