#pragma once

/**
 * ROM Set Descriptor — Generic multi-file ROM loading for arcade systems
 *
 * Many arcade and vintage systems use multiple ROM chips, each containing
 * a separate binary blob that must be loaded at a specific memory address.
 * These ROMs are typically distributed as a set of files (often inside a
 * zip archive), identified by part numbers, PCB socket labels, or
 * conventional names.
 *
 * This header provides a declarative way for systems to describe their
 * ROM sets:
 *
 *   1.  RomEntryDescriptor — describes one ROM chip: filename patterns
 *       that identify it, the memory address it maps to, and its size.
 *
 *   2.  RomSetDescriptor — a named collection of RomEntryDescriptors
 *       representing one version/revision of a system's ROM set.
 *
 *   3.  rom_set_scan_and_match() — scans files at a VFS path, matches
 *       them against one or more RomSetDescriptors, and returns the
 *       best match with all resolved file paths.
 *
 *   4.  rom_set_load_matched() — given a successful match, reads each
 *       file via VFS and writes it to a caller-provided memory write
 *       callback.
 *
 * Usage pattern (in a System subclass):
 *
 *     static constexpr RomEntryDescriptor ast_v2_entries[] = { ... };
 *     static constexpr RomSetDescriptor   ast_v2_set = { ... };
 *
 *     // In probe or load:
 *     RomSetMatch match = rom_set_scan_and_match(vfs_base, &ast_v2_set, 1);
 *     if (match.matched)
 *         rom_set_load_matched(match, write_callback);
 *
 * File Organization:
 *   This file lives in src/core/ because it's a generic mechanism
 *   usable by any system, not specific to any hardware family.
 */

#include "core/vfs/vfs.hpp"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

// ============================================================================
// ROM Entry Descriptor — one ROM chip / binary blob
// ============================================================================

/**
 * Maximum number of alternative filename patterns per ROM entry.
 * Covers part-number variations, socket labels, and friendly names.
 */
inline constexpr int ROM_ENTRY_MAX_PATTERNS = 8;

/**
 * Describes one ROM chip within a ROM set.
 *
 * Example (Asteroids program ROM at $6800):
 *   { {"035143", "e_f2", "ef2"},   // match any of these substrings
 *     0x6800,                       // load address
 *     2048,                         // expected size
 *     true }                        // required
 */
struct RomEntryDescriptor {
    /// Filename patterns — case-insensitive substring match.
    /// Any file whose name contains one of these patterns is a candidate.
    /// NULL-terminated (unused slots are nullptr).
    const char* patterns[ROM_ENTRY_MAX_PATTERNS];

    /// Target memory address where this ROM's data should be written.
    uint32_t    load_address;

    /// Expected ROM size in bytes (0 = accept any size).
    uint32_t    expected_size;

    /// Whether this ROM is required for the system to boot.
    /// Optional ROMs (language PROMs, color PROMs) can be missing.
    bool        required;
};

// ============================================================================
// ROM Set Descriptor — a named collection of ROM entries
// ============================================================================

/**
 * Describes a complete ROM set version / revision.
 *
 * A system may have multiple RomSetDescriptors for different revisions,
 * region variants, or PCB revisions.  The matching code tries all of them
 * and picks the best match.
 */
struct RomSetDescriptor {
    const char*               name;         ///< Human-readable label, e.g. "Asteroids Rev 2"
    const char*               system_name;  ///< System short_name for SystemRegistry lookup
    const RomEntryDescriptor* entries;      ///< Array of ROM entries
    int                       entry_count;  ///< Number of entries in the array
};

// ============================================================================
// ROM Set Match Result
// ============================================================================

/// A single resolved ROM entry — one file matched to one descriptor entry.
struct RomEntryMatch {
    int         entry_index;    ///< Index into RomSetDescriptor::entries
    std::string vfs_path;       ///< Full VFS path to the matched file
    uint32_t    load_address;   ///< From the entry descriptor
    uint32_t    file_size;      ///< Actual file size in bytes
};

/// Result of matching files against ROM set descriptors.
struct RomSetMatch {
    bool          matched;            ///< True if all required entries were found
    float         confidence;         ///< 0.0–1.0: fraction of entries matched
    const RomSetDescriptor* rom_set;  ///< Which descriptor matched
    std::vector<RomEntryMatch> entries; ///< Resolved file paths for each matched entry
};

// ============================================================================
// Matching API
// ============================================================================

