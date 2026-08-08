/**
 * @file dump_nbt.cpp
 * @brief Read any NBT file (uncompressed, GZip, ZLib) and dump as text.
 *
 * Usage: nbtcpp_dump_nbt <file.nbt|file.dat> [output.txt]
 *
 * Supports GZip, ZLib, and uncompressed NBT files.
 * For .dat files (like Minecraft level.dat), auto-detection works.
 */

#include "nbtcpp/nbt_file.h"
#include "nbtcpp/tags/nbt_compound.h"

#include <fstream>
#include <iostream>
#include <string>

using namespace nbtcpp;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.nbt|file.dat> [output.txt]\n";
        return 1;
    }

    std::string in_path = argv[1];

    // Derive output path
    std::string out_path;
    if (argc >= 3) {
        out_path = argv[2];
    } else {
        auto dot = in_path.rfind('.');
        if (dot != std::string::npos && dot > 0) {
            out_path = in_path.substr(0, dot) + "_dump.txt";
        } else {
            out_path = in_path + "_dump.txt";
        }
    }

    try {
        std::cout << "Loading: " << in_path << "\n";

        NbtFile nbt(in_path);

        auto root = nbt.root_tag();
        if (!root) {
            std::cerr << "Error: No root tag found.\n";
            return 1;
        }

        std::cout << "Root tag: " << root->name() << "\n";
        std::cout << "Compression: " << to_string(nbt.file_compression()) << "\n";
        std::cout << "Big-endian: " << (nbt.is_big_endian() ? "true" : "false") << "\n";
        std::cout << "Children: " << root->size() << "\n";
        std::cout << "Output: " << out_path << "\n\n";

        std::ofstream out(out_path);
        if (!out) {
            std::cerr << "Error: Cannot create output file: " << out_path << "\n";
            return 1;
        }

        out << root->to_string() << "\n";
        out.close();

        std::cout << "Done. Output written to: " << out_path << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";

        // Try loading with explicit settings as fallback
        if (argc <= 2) {
            auto dot = in_path.rfind('.');
            std::string fallback_path;
            if (dot != std::string::npos && dot > 0) {
                fallback_path = in_path.substr(0, dot) + "_dump.txt";
            } else {
                fallback_path = in_path + "_dump.txt";
            }

            for (auto comp : { NbtCompression::GZip, NbtCompression::ZLib, NbtCompression::None }) {
                try {
                    NbtFile fallback;
                    std::cout << "  Retrying with " << to_string(comp) << "...\n";
                    fallback.load_from_file(in_path, comp);
                    std::ofstream out(fallback_path);
                    if (out) {
                        out << fallback.root_tag()->to_string() << "\n";
                        std::cout << "  Success with " << to_string(comp) << "!\n";
                        std::cout << "Output: " << fallback_path << "\n";
                    }
                    return 0;
                } catch (...) {
                    // continue to next
                }
            }
        }

        return 1;
    }

    return 0;
}
