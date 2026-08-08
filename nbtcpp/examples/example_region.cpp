/**
 * @file example_region.cpp
 * @brief Example of reading a Minecraft region (.mca) file.
 *
 * This example shows how to iterate over chunks in a region file
 * and access their NBT data.
 */

#include "nbtcpp/region_file.h"
#include "nbtcpp/chunk.h"
#include "nbtcpp/tags/nbt_compound.h"

#include <iostream>
#include <string>

using namespace nbtcpp;

void print_chunk_info(const Chunk& chunk) {
    std::cout << "  Chunk (" << chunk.x() << ", " << chunk.z() << "): ";
    if (chunk.is_corrupt()) {
        std::cout << "[CORRUPT]";
    } else if (chunk.is_external()) {
        std::cout << "[EXTERNAL]";
    } else if (!chunk.is_loaded()) {
        std::cout << "[NOT LOADED]";
    } else {
        std::cout << "loaded, " << (chunk.has_unsaved_changes() ? "dirty" : "clean");
        // Print top-level keys
        auto data = chunk.data();
        if (data) {
            std::cout << ", keys: {";
            auto names = data->names();
            for (size_t i = 0; i < names.size() && i < 8; ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << names[i];
            }
            if (names.size() > 8) std::cout << ", ...";
            std::cout << "}";
        }
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <region.mca>\n";
        return 1;
    }

    std::string path = argv[1];

    try {
        RegionFile region(path);

        auto coords = RegionFile::parse_coords(path);
        std::cout << "Region file: " << path << "\n";
        std::cout << "Region coords: r." << coords.x << "." << coords.z << ".mca\n";
        std::cout << "Chunk count: " << region.chunk_count() << "\n";

        int loaded = 0;
        int corrupt = 0;
        int external = 0;

        for (int z = 0; z < RegionFile::kChunkDimZ; ++z) {
            for (int x = 0; x < RegionFile::kChunkDimX; ++x) {
                const Chunk* chunk = region.get_chunk(x, z);
                if (!chunk) continue;

                if (chunk->is_corrupt()) {
                    corrupt++;
                } else if (chunk->is_external()) {
                    external++;
                } else {
                    loaded++;
                }
            }
        }

        std::cout << "Loaded: " << loaded << "\n";
        std::cout << "Corrupt: " << corrupt << "\n";
        std::cout << "External: " << external << "\n";

        // Print first few chunks
        std::cout << "\nFirst chunks:\n";
        int count = 0;
        for (int z = 0; z < RegionFile::kChunkDimZ && count < 5; ++z) {
            for (int x = 0; x < RegionFile::kChunkDimX && count < 5; ++x) {
                Chunk* chunk = region.get_chunk(x, z);
                if (chunk) {
                    chunk->load();  // ensure loaded
                    print_chunk_info(*chunk);
                    count++;
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
