#ifndef NBTCPP_NBT_DIFF_H
#define NBTCPP_NBT_DIFF_H

/**
 * @file nbt_diff.h
 * @brief NBT tree diff/patch: generate minimal diffs between two NBT trees
 *        and apply diffs to reconstruct the target tree.
 *
 * Handles unordered Compound children by matching by name (not position).
 * Uses a streaming callback-based approach: diffs are emitted as they are
 * found, keeping memory usage proportional to tree depth, not tree size.
 */

#include "nbtcpp/tags/nbt_tag.h"
#include "nbtcpp/tags/nbt_compound.h"
#include "nbtcpp/tags/nbt_list.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nbtcpp {

// ─── Diff operation types ─────────────────────────────────────────────────

/// Operation type for a single diff entry.
enum class DiffOp : uint8_t {
    Set    = 0,  ///< Replace value/subtree at path (or add if path is new)
    Remove = 1   ///< Remove the node at path
};

// ─── Callback type ────────────────────────────────────────────────────────

/**
 * @brief Callback invoked for each difference found during tree comparison.
 *
 * @param path   Dotted path to the changed node, e.g. "Level.xPos"
 *               or "Level.Sections[3].BlockStates".
 * @param op     The operation: Set (replace value) or Remove (delete node).
 * @param value  For Set: pointer to the NEW value in tree B.
 *               For Remove: nullptr.
 *               The pointer is valid only during the callback; do not store it.
 */
using DiffCallback = std::function<void(
    const std::string& path,
    const std::vector<std::string>& segments,
    DiffOp op,
    const NbtTag* value)>;

// ─── Diff generation ──────────────────────────────────────────────────────

/**
 * @brief Streamingly compare two NBT subtrees and invoke @p callback for
 *        every difference found.
 *
 * Compounds are compared by NAME (not position), so different child orderings
 * do NOT produce spurious diffs.  Lists are compared by position.
 *
 * @param treeA    Source tree (the "old" version).
 * @param treeB    Target tree (the "new" version).
 * @param callback Invoked once per detected difference.  Paths are relative
 *                 to the subtree root (do not include the root name).
 *
 * Memory: O(depth) stack frames + O(depth) path segments.
 * Time:   O(N) where N = total number of tags in both trees.
 *         Compound children are compared via a single O(m) merge pass
 *         over the two ordered std::maps.
 */
void diff_subtrees(const NbtTag& treeA, const NbtTag& treeB,
                   DiffCallback callback);

// ─── Binary diff file I/O ─────────────────────────────────────────────────

/// Magic bytes for the binary diff format: "NBTDIFF" + version byte.
/// Version 2: path stored as length-prefixed segments (handles dots in names).
struct DiffHeader {
    char    magic[7] = {'N','B','T','D','I','F','F'};
    uint8_t version  = 2;
};

/**
 * @brief Generate a diff between two trees and write it directly to a
 *        compact binary file (streaming — does not buffer in memory).
 *
 * File format (v2):
 *   Header (8 bytes): magic "NBTDIFF" + version(2)
 *   For each entry:
 *     op:        1 byte  (0=Set, 1=Remove)
 *     seg_count: 2 bytes (uint16, big-endian)
 *     For each segment:
 *       seg_len: 2 bytes (uint16, big-endian)
 *       seg_data: seg_len bytes (UTF-8, no escaping needed)
 *     [if op==Set]:
 *       value_len: 4 bytes (uint32, big-endian)
 *       value:     value_len bytes (NBT write_tag format)
 *
 * @param filepath  Output diff file path.
 * @param treeA     Old tree.
 * @param treeB     New tree.
 */
void save_diff_file(const std::string& filepath,
                    const NbtTag& treeA, const NbtTag& treeB);

/**
 * @brief Apply a binary diff file to @p tree, modifying it in-place so that
 *        it becomes the target tree.
 *
 * Diff entries are sorted by path length before application so that parent
 * paths are processed before child paths (important when the parent itself
 * is being replaced).
 *
 * @param diff_path  Path to the diff file (created by save_diff_file).
 * @param tree       The tree to patch IN-PLACE.
 * @throws std::runtime_error on I/O or format errors.
 */
void apply_diff_file(const std::string& diff_path, NbtTag& tree);

} // namespace nbtcpp

#endif // NBTCPP_NBT_DIFF_H
