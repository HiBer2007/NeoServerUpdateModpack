/**
 * @file nbt_diff.cpp
 * @brief NBT tree diff/patch implementation.
 *
 * Key design decisions:
 *  - Dual-pointer merge over ordered std::maps for O(n) compound comparison
 *  - Lazy path construction (vector stack, join only on emit)
 *  - Streaming callback (no diff buffered in memory)
 *  - apply_diff uses path-based tree navigation
 */

#include "nbtcpp/nbt_diff.h"
#include "nbtcpp/nbt_binary_writer.h"
#include "nbtcpp/nbt_binary_reader.h"
#include "nbtcpp/nbt_exception.h"
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
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <vector>

namespace nbtcpp {

// ═══════════════════════════════════════════════════════════════════════════
//  Path stack — lazy string construction
// ═══════════════════════════════════════════════════════════════════════════

namespace {

/// A stack of path segments, built up during recursive tree walk.
/// The full dotted path is only materialised (joined) when a difference
/// is actually emitted.
class PathStack {
public:
    void push(const std::string& seg) { segments_.push_back(seg); }
    void push_list_index(int idx) {
        segments_.push_back("[" + std::to_string(idx) + "]");
    }

    void pop() { if (!segments_.empty()) segments_.pop_back(); }
    size_t size() const noexcept { return segments_.size(); }

    /// Access the raw segments (for unambiguous serialisation).
    const std::vector<std::string>& segments() const { return segments_; }

