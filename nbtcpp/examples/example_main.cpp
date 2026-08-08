/**
 * @file example_main.cpp
 * @brief Usage examples for the nbtcpp library.
 *
 * Compile with:
 *   g++ -std=c++17 -I../include example_main.cpp -lnbtcpp -lz -o nbtcpp_example
 */

#include "nbtcpp/nbt_file.h"
#include "nbtcpp/nbt_reader.h"
#include "nbtcpp/nbt_writer.h"
#include "nbtcpp/tags/nbt_byte.h"
#include "nbtcpp/tags/nbt_short.h"
#include "nbtcpp/tags/nbt_int.h"
#include "nbtcpp/tags/nbt_long.h"
#include "nbtcpp/tags/nbt_float.h"
#include "nbtcpp/tags/nbt_double.h"
#include "nbtcpp/tags/nbt_string.h"
#include "nbtcpp/tags/nbt_byte_array.h"
#include "nbtcpp/tags/nbt_int_array.h"
#include "nbtcpp/tags/nbt_long_array.h"
#include "nbtcpp/tags/nbt_list.h"
#include "nbtcpp/tags/nbt_compound.h"
#include "nbtcpp/region_file.h"
#include "nbtcpp/snbt/snbt_parser.h"
#include "nbtcpp/snbt/snbt_maker.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace nbtcpp;

// ─── Example 1: Build a tag tree in memory ───────────────────────────────────
void example_build_tree() {
    std::cout << "=== Example 1: Build a tag tree ===\n";

    auto root = std::make_shared<NbtCompound>("hello_world");

    root->add(std::make_unique<NbtByte>("byte_tag", static_cast<uint8_t>(42)));
    root->add(std::make_unique<NbtShort>("short_tag", static_cast<int16_t>(12345)));
    root->add(std::make_unique<NbtInt>("int_tag", -1000000));
    root->add(std::make_unique<NbtLong>("long_tag", 9999999999LL));
    root->add(std::make_unique<NbtFloat>("float_tag", 3.14159f));
    root->add(std::make_unique<NbtDouble>("double_tag", 2.718281828459045));
    root->add(std::make_unique<NbtString>("string_tag", "Hello, NBT!"));

    // Add an array
    std::vector<uint8_t> bytes = {0, 1, 2, 3, 4, 5};
    root->add(std::make_unique<NbtByteArray>("byte_array", bytes));

    // Add a nested compound
    auto nested = std::make_unique<NbtCompound>("nested");
    nested->add(std::make_unique<NbtInt>("x", 10));
    nested->add(std::make_unique<NbtInt>("y", 20));
    nested->add(std::make_unique<NbtInt>("z", 30));
    root->add(std::move(nested));

    // Add a list
    auto list = std::make_unique<NbtList>("numbers");
    for (int32_t i = 0; i < 5; ++i) {
        list->add(std::make_unique<NbtInt>(i * 10));
    }
    root->add(std::move(list));

    std::cout << root->to_string() << "\n\n";
}

// ─── Example 2: Read/write NBT files ─────────────────────────────────────────
void example_file_io() {
    std::cout << "=== Example 2: Write and read NBT files ===\n";

    // Build a tree
    auto root = std::make_shared<NbtCompound>("Level");
    root->add(std::make_unique<NbtInt>("x", 100));
    root->add(std::make_unique<NbtInt>("y", 64));
    root->add(std::make_unique<NbtInt>("z", -200));
    root->add(std::make_unique<NbtString>("Biome", "plains"));

    // Save to file (uncompressed, big-endian)
    NbtFile file(root);
    file.set_big_endian(true);
    file.save_to_file("test.nbt", NbtCompression::None);
    std::cout << "Saved test.nbt\n";

    // Read it back
    NbtFile loaded("test.nbt");
    std::cout << "Root: " << loaded.root_tag()->name() << "\n";
    std::cout << "x = " << loaded.root_tag()->get_as<NbtInt>("x")->value() << "\n";
    std::cout << "y = " << loaded.root_tag()->get_as<NbtInt>("y")->value() << "\n";
    std::cout << "z = " << loaded.root_tag()->get_as<NbtInt>("z")->value() << "\n";
    std::cout << "Biome = " << loaded.root_tag()->get_as<NbtString>("Biome")->value() << "\n";

    // Save as ZLib compressed
    file.save_to_file("test_zlib.nbt", NbtCompression::ZLib);
    std::cout << "Saved test_zlib.nbt\n";

    // Save as GZip compressed
    file.save_to_file("test_gzip.nbt", NbtCompression::GZip);
    std::cout << "Saved test_gzip.nbt\n\n";
}

