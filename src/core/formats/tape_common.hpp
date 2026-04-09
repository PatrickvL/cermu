#pragma once

/**
 * Tape Common Types and Helpers
 *
 * Shared abstraction for cassette tape formats that store loadable programs
 * as sequential blocks.  Used by: Oric TAP, VZ, MSX CAS, UEF, KC85 K7.
 *
 * Each tape format parses its container into an array of tape::block_t
 * entries plus raw data pointers, then calls the shared load/list helpers
 * to produce a format_load_result_t.
 *
 * Mirrors the design of trdos_common.hpp for TR-DOS disk containers.
 */

#include "core/cermu.hpp"
#include "core/formats/format_handler.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace tape {

// ============================================================================
// Constants
// ============================================================================

inline constexpr int MAX_BLOCKS = 64;   // Max blocks per tape image

// ============================================================================
// Block Type Classification
// ============================================================================

enum block_type_t : uint8_t {
    TAPE_BLOCK_BASIC   = 0,     // BASIC program
    TAPE_BLOCK_CODE    = 1,     // Machine code / binary data
    TAPE_BLOCK_DATA    = 2,     // Data file
    TAPE_BLOCK_UNKNOWN = 0xFF
};

// ============================================================================
// Parsed Tape Block
// ============================================================================

struct block_t {
    char         name[33];      // Filename (NUL-terminated)
    block_type_t type;          // BASIC, code, data
    uint16_t     load_addr;     // Destination address in system memory
    uint16_t     exec_addr;     // Execution address (0 = none / not applicable)
    bool         autorun;       // Auto-execute after load
};

// ============================================================================
// Block Selection & Loading
// ============================================================================

/// Select the best block and populate a format_load_result_t.
/// Prefers CODE blocks, falls back to BASIC, then any block with data.
/// Stores the selected block_t in the result's metadata blob.
/// @return true on success
inline bool load_best_entry(const block_t* blocks, int count,
                            const uint8_t* const* data_ptrs,
                            const size_t* data_sizes,
                            const char* format_name,
                            format_load_result_t* out) {
    if (count <= 0) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "%s: No loadable blocks found", format_name);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Strategy: first CODE block, fall back to BASIC, then first with data
    int best = -1;
    int basic_idx = -1;

    for (int i = 0; i < count; ++i) {
        if (blocks[i].type == TAPE_BLOCK_CODE && data_sizes[i] > 0) {
            best = i;
            break;
        }
        if (blocks[i].type == TAPE_BLOCK_BASIC && data_sizes[i] > 0 && basic_idx < 0)
            basic_idx = i;
    }
    if (best < 0) best = (basic_idx >= 0) ? basic_idx : 0;

    const block_t& b = blocks[best];
    size_t payload_size = data_sizes[best];

    if (payload_size == 0) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "%s: Selected block '%s' has no data", format_name, b.name);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    out->program.data = static_cast<uint8_t*>(std::malloc(payload_size));
    if (!out->program.data) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "%s: Out of memory", format_name);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    std::memcpy(out->program.data, data_ptrs[best], payload_size);
    out->program.data_size = payload_size;
    out->program.load_addr = b.load_addr;
    out->program.end_addr  = static_cast<uint16_t>(b.load_addr + payload_size);

    // Store the block descriptor in metadata for the system's load handler
    static_assert(sizeof(block_t) <= FORMAT_METADATA_MAX_SIZE,
                  "tape::block_t must fit in metadata blob");
    std::memcpy(out->metadata, &b, sizeof(b));
    out->metadata_size = sizeof(b);

    out->type = FORMAT_LOAD_PROGRAM;

    log_info("%s: Loading '%s' (type=%d, addr=$%04X, exec=$%04X, len=%zu)\n",
             format_name, b.name, b.type,
             b.load_addr, b.exec_addr, payload_size);
    return true;
}

// ============================================================================
// Container Directory Listing
// ============================================================================

/// Populate container directory entries from parsed tape blocks.
inline int list_entries_common(const block_t* blocks, int count,
                               const size_t* data_sizes,
                               format_container_entry_t* out_entries,
                               int max_entries) {
    int out_count = 0;
    for (int i = 0; i < count && out_count < max_entries; ++i) {
        format_container_entry_t& ce = out_entries[out_count];
        snprintf(ce.display_name, sizeof(ce.display_name), "%s", blocks[i].name);
        ce.size  = data_sizes[i];
        ce.index = i;
        out_count++;
    }
    return out_count;
}

/// Map tape block type to a human-readable extension.
inline const char* type_extension(block_type_t type) {
    switch (type) {
        case TAPE_BLOCK_BASIC:   return ".bas";
        case TAPE_BLOCK_CODE:    return ".bin";
        case TAPE_BLOCK_DATA:    return ".dat";
        default:                 return ".blk";
    }
}

}  // namespace tape