    /// Materialise the full dotted path (for display only; may be
    /// ambiguous if tag names contain dots).
    std::string join() {
        std::string result;
        for (size_t i = 0; i < segments_.size(); ++i) {
            if (i > 0 && segments_[i][0] != '[') result += '.';
            result += segments_[i];
        }
        return result;
    }

private:
    std::vector<std::string> segments_;
};

// ═══════════════════════════════════════════════════════════════════════════
//  Value comparison helpers
// ═══════════════════════════════════════════════════════════════════════════

/// Quick equality check for two tags of the same type.
/// Does NOT check names or recurse into containers.
static bool leaf_values_equal(const NbtTag& a, const NbtTag& b) {
    auto t = a.tag_type();
    if (t != b.tag_type()) return false;

    switch (t) {
        case NbtTagType::Byte:
            return static_cast<const NbtByte&>(a).value()
                == static_cast<const NbtByte&>(b).value();
        case NbtTagType::Short:
            return static_cast<const NbtShort&>(a).value()
                == static_cast<const NbtShort&>(b).value();
        case NbtTagType::Int:
            return static_cast<const NbtInt&>(a).value()
                == static_cast<const NbtInt&>(b).value();
        case NbtTagType::Long:
            return static_cast<const NbtLong&>(a).value()
                == static_cast<const NbtLong&>(b).value();
        case NbtTagType::Float: {
            auto va = static_cast<const NbtFloat&>(a).value();
            auto vb = static_cast<const NbtFloat&>(b).value();
            return std::memcmp(&va, &vb, sizeof(float)) == 0;
        }
        case NbtTagType::Double: {
            auto va = static_cast<const NbtDouble&>(a).value();
            auto vb = static_cast<const NbtDouble&>(b).value();
            return std::memcmp(&va, &vb, sizeof(double)) == 0;
        }
        case NbtTagType::String:
            return static_cast<const NbtString&>(a).value()
                == static_cast<const NbtString&>(b).value();
        case NbtTagType::ByteArray: {
            auto& ba = static_cast<const NbtByteArray&>(a).value();
            auto& bb = static_cast<const NbtByteArray&>(b).value();
            return ba.size() == bb.size()
                && (ba.empty() || std::memcmp(ba.data(), bb.data(), ba.size()) == 0);
        }
        case NbtTagType::IntArray: {
            auto& ia = static_cast<const NbtIntArray&>(a).value();
            auto& ib = static_cast<const NbtIntArray&>(b).value();
            return ia.size() == ib.size()
                && (ia.empty() || std::memcmp(ia.data(), ib.data(), ia.size() * sizeof(int32_t)) == 0);
        }
        case NbtTagType::LongArray: {
            auto& la = static_cast<const NbtLongArray&>(a).value();
            auto& lb = static_cast<const NbtLongArray&>(b).value();
            return la.size() == lb.size()
                && (la.empty() || std::memcmp(la.data(), lb.data(), la.size() * sizeof(int64_t)) == 0);
        }
        default:
            return false;  // containers — caller shouldn't call this
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Recursive diff engine
// ═══════════════════════════════════════════════════════════════════════════

static void diff_impl(const NbtTag& treeA, const NbtTag& treeB,
                      PathStack& path, DiffCallback& callback) {
    NbtTagType typeA = treeA.tag_type();
    NbtTagType typeB = treeB.tag_type();

    // ── Type mismatch → wholesale replacement ──────────────────────────
    if (typeA != typeB) {
        callback(path.join(), path.segments(), DiffOp::Set, &treeB);
        return;
    }

    // ── Compound ────────────────────────────────────────────────────────
    if (typeA == NbtTagType::Compound) {
        auto& compA = static_cast<const NbtCompound&>(treeA);
        auto& compB = static_cast<const NbtCompound&>(treeB);

        // Dual-pointer merge over ordered std::maps
        // Both iterate in alphabetical key order → single O(m) pass
        auto itA = compA.sorted_begin();
        auto itB = compB.sorted_begin();
        auto endA = compA.sorted_end();
        auto endB = compB.sorted_end();

        while (itA != endA && itB != endB) {
            const std::string& keyA = itA->first;
            const std::string& keyB = itB->first;

            if (keyA < keyB) {
                // keyA only in A → remove
                path.push(keyA);
                callback(path.join(), path.segments(), DiffOp::Remove, nullptr);
                path.pop();
                ++itA;
            } else if (keyA > keyB) {
                // keyB only in B → add (via Set on the new path)
                path.push(keyB);
                callback(path.join(), path.segments(), DiffOp::Set, itB->second.get());
                path.pop();
                ++itB;
            } else {
                // Same key in both → recurse
                path.push(keyA);
                diff_impl(*itA->second, *itB->second, path, callback);
                path.pop();
                ++itA; ++itB;
            }
        }
        // Remaining keys only in A → removes
        while (itA != endA) {
            path.push(itA->first);
            callback(path.join(), path.segments(), DiffOp::Remove, nullptr);
            path.pop();
            ++itA;
        }
        // Remaining keys only in B → adds
        while (itB != endB) {
            path.push(itB->first);
            callback(path.join(), path.segments(), DiffOp::Set, itB->second.get());
            path.pop();
            ++itB;
        }
        return;
    }

    // ── List ────────────────────────────────────────────────────────────
    if (typeA == NbtTagType::List) {
        auto& listA = static_cast<const NbtList&>(treeA);
        auto& listB = static_cast<const NbtList&>(treeB);

        // Check list element type first
        if (listA.list_type() != listB.list_type()) {
            callback(path.join(), path.segments(), DiffOp::Set, &treeB);
            return;
        }

        size_t szA = listA.size();
        size_t szB = listB.size();
        size_t common = std::min(szA, szB);

        // Compare common prefix — push [i] as NEW segment (not replace)
        for (size_t i = 0; i < common; ++i) {
            path.push_list_index(static_cast<int>(i));
            diff_impl(*listA.at(i), *listB.at(i), path, callback);
            path.pop();
        }

        // Extra elements in A → removes
        for (size_t i = common; i < szA; ++i) {
            path.push_list_index(static_cast<int>(i));
            callback(path.join(), path.segments(), DiffOp::Remove, nullptr);
            path.pop();
        }

        // Extra elements in B → adds
        for (size_t i = common; i < szB; ++i) {
            path.push_list_index(static_cast<int>(i));
            callback(path.join(), path.segments(), DiffOp::Set, listB.at(i));
            path.pop();
        }
        return;
    }

    // ── Leaf types (scalar / string / array) ────────────────────────────
    if (!leaf_values_equal(treeA, treeB)) {
        callback(path.join(), path.segments(), DiffOp::Set, &treeB);
    }
    // else: identical → no output
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════════════════════

void diff_subtrees(const NbtTag& treeA, const NbtTag& treeB,
                   DiffCallback callback) {
    PathStack path;
    diff_impl(treeA, treeB, path, callback);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Binary diff file I/O
// ═══════════════════════════════════════════════════════════════════════════

static void write_big_uint16(std::ostream& out, uint16_t v) {
    out.put(static_cast<char>((v >> 8) & 0xFF));
    out.put(static_cast<char>(v & 0xFF));
}

static void write_big_uint32(std::ostream& out, uint32_t v) {
    out.put(static_cast<char>((v >> 24) & 0xFF));
    out.put(static_cast<char>((v >> 16) & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
    out.put(static_cast<char>(v & 0xFF));
}

static uint16_t read_big_uint16(std::istream& in) {
    uint16_t v = 0;
    v |= static_cast<uint16_t>(static_cast<uint8_t>(in.get())) << 8;
    v |= static_cast<uint16_t>(static_cast<uint8_t>(in.get()));
    return v;
}

static uint32_t read_big_uint32(std::istream& in) {
    uint32_t v = 0;
    v |= static_cast<uint32_t>(static_cast<uint8_t>(in.get())) << 24;
    v |= static_cast<uint32_t>(static_cast<uint8_t>(in.get())) << 16;
    v |= static_cast<uint32_t>(static_cast<uint8_t>(in.get())) << 8;
    v |= static_cast<uint32_t>(static_cast<uint8_t>(in.get()));
    return v;
}

void save_diff_file(const std::string& filepath,
                    const NbtTag& treeA, const NbtTag& treeB) {
    std::ofstream out(filepath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot create diff file: " + filepath);
    }

    // Write header
    DiffHeader hdr;
    out.write(hdr.magic, 7);
    out.put(static_cast<char>(hdr.version));

    // Stream diffs directly to file — no in-memory buffering
    diff_subtrees(treeA, treeB, [&out](
            const std::string& /*display_path*/, const std::vector<std::string>& segments,
            DiffOp op, const NbtTag* value) {
        // op
        out.put(static_cast<char>(static_cast<uint8_t>(op)));

        // segment count (v2 format — length-prefixed segments)
        if (segments.size() > 65535) {
            throw std::runtime_error("Too many path segments in diff");
        }
        write_big_uint16(out, static_cast<uint16_t>(segments.size()));
        for (const auto& seg : segments) {
            if (seg.size() > 65535) {
                throw std::runtime_error("Path segment too long: " + seg);
            }
            write_big_uint16(out, static_cast<uint16_t>(seg.size()));
            out.write(seg.data(), static_cast<std::streamsize>(seg.size()));
        }

        // value (Set only)
        if (op == DiffOp::Set && value) {
            // Serialise the value tag using write_tag (includes type+name+data)
            std::ostringstream val_stream(std::ios::binary);
            NbtBinaryWriter writer(val_stream, true);  // big-endian
            value->write_tag(writer);
            auto val_str = val_stream.str();

            write_big_uint32(out, static_cast<uint32_t>(val_str.size()));
            out.write(val_str.data(), static_cast<std::streamsize>(val_str.size()));
        } else {
            write_big_uint32(out, 0);  // zero-length value (Remove)
        }
    });

    if (!out) {
        throw std::runtime_error("Failed writing diff file: " + filepath);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Path-based tree navigation for applying diffs
// ═══════════════════════════════════════════════════════════════════════════

namespace {

/// Parse a path segment: either a name or [N] list index.
/// Returns the name part (empty for list indices) and sets is_list_idx.
static std::string parse_segment(const std::string& path, size_t& pos,
                                  int& list_idx) {
    list_idx = -1;

    // Check for list index: [N]
    if (pos < path.size() && path[pos] == '[') {
        size_t end = path.find(']', pos);
        if (end == std::string::npos) {
            throw NbtFormatException("Malformed path (unclosed bracket): " + path);
        }
        list_idx = std::stoi(path.substr(pos + 1, end - pos - 1));
        pos = end + 1;
        if (pos < path.size() && path[pos] == '.') ++pos;  // skip dot after ]
        return {};
    }

    // Regular name segment
    size_t dot = path.find('.', pos);
    size_t bracket = path.find('[', pos);
    size_t end = std::min(dot, bracket);
    if (end == std::string::npos) end = path.size();

    std::string name = path.substr(pos, end - pos);
    pos = end;
    if (pos < path.size() && path[pos] == '.') ++pos;  // skip dot
    return name;
}

/// Navigate using pre-parsed segments (no dot ambiguity).
static NbtTag* navigate_segments(NbtTag& root,
                                  const std::vector<std::string>& segments,
                                  std::string& last_name, int& last_idx,
                                  bool is_remove) noexcept {
    last_name.clear();
    last_idx = -1;
    if (segments.empty()) return &root;

    NbtTag* current = &root;
    NbtTag* last_valid = &root;

    for (size_t si = 0; si < segments.size(); ++si) {
        const std::string& seg = segments[si];
        bool is_last = (si == segments.size() - 1);

        if (!seg.empty() && seg[0] == '[') {
            // List index
            auto* list = dynamic_cast<NbtList*>(current);
            if (!list) return is_remove ? last_valid : nullptr;
            int idx = std::stoi(seg.substr(1, seg.size() - 2));
            last_idx = idx;
            last_name.clear();
            last_valid = list;
            if (is_last) return list;
            if (idx < 0 || static_cast<size_t>(idx) >= list->size()) {
                if (!is_remove && static_cast<size_t>(idx) == list->size())
                    return list;  // append OK for SET
                return is_remove ? last_valid : nullptr;
            }
            current = list->at(static_cast<size_t>(idx));
        } else {
            // Compound child name
            auto* comp = dynamic_cast<NbtCompound*>(current);
            if (!comp) return is_remove ? last_valid : nullptr;
            last_name = seg;
            last_idx = -1;
            last_valid = comp;
            if (is_last) return comp;
            NbtTag* child = comp->get(seg);
            if (!child) return is_remove ? last_valid : nullptr;
            current = child;
        }
    }
    return current;
}

/// Navigate to the PARENT and leaf name/index of the node identified by @p path.
/// Returns the deepest valid parent. For REMOVE operations, if an intermediate
/// node doesn't exist, returns nullptr (already removed).
/// For SET operations, intermediate nodes must exist.
static NbtTag* navigate_to_parent(NbtTag& root, const std::string& path,
                                   std::string& last_name, int& last_idx,
                                   bool is_remove) noexcept {
    last_name.clear();
    last_idx = -1;
    if (path.empty()) return &root;

    NbtTag* current = &root;
    NbtTag* last_valid = &root;
    std::string last_valid_name;
    int last_valid_idx = -1;
    size_t pos = 0;

    while (pos < path.size()) {
        int list_idx = -1;
        std::string seg = parse_segment(path, pos, list_idx);

        if (list_idx >= 0) {
            auto* list = dynamic_cast<NbtList*>(current);
            if (!list) {
                return is_remove ? last_valid : nullptr;
            }
            last_name.clear();  // list indices don't have names
            last_idx = list_idx;
            last_valid = list;
            last_valid_name.clear();
            last_valid_idx = list_idx;
            if (pos >= path.size()) {
                return list;
            }
            if (list_idx < 0 || static_cast<size_t>(list_idx) >= list->size()) {
                // For SET: if index is exactly list->size(), allow append
                if (!is_remove && static_cast<size_t>(list_idx) == list->size()) {
                    // Append is OK — return list as parent
                    last_name.clear(); last_idx = list_idx;
                    return list;
                }
                return is_remove ? last_valid : nullptr;
            }
            current = list->at(static_cast<size_t>(list_idx));
        } else if (!seg.empty()) {
            auto* comp = dynamic_cast<NbtCompound*>(current);
            if (!comp) {
                return is_remove ? last_valid : nullptr;
            }
            last_name = seg;   // update for EVERY segment, not just last
            last_idx = -1;
            last_valid = comp;
            last_valid_name = seg;
            last_valid_idx = -1;
            if (pos >= path.size()) {
                return comp;
            }
            NbtTag* child = comp->get(seg);
            if (!child) {
                return is_remove ? last_valid : nullptr;
            }
            current = child;
        }
    }
    return current;
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
//  Apply diff file
// ═══════════════════════════════════════════════════════════════════════════

struct DiffEntry {
    std::string              path;       // dotted path (for display/sorting)
    std::vector<std::string> segments;   // unambiguous path segments
    DiffOp                   op;
    std::vector<uint8_t>     value_data; // raw NBT binary (write_tag format)
};

void apply_diff_file(const std::string& diff_path, NbtTag& tree) {
    std::ifstream in(diff_path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open diff file: " + diff_path);
    }

    // Read and verify header
    char magic[7];
    in.read(magic, 7);
    if (std::memcmp(magic, "NBTDIFF", 7) != 0) {
        throw std::runtime_error("Not a valid NBT diff file: " + diff_path);
    }
    int version = in.get();
    if (version != 2) {
        throw std::runtime_error("Unsupported diff file version: " +
                                 std::to_string(version));
    }

    // Read all entries (v2: length-prefixed segments)
    std::vector<DiffEntry> entries;
    while (in.peek() != std::char_traits<char>::eof()) {
        DiffEntry entry;
        entry.op = static_cast<DiffOp>(static_cast<uint8_t>(in.get()));

        uint16_t seg_count = read_big_uint16(in);
        std::vector<std::string> segments;
        segments.reserve(seg_count);
        for (uint16_t i = 0; i < seg_count; ++i) {
            uint16_t seg_len = read_big_uint16(in);
            std::string seg(seg_len, '\0');
            in.read(&seg[0], seg_len);
            segments.push_back(std::move(seg));
        }
        // Build display path from segments (for sorting/debug)
        entry.path.clear();
        for (size_t i = 0; i < segments.size(); ++i) {
            if (i > 0 && segments[i][0] != '[') entry.path += '.';
            entry.path += segments[i];
        }
        entry.segments = std::move(segments);

        uint32_t value_len = read_big_uint32(in);
        if (value_len > 0) {
            entry.value_data.resize(value_len);
            in.read(reinterpret_cast<char*>(entry.value_data.data()), value_len);
        }

        if (!in) break;
        entries.push_back(std::move(entry));
    }

    // ── Sorting strategy ──────────────────────────────────────────────────
    // To avoid list-index drift, we group operations by their parent path
    // and sort within each group so that removals happen in descending index
    // order and sets/adds happen in ascending index order.
    //
    // Sort key:
    //   {parent_path, op_priority, index_priority}
    //
    //   op_priority: Remove=0, Set=1  (removes first to make room)
    //   index_priority: for list elements, the negated index (so high→low
    //     for removes); for compound children, 0.

    auto parent_of = [](const std::string& p) -> std::string {
        // Strip last segment (including its leading dot if present)
        auto dot = p.rfind('.');
        auto brk = p.rfind('[');
        if (brk != std::string::npos && (dot == std::string::npos || brk > dot)) {
            // Last segment is a list index: "Foo.Bar[N]" → parent = "Foo.Bar"
            return p.substr(0, brk);
        } else if (dot != std::string::npos) {
            return p.substr(0, dot);
        }
        return "";  // Top-level path — parent is root
    };

    auto last_is_list = [](const std::string& p) -> bool {
        auto brk = p.rfind('[');
        if (brk == std::string::npos) return false;
        auto dot = p.rfind('.');
        return (dot == std::string::npos || brk > dot);
    };

    auto list_idx = [](const std::string& p) -> int {
        auto brk = p.rfind('[');
        if (brk == std::string::npos) return -1;
        auto end = p.find(']', brk);
        if (end == std::string::npos) return -1;
        return std::stoi(p.substr(brk + 1, end - brk - 1));
    };

    // Sort: process deepest-first for REMOVEs (children before parents),
    //        shallowest-first for SETs (parents before children).
    auto path_depth = [](const std::string& p) -> int {
        int d = 0;
        for (char c : p) if (c == '.' || c == '[') ++d;
        return d;
    };
    std::stable_sort(entries.begin(), entries.end(),
        [&](const DiffEntry& a, const DiffEntry& b) {
            // Remove before Set
            if (a.op != b.op) return a.op == DiffOp::Remove;

            int da = path_depth(a.path);
            int db = path_depth(b.path);
            if (a.op == DiffOp::Remove) {
                // Deepest removals first (child before parent)
                if (da != db) return da > db;
            } else {
                // Shallowest sets first (parent before child)
                if (da != db) return da < db;
            }

            // For list indices at same depth & op:
            if (last_is_list(a.path) && last_is_list(b.path)) {
                int ia = list_idx(a.path);
                int ib = list_idx(b.path);
                if (a.op == DiffOp::Remove) return ia > ib;
                return ia < ib;
            }
            return a.path < b.path;
        });

    // ── Deserialise helper ────────────────────────────────────────────────
    auto deserialise_value = [](const std::vector<uint8_t>& data)
            -> std::unique_ptr<NbtTag> {
        struct membuf : std::streambuf {
            membuf(const uint8_t* d, size_t len) {
                auto* p = const_cast<char*>(reinterpret_cast<const char*>(d));
                setg(p, p, p + static_cast<std::streamsize>(len));
            }
        };
        membuf sb(data.data(), data.size());
        std::istream val_stream(&sb);
        NbtBinaryReader reader(val_stream, true);
        NbtTagType val_type = reader.read_tag_type();
        auto tag = NbtCompound::create_tag(val_type);
        tag->set_name(reader.read_string());
        tag->read_tag(reader);
        return tag;
    };

    // ── Apply each entry (v2: use segments, no dot ambiguity) ────────────
    int skipped = 0, applied = 0;
    for (auto& entry : entries) {
        std::string last_name;
        int last_idx = -1;
        bool is_remove = (entry.op == DiffOp::Remove);
        NbtTag* parent = navigate_segments(tree, entry.segments,
                                           last_name, last_idx, is_remove);
        if (!parent) {
            if (skipped < 10) {
                std::cerr << "SKIP " << (entry.op == DiffOp::Remove ? "REMOVE" : "SET")
                          << " " << entry.path << " (parent not found)" << std::endl;
            }
            skipped++;
            continue;
        }
        applied++;

        if (entry.op == DiffOp::Remove) {
            if (auto* comp = dynamic_cast<NbtCompound*>(parent)) {
                if (!last_name.empty()) {
                    comp->remove(last_name);
                }
            } else if (auto* list = dynamic_cast<NbtList*>(parent)) {
                if (last_idx >= 0 && static_cast<size_t>(last_idx) < list->size()) {
                    list->remove_at(static_cast<size_t>(last_idx));
                }
            }
        } else {  // Set
            auto new_tag = deserialise_value(entry.value_data);

            if (auto* comp = dynamic_cast<NbtCompound*>(parent)) {
                if (comp->contains(new_tag->name())) {
                    comp->remove(new_tag->name());
                }
                comp->add(std::move(new_tag));
            } else if (auto* list = dynamic_cast<NbtList*>(parent)) {
                size_t sz = list->size();
                if (last_idx >= 0 && last_idx < static_cast<int>(sz)) {
                    list->remove_at(static_cast<size_t>(last_idx));
                    list->insert(static_cast<size_t>(last_idx), std::move(new_tag));
                } else if (last_idx >= 0 && last_idx == static_cast<int>(sz)) {
                    list->add(std::move(new_tag));
                } else if (last_idx >= 0) {
                    while (list->size() < static_cast<size_t>(last_idx)) {
                        list->add(NbtCompound::create_tag(list->list_type()));
                    }
                    list->add(std::move(new_tag));
                }
            }
        }
    }
    if (skipped > 0) {
        std::cerr << "  (skipped " << skipped << " unresolvable entries, "
                  << "applied " << applied << ")" << std::endl;
    }
}

} // namespace nbtcpp
