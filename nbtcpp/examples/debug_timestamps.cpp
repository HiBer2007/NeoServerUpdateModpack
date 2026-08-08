#include "nbtcpp/region_file.h"
#include <fstream>
#include <iostream>
#include <iomanip>
using namespace nbtcpp;
int main() {
    // Read headers directly to compare
    auto read_mca = [](const std::string& path, uint8_t* loc, uint8_t* ts) {
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char*>(loc), 4096);
        f.read(reinterpret_cast<char*>(ts), 4096);
    };
    uint8_t loc1[4096], ts1[4096], loc2[4096], ts2[4096];
    read_mca("K:\\nbtcpp\\r.0.5.mca", loc1, ts1);
    read_mca("K:\\nbtcpp\\r.0.5_roundtrip.mca", loc2, ts2);

    int diff_loc = 0, diff_ts = 0, diff_chunk = 0;
    for (int z = 0; z < 32; z++) for (int x = 0; x < 32; x++) {
        int i = (x + z * 32) * 4;
        if (memcmp(loc1+i, loc2+i, 4)) diff_loc++;
        if (memcmp(ts1+i, ts2+i, 4)) diff_ts++;
    }
    std::cout << "Location table differences: " << diff_loc << "/1024\n";
    std::cout << "Timestamp table differences: " << diff_ts << "/1024\n";
    std::cout << "\nFirst 5 timestamps from original:\n";
    for (int i = 0; i < 5; i++) {
        uint32_t t = (ts1[i*4]<<24)|(ts1[i*4+1]<<16)|(ts1[i*4+2]<<8)|ts1[i*4+3];
        std::cout << "  chunk " << i << ": " << t << "\n";
    }
    std::cout << "\nFirst 5 timestamps from saved:\n";
    for (int i = 0; i < 5; i++) {
        uint32_t t = (ts2[i*4]<<24)|(ts2[i*4+1]<<16)|(ts2[i*4+2]<<8)|ts2[i*4+3];
        std::cout << "  chunk " << i << ": " << t << "\n";
    }
    return 0;
}
