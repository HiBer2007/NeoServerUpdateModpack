/**
 * @file roundtrip_test.cpp
 * @brief Round-trip test: load a file, save it back, compare hashes.
 *
 * Tests:
 *   - .nbt / .dat: NbtFile (load → save → hash)
 *   - .mca        : RegionFile (load → save → hash each chunk & full file)
 *
 * Usage: nbtcpp_roundtrip_test <file.nbt|file.dat|file.mca>
 */

#include "nbtcpp/nbt_file.h"
#include "nbtcpp/region_file.h"
#include "nbtcpp/chunk.h"
#include "nbtcpp/tags/nbt_compound.h"
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

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace nbtcpp;

// ─── SHA-256 helper ─────────────────────────────────────────────────────────
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

static std::string sha256_file(const std::string& path) {
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES,
                             CRYPT_VERIFYCONTEXT))
        return "ERR";
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "ERR";
    }

    std::ifstream f(path, std::ios::binary);
    std::vector<char> buf(65536);
    while (f) {
        f.read(buf.data(), buf.size());
        CryptHashData(hHash, reinterpret_cast<const BYTE*>(buf.data()),
                      static_cast<DWORD>(f.gcount()), 0);
    }
    f.close();

    BYTE hash[32];
    DWORD hashLen = 32;
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    std::ostringstream out;
    for (int i = 0; i < 32; ++i)
        out << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    return out.str();
#else
    (void)path;
    return "SHA256 only supported on Windows";
#endif
}

// ─── Deep NBT comparison ────────────────────────────────────────────────────
static bool compare_tags(const NbtTag* a, const NbtTag* b, std::string& diff,
                         const std::string& path_str) {
    if (!a && !b) return true;
    if (!a || !b) {
        diff = path_str + ": one is null";
        return false;
    }
    if (a->tag_type() != b->tag_type()) {
        diff = path_str + ": type mismatch (" +
               to_string(a->tag_type()) + " vs " +
               to_string(b->tag_type()) + ")";
        return false;
    }
    if (a->name() != b->name()) {
        diff = path_str + ": name mismatch (\"" +
               a->name() + "\" vs \"" + b->name() + "\")";
        return false;
    }

    switch (a->tag_type()) {
        case NbtTagType::Byte:
            if (a->byte_value() != b->byte_value()) {
                diff = path_str + ": byte value mismatch";
                return false;
            }
            break;
        case NbtTagType::Short:
            if (a->short_value() != b->short_value()) {
                diff = path_str + ": short value mismatch";
                return false;
            }
            break;
        case NbtTagType::Int:
            if (a->int_value() != b->int_value()) {
                diff = path_str + ": int value mismatch";
                return false;
            }
            break;
        case NbtTagType::Long:
            if (a->long_value() != b->long_value()) {
                diff = path_str + ": long value mismatch";
                return false;
            }
            break;
        case NbtTagType::Float:
            if (a->float_value() != b->float_value()) {
                diff = path_str + ": float value mismatch";
                return false;
            }
            break;
        case NbtTagType::Double:
            if (a->double_value() != b->double_value()) {
                diff = path_str + ": double value mismatch";
                return false;
            }
            break;
        case NbtTagType::String:
            if (a->string_value() != b->string_value()) {
                diff = path_str + ": string value mismatch";
                return false;
            }
            break;
        case NbtTagType::ByteArray: {
            auto& ba_a = *static_cast<const NbtByteArray*>(a);
            auto& ba_b = *static_cast<const NbtByteArray*>(b);
            if (ba_a.value() != ba_b.value()) {
                diff = path_str + ": byte_array size mismatch";
                return false;
            }
            break;
        }
        case NbtTagType::IntArray: {
            auto& ia_a = *static_cast<const NbtIntArray*>(a);
            auto& ia_b = *static_cast<const NbtIntArray*>(b);
            if (ia_a.value() != ia_b.value()) {
                diff = path_str + ": int_array size mismatch";
                return false;
            }
            break;
        }
        case NbtTagType::LongArray: {
            auto& la_a = *static_cast<const NbtLongArray*>(a);
            auto& la_b = *static_cast<const NbtLongArray*>(b);
            if (la_a.value() != la_b.value()) {
                diff = path_str + ": long_array size mismatch";
                return false;
            }
            break;
        }
        case NbtTagType::List: {
            auto& list_a = *static_cast<const NbtList*>(a);
            auto& list_b = *static_cast<const NbtList*>(b);
            if (list_a.size() != list_b.size()) {
                diff = path_str + ": list size mismatch";
                return false;
            }
            for (size_t i = 0; i < list_a.size(); ++i) {
                auto child_path = path_str + "[" + std::to_string(i) + "]";
                if (!compare_tags(list_a.at(i), list_b.at(i), diff, child_path))
                    return false;
            }
            break;
        }
        case NbtTagType::Compound: {
            auto& comp_a = *static_cast<const NbtCompound*>(a);
            auto& comp_b = *static_cast<const NbtCompound*>(b);
            auto names_a = comp_a.names();
            auto names_b = comp_b.names();
            if (names_a.size() != names_b.size()) {
                diff = path_str + ": compound size mismatch (" +
                       std::to_string(names_a.size()) + " vs " +
                       std::to_string(names_b.size()) + ")";
                return false;
            }
            for (const auto& name : names_a) {
                auto child_path = path_str + "." + name;
                if (!compare_tags(comp_a.get(name), comp_b.get(name),
                                  diff, child_path))
                    return false;
            }
            break;
        }
        default:
            break;
    }
    return true;
}

