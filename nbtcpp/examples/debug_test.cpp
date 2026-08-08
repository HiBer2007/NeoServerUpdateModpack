#include "nbtcpp/nbt_file.h"
#include "nbtcpp/tags/nbt_compound.h"
#include <fstream>
#include <iostream>
using namespace nbtcpp;
int main() {
    NbtFile f("K:\\nbtcpp\\level.dat");
    auto root = f.root_tag();
    std::cout << "orig names:";
    for (auto& n : root->names()) std::cout << " \"" << n << "\"";
    std::cout << "\n";
    std::cout << "orig size: " << root->size() << "\n";
    f.save_to_file("K:\\nbtcpp\\level_test_round.nbt", NbtCompression::None);
    NbtFile f2("K:\\nbtcpp\\level_test_round.nbt");
    auto r2 = f2.root_tag();
    std::cout << "new names:";
    for (auto& n : r2->names()) std::cout << " \"" << n << "\"";
    std::cout << "\n";
    std::cout << "new size: " << r2->size() << "\n";
    return 0;
}