/**
 * Scan files at a VFS path and match against ROM set descriptors.
 *
 * The vfs_base_path can be:
 *   - A filesystem directory:        /home/user/asteroids_roms/
 *   - An archive file:               /home/user/asteroids_v2.zip
 *   - A VFS archive-internal path:   /home/user/collection.zip!/asteroids/
 *
 * For each file found via vfs_list_entries(), the filename is tested
 * against every entry's patterns (case-insensitive substring match).
 * If expected_size is non-zero, the file size must also match.
 *
 * The best-matching ROM set (highest confidence) is returned.  A match is
 * considered successful if all required entries are found.
 *
 * @param vfs_base_path  VFS path to scan (directory or archive)
 * @param descriptors    Array of ROM set descriptors to try
 * @param num_descriptors Number of descriptors in the array
 * @return               Best match result (check .matched for success)
 */
inline RomSetMatch rom_set_scan_and_match(
        const char* vfs_base_path,
        const RomSetDescriptor* const* descriptors,
        int num_descriptors) {
    RomSetMatch best;
    best.matched    = false;
    best.confidence = 0.0f;
    best.rom_set    = nullptr;

    if (!vfs_base_path || !descriptors || num_descriptors <= 0)
        return best;

    // List all files at the primary VFS path
    auto files = vfs_list_entries(vfs_base_path);
    if (files.empty()) return best;

    // Case-insensitive substring match helper
    auto icontains = [](const std::string& haystack, const char* needle) -> bool {
        if (!needle || !needle[0]) return false;
        size_t nlen = strlen(needle);
        if (nlen > haystack.size()) return false;
        for (size_t i = 0; i <= haystack.size() - nlen; i++) {
            bool match = true;
            for (size_t j = 0; j < nlen; j++) {
                if (tolower(static_cast<unsigned char>(haystack[i + j])) !=
                    tolower(static_cast<unsigned char>(needle[j]))) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
        return false;
    };

    // Match a single descriptor against a file list.
    // Returns {matched_entries, required_found, required_total, optional_found}.
    struct MatchResult {
        std::vector<RomEntryMatch> entries;
        int required_found = 0;
        int required_total = 0;
        int optional_found = 0;
    };

    // Track which entry indices have been matched (to avoid duplicates during
    // sibling search).
    auto match_descriptor = [&](const RomSetDescriptor* desc,
                                const std::vector<VfsEntry>& file_list,
                                const std::vector<bool>& already_matched) -> MatchResult {
        MatchResult r;
        for (int e = 0; e < desc->entry_count; e++) {
            const RomEntryDescriptor& entry = desc->entries[e];
            if (entry.required) r.required_total++;
            if (already_matched[e]) {
                if (entry.required) r.required_found++;
                else                r.optional_found++;
                continue;
            }

            bool found = false;
            for (const auto& file : file_list) {
                if (file.type == VfsEntryType::Directory) continue;
                for (int p = 0; p < ROM_ENTRY_MAX_PATTERNS && entry.patterns[p]; p++) {
                    if (icontains(file.name, entry.patterns[p])) {
                        if (entry.expected_size > 0 && file.size != 0 &&
                            file.size != entry.expected_size)
                            continue;

                        RomEntryMatch em;
                        em.entry_index  = e;
                        em.vfs_path     = file.full_path;
                        em.load_address = entry.load_address;
                        em.file_size    = static_cast<uint32_t>(file.size);
                        r.entries.push_back(std::move(em));

                        if (entry.required) r.required_found++;
                        else                r.optional_found++;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }
        return r;
    };

    // Try each descriptor
    for (int d = 0; d < num_descriptors; d++) {
        const RomSetDescriptor* desc = descriptors[d];
        if (!desc || !desc->entries || desc->entry_count <= 0) continue;

        std::vector<bool> already_matched(desc->entry_count, false);
        auto primary = match_descriptor(desc, files, already_matched);

        auto matched_entries = std::move(primary.entries);
        int required_found = primary.required_found;
        int required_total = primary.required_total;
        int optional_found = primary.optional_found;

        // ── Sibling search ──────────────────────────────────────────
        //
        // When the primary path doesn't contain all required entries,
        // scan sibling files at the parent directory level.  This lets
        // supplementary ROMs (mathbox PROMs, color PROMs, etc.) live
        // in separate archives next to the main ROM set archive.
        //
        // Example: tempest_ver1_roms.zip + tempest_mathbox_prom.zip
        //          in the same directory.
        if (required_found > 0 && required_found < required_total) {
            std::string parent = vfs_parent_path(vfs_base_path);
            if (!parent.empty()) {
                // Mark entries already found so we don't duplicate them
                for (const auto& em : matched_entries)
                    already_matched[em.entry_index] = true;

                auto siblings = vfs_list_entries(parent.c_str());

                // First pass: match loose files sitting directly in the parent folder.
                // Wrap them in a single-entry vector so match_descriptor() can test them.
                for (const auto& sibling : siblings) {
                    if (required_found >= required_total) break;
                    if (sibling.full_path == vfs_base_path) continue;
                    if (sibling.type == VfsEntryType::File) {
                        std::vector<VfsEntry> one = { sibling };
                        auto extra = match_descriptor(desc, one, already_matched);
                        for (auto& em : extra.entries) {
                            already_matched[em.entry_index] = true;
                            matched_entries.push_back(std::move(em));
                        }
                        required_found += extra.required_found;
                        optional_found += extra.optional_found;
                    }
                }

                // Second pass: recurse into sibling archives and directories.
                for (const auto& sibling : siblings) {
                    if (required_found >= required_total) break;
                    if (sibling.full_path == vfs_base_path) continue;
                    if (sibling.type == VfsEntryType::Directory ||
                        sibling.type == VfsEntryType::Archive) {
                        auto sibling_files = vfs_list_entries(sibling.full_path.c_str());
                        if (sibling_files.empty()) continue;

                        auto extra = match_descriptor(desc, sibling_files, already_matched);
                        for (auto& em : extra.entries) {
                            already_matched[em.entry_index] = true;
                            matched_entries.push_back(std::move(em));
                        }
                        required_found += extra.required_found;
                        optional_found += extra.optional_found;
                    }
                }
            }
        }

        // Calculate confidence
        bool all_required = (required_found == required_total);
        int total_entries = desc->entry_count;
        int total_found   = required_found + optional_found;
        float conf = (total_entries > 0)
            ? static_cast<float>(total_found) / static_cast<float>(total_entries)
            : 0.0f;

        // Boost confidence for complete required sets
        if (all_required && total_found > 0)
            conf = std::max(conf, 0.8f);

        if (all_required && conf > best.confidence) {
            best.matched    = true;
            best.confidence = conf;
            best.rom_set    = desc;
            best.entries    = std::move(matched_entries);
        }
    }

    return best;
}

// ============================================================================
// Loading API
// ============================================================================

/**
 * Memory write callback — called for each ROM entry to write data
 * to the correct memory location.
 *
 * @param load_address  Target address from the ROM entry descriptor
 * @param data          ROM data buffer
 * @param size          ROM data size in bytes
 * @param entry_index   Index of the entry in the ROM set descriptor
 * @return              true if write succeeded
 */
using RomWriteCallback = std::function<bool(uint32_t load_address,
                                            const uint8_t* data,
                                            size_t size,
                                            int entry_index)>;

/**
 * Load all matched ROM entries from VFS and write to memory via callback.
 *
 * Reads each matched file through the VFS (transparent archive access),
 * then calls the write callback with the data and target address.
 *
 * @param match     A successful RomSetMatch (must have .matched == true)
 * @param write_fn  Callback that writes ROM data to the system's memory
 * @return          true if all required entries were loaded successfully
 */
inline bool rom_set_load_matched(const RomSetMatch& match,
                                 const RomWriteCallback& write_fn) {
    if (!match.matched || !match.rom_set || !write_fn)
        return false;

    bool ok = true;
    for (const auto& em : match.entries) {
        size_t file_size = 0;
        uint8_t* data = vfs_read_file(em.vfs_path.c_str(), &file_size);
        if (!data) {
            // Check if this entry is required
            if (em.entry_index < match.rom_set->entry_count &&
                match.rom_set->entries[em.entry_index].required) {
                printf("ROM set: failed to read required ROM: %s\n",
                       em.vfs_path.c_str());
                ok = false;
            }
            continue;
        }

        if (!write_fn(em.load_address, data, file_size, em.entry_index)) {
            printf("ROM set: failed to write ROM at $%04X from %s\n",
                   em.load_address, em.vfs_path.c_str());
            ok = false;
        }
        free(data);
    }

    return ok;
}

// ============================================================================
// System Integration — ROM-set-aware probing
// ============================================================================

/**
 * Probe files at a VFS path against ROM set descriptors from all
 * registered systems.
 *
 * This is the top-level entry point for ROM set identification.
 * It delegates to system-provided ROM set descriptors. Systems that
 * return non-empty get_rom_set_descriptors() participate.
 *
 * Returns a SystemMatch with the best-matching system and configuration,
 * plus the RomSetMatch embedded in a custom field.
 *
 * @param vfs_base_path  Path to scan (directory, archive, or VFS path)
 * @return               Best ROM set match result
 */
struct RomSetProbeResult {
    std::string  system_name;   ///< System short_name, or empty
    float        confidence;    ///< Match confidence (0.0–1.0)
    RomSetMatch  rom_match;     ///< The detailed match result
};

RomSetProbeResult rom_set_probe(const char* vfs_base_path);
