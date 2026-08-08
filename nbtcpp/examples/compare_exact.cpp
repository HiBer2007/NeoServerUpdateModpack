#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
int main() {
    std::ifstream f1("K:\\nbtcpp\\r.0.5.mca", std::ios::binary);
    std::ifstream f2("K:\\nbtcpp\\r.0.5_roundtrip.mca", std::ios::binary);
    std::vector<char> d1((std::istreambuf_iterator<char>(f1)), {});
    std::vector<char> d2((std::istreambuf_iterator<char>(f2)), {});
    std::cout << "Original: " << d1.size() << " bytes\n";
    std::cout << "Saved:    " << d2.size() << " bytes\n";

    if (d1.size() != d2.size()) {
        std::cout << "Sizes differ!\n";
        return 1;
    }

    int loc_diff = 0, ts_diff = 0, data_diff = 0;
    for (int i = 0; i < 1024; i++) {
        int base = i * 4;
        if (std::memcmp(&d1[base], &d2[base], 4)) loc_diff++;
    }
    for (int i = 0; i < 1024; i++) {
        int base = 4096 + i * 4;
        if (std::memcmp(&d1[base], &d2[base], 4)) ts_diff++;
    }
    for (int i = 0; i < 1024; i++) {
        auto l1 = (unsigned char*)&d1[i*4];
        int off1 = (l1[0]<<16)|(l1[1]<<8)|l1[2], sz1 = l1[3];
        if (off1 == 0) continue;
        auto l2 = (unsigned char*)&d2[i*4];
        int off2 = (l2[0]<<16)|(l2[1]<<8)|l2[2], sz2 = l2[3];
        if (off1==off2 && sz1==sz2 && std::memcmp(&d1[off1*4096], &d2[off2*4096], sz1*4096)==0)
            continue;
        data_diff++;
    }
    std::cout << "Location diffs: " << loc_diff << "/241\n";
    std::cout << "Timestamp diffs: " << ts_diff << "/241\n";
    std::cout << "Chunk data diffs: " << data_diff << "/241\n";
    if (data_diff==0 && loc_diff==0 && ts_diff==0)
        std::cout << "\nFILES ARE BIT-IDENTICAL!\n";
    return 0;
}
