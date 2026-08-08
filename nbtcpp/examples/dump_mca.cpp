/**
 * @file dump_mca.cpp
 * @brief Read a Minecraft region (.mca) file and dump all chunk data as text.
 *
 * Usage: nbtcpp_dump_mca <region.mca> [output.txt]
 *
 * If output.txt is omitted, the dump is written to
 * "<region_filename>_dump.txt".
 */

#include "nbtcpp/region_file.h"
#include "nbtcpp/chunk.h"
#include "nbtcpp/tags/nbt_compound.h"

#include <fstream>
#include <iostream>
#include <string>

using namespace nbtcpp;

static void dump_chunk(std::ofstream& out, const Chunk& chunk) {
    out << "========================================\n";
    out << "Chunk (" << chunk.x() << ", " << chunk.z() << ")\n";
    out << "========================================\n";

    if (chunk.is_corrupt()) {
        out << "[CORRUPT]\n\n";
        return;
    }
    if (chunk.is_external()) {
        out << "[EXTERNAL]\n\n";
        return;
    }

    auto data = chunk.data();
    if (!data) {
        out << "[NOT LOADED]\n\n";
        return;
    }

    out << data->to_string() << "\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <region.mca> [output.txt]\n";
        return 1;
    }

    std::string mca_path = argv[1];

    // Determine output file path
    std::string out_path;
    if (argc >= 3) {
        out_path = argv[2];
    } else {
        // Derive from input name: e.g. "r.0.5.mca" → "r.0.5_dump.txt"
        auto dot = mca_path.rfind('.');
        if (dot != std::string::npos && dot > 0) {
            out_path = mca_path.substr(0, dot) + "_dump.txt";
        } else {
            out_path = mca_path + "_dump.txt";
        }
    }

    try {
        RegionFile region(mca_path);

        auto coords = RegionFile::parse_coords(mca_path);
        std::cout << "Region file: " << mca_path << "\n";
        if (coords.x >= 0) {
            std::cout << "Region coords: r." << coords.x << "." << coords.z << ".mca\n";
        }
        std::cout << "Chunk count: " << region.chunk_count() << "\n";
        std::cout << "Output: " << out_path << "\n\n";

        // Open output file
        std::ofstream out(out_path);
        if (!out) {
            std::cerr << "Error: Cannot create output file: " << out_path << "\n";
            return 1;
        }

        // Count stats
        int total = 0, corrupt = 0, external = 0, loaded = 0;

        for (int z = 0; z < RegionFile::kChunkDimZ; ++z) {
            for (int x = 0; x < RegionFile::kChunkDimX; ++x) {
                Chunk* chunk = region.get_chunk(x, z);
                if (!chunk) continue;

                total++;
                chunk->load();  // ensure loaded

                if (chunk->is_corrupt()) {
                    corrupt++;
                } else if (chunk->is_external()) {
                    external++;
                } else {
                    loaded++;
                }

                dump_chunk(out, *chunk);
            }
        }

        out.close();

        // Print summary
        std::cout << "Summary:\n";
        std::cout << "  Total chunks: " << total << "\n";
        std::cout << "  Loaded:       " << loaded << "\n";
        std::cout << "  Corrupt:      " << corrupt << "\n";
        std::cout << "  External:     " << external << "\n";
        std::cout << "\nDone. Output written to: " << out_path << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
