#pragma once

/**
 * Disk Image Common Types and Helpers
 *
 * Shared abstraction for disk image formats that store files with directory
 * metadata.  Used by: Acorn DFS (SSD/DSD), CPCEMU DSK, Apple II DSK.
 *
 * Each disk format parses its container into an array of disk_image::file_entry_t
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

namespace disk_image {

// ============================================================================
// Constants
// ============================================================================

inline constexpr int MAX_FILES = 128;   // Max files per disk image

// ============================================================================
// File Type Classification
// ============================================================================

enum file_type_t : uint8_t {
    DISK_FILE_BASIC    = 0,     // BASIC program
    DISK_FILE_CODE     = 1,     // Machine code / binary
    DISK_FILE_DATA     = 2,     // Data file
    DISK_FILE_TEXT     = 3,     // Text / ASCII
    DISK_FILE_UNKNOWN  = 0xFF
};

// ============================================================================
// Parsed File Entry
// ============================================================================

struct file_entry_t {
    char         name[33];      // Filename (NUL-terminated)
    file_type_t  type;          // BASIC, code, data, text
    uint32_t     load_addr;     // Load address (destination in system memory)
    uint32_t     exec_addr;     // Execution address
    uint32_t     size;          // File size in bytes
    bool         locked;        // File is locked/read-only
};

// ============================================================================
// File Selection & Loading
// ============================================================================

/// Select the best file and populate a format_load_result_t.
/// Prefers CODE files, falls back to BASIC, then any file with data.
/// Stores the selected file_entry_t in the result's metadata blob.
/// @return true on success
inline bool load_best_entry(const file_entry_t* entries, int count,
                            const uint8_t* const* data_ptrs,
                            const size_t* data_sizes,
                            const char* format_name,
                            format_load_result_t* out) {
    if (count <= 0) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "%s: No files found on disk image", format_name);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Strategy: first CODE file, fall back to BASIC, then first with data
    int best = -1;
    int basic_idx = -1;

    for (int i = 0; i < count; ++i) {
        if (entries[i].type == DISK_FILE_CODE && data_sizes[i] > 0) {
            best = i;
            break;
        }
        if (entries[i].type == DISK_FILE_BASIC && data_sizes[i] > 0 && basic_idx < 0)
            basic_idx = i;
    }
    if (best < 0) best = (basic_idx >= 0) ? basic_idx : 0;

    const file_entry_t& e = entries[best];
    size_t payload_size = data_sizes[best];

    if (payload_size == 0) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "%s: Selected file '%s' has no data", format_name, e.name);
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
    out->program.load_addr = static_cast<uint16_t>(e.load_addr & 0xFFFF);
    out->program.end_addr  = static_cast<uint16_t>((e.load_addr + payload_size) & 0xFFFF);

    // Store the file entry in metadata for the system's load handler
    static_assert(sizeof(file_entry_t) <= FORMAT_METADATA_MAX_SIZE,
                  "disk_image::file_entry_t must fit in metadata blob");
    std::memcpy(out->metadata, &e, sizeof(e));
    out->metadata_size = sizeof(e);

    out->type = FORMAT_LOAD_PROGRAM;

    log_info("%s: Loading '%s' (type=%d, addr=$%04X, exec=$%04X, len=%zu)\n",
             format_name, e.name, e.type,
             e.load_addr, e.exec_addr, payload_size);
    return true;
}

// ============================================================================
// Container Directory Listing
// ============================================================================

/// Populate container directory entries from parsed disk files.
inline int list_entries_common(const file_entry_t* entries, int count,
                               format_container_entry_t* out_entries,
                               int max_entries) {
    int out_count = 0;
    for (int i = 0; i < count && out_count < max_entries; ++i) {
        format_container_entry_t& ce = out_entries[out_count];
        snprintf(ce.display_name, sizeof(ce.display_name), "%s%s",
                 entries[i].name,
                 entries[i].locked ? " (L)" : "");
        ce.size  = entries[i].size;
        ce.index = i;
        out_count++;
    }
    return out_count;
}

}  // namespace disk_image