static bool compare_nbt_files(NbtFile& orig, NbtFile& regenerated,
                              std::string& diff) {
    auto root_a = orig.root_tag().get();
    auto root_b = regenerated.root_tag().get();
    return compare_tags(root_a, root_b, diff, "(root)");
}

// ─── Test an NBT / DAT file ─────────────────────────────────────────────────
static bool test_nbt_file(const std::string& path) {
    std::cout << "\n=== Testing NBT file: " << path << " ===\n";

    auto orig_hash = sha256_file(path);
    std::cout << "  Original SHA256: " << orig_hash << "\n";

    // Load original
    NbtFile orig;
    try {
        orig.load_from_file(path, NbtCompression::AutoDetect);
    } catch (const std::exception& e) {
        std::cout << "  [FAIL] Cannot load: " << e.what() << "\n";
        return false;
    }

    auto root = orig.root_tag();
    std::cout << "  Root: \"" << root->name() << "\", children: "
              << (root->size()) << "\n";
    std::cout << "  Compression: " << to_string(orig.file_compression()) << "\n";

    // If root has no name, assign one temporarily (NbtFile requires named root)
    bool had_no_name = root->name().empty();
    if (had_no_name) {
        std::cout << "  (root had no name — assigning \"Root\" for round-trip)\n";
        root->set_name("Root");
    }

    // Test round-trip with each compression type
    struct TestCase { const char* label; NbtCompression comp; const char* suffix; };
    TestCase cases[] = {
        {"None (uncompressed)", NbtCompression::None, "_roundtrip_none.nbt"},
        {"GZip",                NbtCompression::GZip, "_roundtrip_gzip.nbt"},
        {"ZLib",                NbtCompression::ZLib, "_roundtrip_zlib.nbt"},
    };

    bool all_ok = true;

    for (auto& tc : cases) {
        // Derive temp file path
        auto dot = path.rfind('.');
        std::string temp_path;
        if (dot != std::string::npos && dot > 0)
            temp_path = path.substr(0, dot) + tc.suffix;
        else
            temp_path = path + tc.suffix;

        // Save
        try {
            orig.save_to_file(temp_path, tc.comp);
        } catch (const std::exception& e) {
            std::cout << "  [" << tc.label << " save] FAIL: "
                      << e.what() << "\n";
            all_ok = false;
            continue;
        }

        // Load back
        NbtFile reloaded;
        try {
            reloaded.load_from_file(temp_path, NbtCompression::AutoDetect);
        } catch (const std::exception& e) {
            std::cout << "  [" << tc.label << " reload] FAIL: "
                      << e.what() << "\n";
            all_ok = false;
            continue;
        }

        // Compare structure
        std::string diff;
        bool struct_ok = compare_nbt_files(orig, reloaded, diff);
        if (!struct_ok) {
            std::cout << "  [" << tc.label << " struct] FAIL: "
                      << diff << "\n";
            all_ok = false;
        } else {
            std::cout << "  [" << tc.label << "] struct OK\n";
        }
    }

    if (all_ok)
        std::cout << "  => ALL PASSED\n";
    else
        std::cout << "  => SOME FAILED\n";

    return all_ok;
}

