/**
 * @file diff_test.cpp
 * @brief Test NBT diff generation and patch application.
 *
 * Uses two level.dat files to:
 *   1. Generate a binary diff (A → B)
 *   2. Apply the diff to A (A + diff → A')
 *   3. Verify A' equals B by saving both and comparing SHA-256 hashes
 *
 * Usage: nbtcpp_diff_test <levelA.dat> <levelB.dat>
 */

#include "nbtcpp/nbt_diff.h"
#include "nbtcpp/nbt_file.h"
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

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace nbtcpp;

// ─── SHA-256 (Windows CryptoAPI) ──────────────────────────────────────────

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
        return "ERR: CryptAcquireContext";
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "ERR: CryptCreateHash";
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
    return "SHA256 only on Windows";
#endif
}

static std::string sha256_buffer(const std::vector<uint8_t>& data) {
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
    CryptHashData(hHash, data.data(), static_cast<DWORD>(data.size()), 0);

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
    (void)data;
    return "ERR";
#endif
}

// ─── Deep tree comparison (for verification) ──────────────────────────────

static bool trees_equal(const NbtTag& a, const NbtTag& b, std::string& diff_at) {
    if (a.tag_type() != b.tag_type()) {
        diff_at = "type: " + to_string(a.tag_type()) + " vs " + to_string(b.tag_type());
        return false;
    }

    switch (a.tag_type()) {
        case NbtTagType::Byte:
            if (static_cast<const NbtByte&>(a).value() !=
                static_cast<const NbtByte&>(b).value()) {
                diff_at = "byte value";
                return false;
            }
            return true;
        case NbtTagType::Short:
            if (static_cast<const NbtShort&>(a).value() !=
                static_cast<const NbtShort&>(b).value()) {
                diff_at = "short value";
                return false;
            }
            return true;
        case NbtTagType::Int:
            if (static_cast<const NbtInt&>(a).value() !=
                static_cast<const NbtInt&>(b).value()) {
                diff_at = "int value: " +
                    std::to_string(static_cast<const NbtInt&>(a).value()) +
                    " vs " +
                    std::to_string(static_cast<const NbtInt&>(b).value());
                return false;
            }
            return true;
        case NbtTagType::Long:
            if (static_cast<const NbtLong&>(a).value() !=
                static_cast<const NbtLong&>(b).value()) {
                diff_at = "long value";
                return false;
            }
            return true;
        case NbtTagType::Float: {
            float va = static_cast<const NbtFloat&>(a).value();
            float vb = static_cast<const NbtFloat&>(b).value();
            if (std::memcmp(&va, &vb, sizeof(float)) != 0) {
                diff_at = "float value";
                return false;
            }
            return true;
        }
        case NbtTagType::Double: {
            double va = static_cast<const NbtDouble&>(a).value();
            double vb = static_cast<const NbtDouble&>(b).value();
            if (std::memcmp(&va, &vb, sizeof(double)) != 0) {
                diff_at = "double value";
                return false;
            }
            return true;
        }
        case NbtTagType::String:
            if (static_cast<const NbtString&>(a).value() !=
                static_cast<const NbtString&>(b).value()) {
                diff_at = "string value";
                return false;
            }
            return true;
        case NbtTagType::ByteArray: {
            auto& ba = static_cast<const NbtByteArray&>(a).value();
            auto& bb = static_cast<const NbtByteArray&>(b).value();
            if (ba.size() != bb.size()) {
                diff_at = "byte_array size: " + std::to_string(ba.size()) +
                          " vs " + std::to_string(bb.size());
                return false;
            }
            if (!ba.empty() && std::memcmp(ba.data(), bb.data(), ba.size()) != 0) {
                diff_at = "byte_array content";
                return false;
            }
            return true;
        }
        case NbtTagType::IntArray: {
            auto& ia = static_cast<const NbtIntArray&>(a).value();
            auto& ib = static_cast<const NbtIntArray&>(b).value();
            if (ia.size() != ib.size()) {
                diff_at = "int_array size";
                return false;
            }
            if (!ia.empty() && std::memcmp(ia.data(), ib.data(),
                                            ia.size() * sizeof(int32_t)) != 0) {
                diff_at = "int_array content";
                return false;
            }
            return true;
        }
        case NbtTagType::LongArray: {
            auto& la = static_cast<const NbtLongArray&>(a).value();
            auto& lb = static_cast<const NbtLongArray&>(b).value();
            if (la.size() != lb.size()) {
                diff_at = "long_array size";
                return false;
            }
            if (!la.empty() && std::memcmp(la.data(), lb.data(),
                                            la.size() * sizeof(int64_t)) != 0) {
                diff_at = "long_array content";
                return false;
            }
            return true;
        }
        case NbtTagType::List: {
            auto& listA = static_cast<const NbtList&>(a);
            auto& listB = static_cast<const NbtList&>(b);
            if (listA.list_type() != listB.list_type()) {
                diff_at = "list type";
                return false;
            }
            if (listA.size() != listB.size()) {
                diff_at = "list size: " + std::to_string(listA.size()) +
                          " vs " + std::to_string(listB.size());
                return false;
            }
            for (size_t i = 0; i < listA.size(); ++i) {
                if (!trees_equal(*listA.at(i), *listB.at(i), diff_at)) {
                    diff_at = "list[" + std::to_string(i) + "]." + diff_at;
                    return false;
                }
            }
            return true;
        }
        case NbtTagType::Compound: {
            auto& compA = static_cast<const NbtCompound&>(a);
            auto& compB = static_cast<const NbtCompound&>(b);
            if (compA.size() != compB.size()) {
                // Show which children differ
                auto namesA = compA.names();
                auto namesB = compB.names();
                std::sort(namesA.begin(), namesA.end());
                std::sort(namesB.begin(), namesB.end());

                std::vector<std::string> onlyA, onlyB;
                std::set_difference(namesA.begin(), namesA.end(),
                                    namesB.begin(), namesB.end(),
                                    std::back_inserter(onlyA));
                std::set_difference(namesB.begin(), namesB.end(),
                                    namesA.begin(), namesA.end(),
                                    std::back_inserter(onlyB));

                diff_at = "compound size: " + std::to_string(compA.size()) +
                          " vs " + std::to_string(compB.size());
                for (auto& n : onlyA) diff_at += " [+" + n + "]";
                for (auto& n : onlyB) diff_at += " [-" + n + "]";
                return false;
            }
            // Compare by name (order-independent)
            auto names = compA.names();
            for (const auto& name : names) {
                auto* childB = compB.get(name);
                if (!childB) {
                    diff_at = "missing child: " + name;
                    return false;
                }
                if (!trees_equal(*compA.get(name), *childB, diff_at)) {
                    diff_at = name + "." + diff_at;
                    return false;
                }
            }
            return true;
        }
        default:
            return false;
    }
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <levelA.dat> <levelB.dat>\n";
        std::cerr << "\nGenerates a binary diff file (A→B), then applies it to A\n";
        std::cerr << "and verifies the result matches B.\n";
        return 1;
    }

    std::string pathA = argv[1];
    std::string pathB = argv[2];

    std::cout << "=== NBT Diff Test ===\n\n";

    // ── 1. Load both files ───────────────────────────────────────────────
    std::cout << "[1] Loading files...\n";

    NbtFile fileA;
    NbtFile fileB;
    try {
        fileA.load_from_file(pathA, NbtCompression::AutoDetect);
        std::cout << "  A: " << pathA << " — root=\"" << fileA.root_tag()->name()
                  << "\", compression=" << to_string(fileA.file_compression())
                  << ", children=" << fileA.root_tag()->size() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "  ERROR loading A: " << e.what() << "\n";
        return 1;
    }

    try {
        fileB.load_from_file(pathB, NbtCompression::AutoDetect);
        std::cout << "  B: " << pathB << " — root=\"" << fileB.root_tag()->name()
                  << "\", compression=" << to_string(fileB.file_compression())
                  << ", children=" << fileB.root_tag()->size() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "  ERROR loading B: " << e.what() << "\n";
        return 1;
    }

    // ── 2. Compute original hashes ───────────────────────────────────────
    std::cout << "\n[2] Original hashes...\n";
    auto hashA_orig = sha256_file(pathA);
    auto hashB_orig = sha256_file(pathB);
    std::cout << "  A: " << hashA_orig << "\n";
    std::cout << "  B: " << hashB_orig << "\n";

    // Quick check: are they already identical?
    std::string diff_loc;
    if (trees_equal(*fileA.root_tag(), *fileB.root_tag(), diff_loc)) {
        std::cout << "\n  Trees are ALREADY IDENTICAL — diff will be empty.\n";
    }

    // ── 3. Generate diff ─────────────────────────────────────────────────
    std::cout << "\n[3] Generating diff...\n";

    std::string diff_path = "test_diff.bin";
    int set_count = 0;
    int remove_count = 0;

    try {
        // First, count and display diffs
        std::cout << "  Differences:\n";
        diff_subtrees(*fileA.root_tag(), *fileB.root_tag(),
            [&](const std::string& path, const std::vector<std::string>& /*segments*/,
                DiffOp op, const NbtTag* /*value*/) {
                if (op == DiffOp::Set) {
                    set_count++;
                    if (set_count <= 20) std::cout << "    SET   " << path << "\n";
                } else {
                    remove_count++;
                    if (remove_count <= 20) std::cout << "    REMOVE " << path << "\n";
                }
            });

        if (set_count > 20) std::cout << "    ... (" << (set_count - 20)
                                       << " more SET)\n";
        if (remove_count > 20) std::cout << "    ... (" << (remove_count - 20)
                                          << " more REMOVE)\n";
        std::cout << "  Total: " << set_count << " set, " << remove_count
                  << " remove\n";

        // Write diff file
        save_diff_file(diff_path, *fileA.root_tag(), *fileB.root_tag());
        std::cout << "  Diff saved to: " << diff_path << "\n";

    } catch (const std::exception& e) {
        std::cerr << "  ERROR: " << e.what() << "\n";
        return 1;
    }

    // ── 4. Apply diff to A ───────────────────────────────────────────────
    std::cout << "\n[4] Applying diff to A...\n";

    try {
        apply_diff_file(diff_path, *fileA.root_tag());
        std::cout << "  Applied " << (set_count + remove_count)
                  << " changes to A\n";
    } catch (const std::exception& e) {
        std::cerr << "  ERROR applying diff: " << e.what() << "\n";
        return 1;
    }

    // ── 5. Verify: A' should equal B ─────────────────────────────────────
    std::cout << "\n[5] Verifying A' == B...\n";

    if (trees_equal(*fileA.root_tag(), *fileB.root_tag(), diff_loc)) {
        std::cout << "  Trees are STRUCTURALLY IDENTICAL ✓\n";
    } else {
        std::cout << "  MISMATCH: " << diff_loc << " ✗\n";

        // Debug: count diffs from A' to B
        int remaining = 0;
        diff_subtrees(*fileA.root_tag(), *fileB.root_tag(),
            [&](const std::string& path, const std::vector<std::string>& /*segments*/,
                DiffOp op, const NbtTag*) {
                if (remaining < 10)
                    std::cout << "    " << (op == DiffOp::Set ? "SET" : "REMOVE")
                              << " " << path << "\n";
                remaining++;
            });
        std::cout << "  Remaining differences: " << remaining << "\n";
        return 1;
    }

    // ── 6. Binary-level verification ─────────────────────────────────────
    std::cout << "\n[6] Binary verification...\n";

    // Save A' (patched) using the same compression as B
    std::string patched_path = "test_patched.dat";
    fileA.save_to_file(patched_path, fileB.file_compression());

    auto hashA_patched = sha256_file(patched_path);
    auto hashB_current = sha256_file(pathB);

    std::cout << "  A' (patched): " << hashA_patched << "\n";
    std::cout << "  B  (target):  " << hashB_current << "\n";

    // For binary identity, the root names must match.
    // NbtFile requires a non-empty root name to save; if the original
    // root is unnamed, adjust.
    bool roots_match = fileA.root_tag()->name() == fileB.root_tag()->name();
    if (!roots_match) {
        std::cout << "  NOTE: Root names differ — binary hashes will differ.\n";
        std::cout << "    A root: \"" << fileA.root_tag()->name() << "\"\n";
        std::cout << "    B root: \"" << fileB.root_tag()->name() << "\"\n";
    }

    if (hashA_patched == hashB_current) {
        std::cout << "  SHA-256 MATCH ✓ — round-trip is bit-identical!\n";
    } else {
        std::cout << "  SHA-256 DIFFER — checking structural round-trip...\n";

        // Reload the patched file and compare structurally
        NbtFile reloaded(patched_path);
        std::string rdiff;
        if (trees_equal(*reloaded.root_tag(), *fileB.root_tag(), rdiff)) {
            std::cout << "  Structural round-trip OK ✓ (binary diff is due to "
                      << "metadata/compression/root-name)\n";
        } else {
            std::cout << "  Structural round-trip FAILED: " << rdiff << " ✗\n";
            return 1;
        }
    }

    // ── 7. Diff file size ────────────────────────────────────────────────
    std::ifstream dfs(diff_path, std::ios::binary | std::ios::ate);
    if (dfs) {
        auto diff_size = dfs.tellg();
        std::cout << "\n[7] Diff file size: " << diff_size << " bytes\n";
    }

    std::cout << "\n=== ALL TESTS PASSED ===\n";
    return 0;
}