// ─── Example 3: Stream-based reading with NbtReader ─────────────────────────
void example_stream_reader() {
    std::cout << "=== Example 3: Stream-based reading ===\n";

    std::stringstream ss;
    {
        NbtWriter writer(ss, "Root", true);
        writer.begin_compound("pos");
        writer.write_int("x", 1);
        writer.write_int("y", 2);
        writer.write_int("z", 3);
        writer.end_compound();
        writer.write_byte("flag", static_cast<uint8_t>(1));
        writer.write_string("name", "test");
        // writer.finish() called implicitly by destructor
    }
    std::cout << "Wrote NBT data to stream, size=" << ss.tellp() << std::endl;

    // Read back with NbtReader
    NbtReader reader(ss, true);
    while (reader.read_to_following()) {
        std::cout << "Depth=" << reader.depth()
                  << " Type=" << to_string(reader.tag_type())
                  << " Name=" << (reader.has_name() ? reader.tag_name() : "(unnamed)")
                  << " Value=";
        if (reader.has_value()) {
            switch (reader.tag_type()) {
                case NbtTagType::Byte:
                    std::cout << static_cast<int>(reader.read_byte_value());
                    break;
                case NbtTagType::Int:
                    std::cout << reader.read_int_value();
                    break;
                case NbtTagType::String:
                    std::cout << reader.read_string_value();
                    break;
                default:
                    std::cout << "?";
                    reader.skip_value();
                    break;
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

// ─── Example 4: SNBT parsing and serialization ──────────────────────────────
void example_snbt() {
    std::cout << "=== Example 4: SNBT ===\n";

    // Parse SNBT
    std::string snbt_input = R"({
        byte_val: 42b,
        short_val: 100s,
        int_val: -1000,
        long_val: 9999999999L,
        float_val: 3.14f,
        double_val: 2.71828,
        string_val: "hello world",
        nested: {
            x: 1,
            y: 2
        },
        list_val: [1, 2, 3, 4, 5],
        byte_array: [B; 1b, 2b, 3b],
        boolean_true: true,
        boolean_false: false
    })";

    auto parsed = snbt::parse(snbt_input, false);
    std::cout << "Parsed SNBT:\n" << parsed->to_string() << "\n";

    // Serialize back to SNBT
    std::string serialized = snbt::to_snbt(*parsed, snbt::Options::default_options(), false);
    std::cout << "Serialized (minified): " << serialized << "\n\n";

    std::string pretty = snbt::to_snbt(*parsed, snbt::Options::default_expanded(), false);
    std::cout << "Serialized (expanded):\n" << pretty << "\n\n";

    // JSON-like output
    std::string json = snbt::to_snbt(*parsed, snbt::Options::json_like(), false);
    std::cout << "JSON-like: " << json << "\n\n";

    // Parse it again (round-trip)
    auto re_parsed = snbt::parse(serialized, false);
    std::cout << "Round-trip: " << (re_parsed ? "success" : "failure") << "\n";
}

// ─── Example 5: Region file support ─────────────────────────────────────────
void example_region() {
    std::cout << "=== Example 5: Region file ===\n";

    // Create an empty region in memory (not backed by a file)
    // In practice, you'd load an existing .mca file:
    //   RegionFile region("r.0.0.mca");
    std::cout << "Region file support is available.\n";
    std::cout << "To load: RegionFile region(\"r.0.0.mca\");\n";
    std::cout << "To access a chunk: Chunk* c = region.get_chunk(0, 0);\n";
    std::cout << "Region dimensions: "
              << RegionFile::kChunkDimX << "x" << RegionFile::kChunkDimZ << "\n\n";
}

// ─── Main ────────────────────────────────────────────────────────────────────
int main() {
    example_build_tree();
    example_file_io();
    example_stream_reader();
    example_snbt();
    example_region();

    std::cout << "All examples completed.\n";
    return 0;
}