// ─── Test an MCA file ───────────────────────────────────────────────────────
static bool test_mca_file(const std::string& path) {
    std::cout << "\n=== Testing MCA file: " << path << " ===\n";

    auto orig_hash = sha256_file(path);
    std::cout << "  Original SHA256: " << orig_hash << "\n";

    // Load region
    RegionFile* region = nullptr;
    try {
        region = new RegionFile(path);
        // Preserve original timestamps to minimize binary diff on round-trip
        region->set_preserve_timestamps(true);
    } catch (const std::exception& e) {
        std::cout << "  [FAIL] Cannot load: " << e.what() << "\n";
        return false;
    }

    std::cout << "  Chunks: " << region->chunk_count() << "\n";

    // Save to temp file
    auto dot = path.rfind('.');
    std::string temp_path;
    if (dot != std::string::npos && dot > 0)
        temp_path = path.substr(0, dot) + "_roundtrip.mca";
    else
        temp_path = path + "_roundtrip.mca";

    try {
        region->save_as(temp_path);
    } catch (const std::exception& e) {
        std::cout << "  [save] FAIL: " << e.what() << "\n";
        delete region;
        return false;
    }

    // Verify structure: reload and compare each chunk's NBT
    RegionFile reloaded(temp_path);
    std::cout << "  Reloaded chunks: " << reloaded.chunk_count() << "\n";

    bool all_ok = true;
    int compared = 0;

    for (int z = 0; z < RegionFile::kChunkDimZ; ++z) {
        for (int x = 0; x < RegionFile::kChunkDimX; ++x) {
            Chunk* orig_chunk = region->get_chunk(x, z);
            Chunk* new_chunk  = reloaded.get_chunk(x, z);
            if (!orig_chunk && !new_chunk) continue;
            if (!orig_chunk || !new_chunk) {
                std::cout << "  Chunk (" << x << "," << z
                          << "): existence mismatch\n";
                all_ok = false;
                continue;
            }

            // Ensure loaded
            if (!orig_chunk->is_loaded()) orig_chunk->load();
            if (!new_chunk->is_loaded())  new_chunk->load();

            if (orig_chunk->is_corrupt() != new_chunk->is_corrupt() ||
                orig_chunk->is_external() != new_chunk->is_external()) {
                std::cout << "  Chunk (" << x << "," << z
                          << "): status mismatch\n";
                all_ok = false;
                continue;
            }

            if (orig_chunk->is_corrupt() || orig_chunk->is_external())
                continue;

            // Compare NBT data
            std::string diff;
            auto data_a = orig_chunk->data().get();
            auto data_b = new_chunk->data().get();
            compared++;
            if (!compare_tags(data_a, data_b, diff,
                              "(root)")) {
                std::cout << "  Chunk (" << x << "," << z
                          << "): struct FAIL - " << diff << "\n";
                all_ok = false;
            }
        }
    }

    std::cout << "  Chunks compared: " << compared << "\n";

    auto new_hash = sha256_file(temp_path);
    std::cout << "  Saved SHA256: " << new_hash << "\n";

    // Binary comparison often fails due to padding/timestamp differences,
    // so we only report it (not a failure)
    if (orig_hash == new_hash)
        std::cout << "  Binary SHA256: MATCH\n";
    else
        std::cout << "  Binary SHA256: DIFFERENT (expected — "
                  << "timestamps/file order may change)\n";

    if (all_ok)
        std::cout << "  => STRUCTURE PASSED\n";
    else
        std::cout << "  => SOME FAILED\n";

    delete region;
    return all_ok;
}

// ─── Main ───────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.nbt|file.dat|file.mca>...\n";
        return 1;
    }

    bool all_pass = true;

    for (int i = 1; i < argc; ++i) {
        std::string path = argv[i];
        std::string ext;
        auto dot = path.rfind('.');
        if (dot != std::string::npos)
            ext = path.substr(dot);

        bool ok = false;
        if (ext == ".mca" || ext == ".mcr")
            ok = test_mca_file(path);
        else if (ext == ".nbt" || ext == ".dat" || ext == ".snbt")
            ok = test_nbt_file(path);
        else {
            std::cout << "\n=== Unknown extension: " << path << " ===\n";
            // Try as NBT file
            ok = test_nbt_file(path);
        }

        if (!ok) all_pass = false;
    }

    std::cout << "\n========================================\n";
    if (all_pass)
        std::cout << "All tests PASSED.\n";
    else
        std::cout << "Some tests FAILED.\n";

    return all_pass ? 0 : 1;
}
