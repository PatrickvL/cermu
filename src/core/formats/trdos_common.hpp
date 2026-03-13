#pragma once

/**
 * TR-DOS Common Types and Helpers
 *
 * Shared code for SCL and TRD format handlers.  Both formats store
 * TR-DOS file descriptors with the same field layout:
 *
 *   Offset  Len  Description
 *   $00     8    Filename (space-padded)
 *   $08     1    File type: 'B'=Basic, 'C'=Code, 'D'=Data, '#'=Sequential
 *   $09     2    Param1 (LE): start address for Code, total size for Basic
 *   $0B     2    Param2 (LE): length for Code/Data, variable offset for Basic
 *   $0D     1    File length in sectors (data = sectors × 256)
 *
 * TRD adds two more bytes per entry (start sector, start track) for a
 * total of 16 bytes.  SCL omits those (14 bytes per entry).
 *
 * Both formats share the same parsed representation: trdos_file_entry_t.
 */

#include "core/formats/format_handler.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

namespace trdos {

inline constexpr size_t SECTOR_SIZE      = 256;
inline constexpr int    MAX_FILES        = 128;  // TR-DOS max catalog entries
inline constexpr size_t SCL_ENTRY_SIZE   = 14;   // SCL: 14-byte header per file
inline constexpr size_t TRD_ENTRY_SIZE   = 16;   // TRD: 16-byte catalog entry
inline constexpr size_t SECTORS_PER_TRACK = 16;

// ============================================================================
// Parsed TR-DOS File Entry
// ============================================================================

struct file_entry_t {
    char     name[9];       // 8-char filename + NUL terminator
    uint8_t  type;          // 'B', 'C', 'D', '#'
    uint16_t param1;        // Code: start address; Basic: total size incl. vars
    uint16_t param2;        // Code/Data: length; Basic: variable area offset
    uint8_t  sectors;       // File length in sectors

    // TRD-only (set to 0 for SCL entries)
    uint8_t  start_sector;
    uint8_t  start_track;   // Logical track (DS: 0=C0H0, 1=C0H1, 2=C1H0…)
};

// ============================================================================
// Parsing Helpers
// ============================================================================

/// Parse a single TR-DOS file entry from raw bytes.
/// @param p            Pointer to the raw entry (14 or 16 bytes)
/// @param entry_size   14 for SCL, 16 for TRD
/// @param out          Parsed entry
inline void parse_entry(const uint8_t* p, size_t entry_size, file_entry_t& out) {
    std::memcpy(out.name, p, 8);
    out.name[8] = '\0';
    // Trim trailing spaces
    for (int j = 7; j >= 0 && out.name[j] == ' '; --j)
        out.name[j] = '\0';

    out.type    = p[8];
    out.param1  = format_read_le16(p + 9);
    out.param2  = format_read_le16(p + 11);
    out.sectors = p[13];

    if (entry_size >= TRD_ENTRY_SIZE) {
        out.start_sector = p[14];
        out.start_track  = p[15];
    } else {
        out.start_sector = 0;
        out.start_track  = 0;
    }
}

/// Is this entry deleted? (first byte of filename == 0x01)
inline bool is_deleted(const uint8_t* raw_entry) {
    return raw_entry[0] == 0x01;
}

/// Is this an empty/end-of-catalog entry? (first byte == 0x00)
inline bool is_end_of_catalog(const uint8_t* raw_entry) {
    return raw_entry[0] == 0x00;
}

/// Get the actual data length in bytes for a file entry.
/// For Code/Data files, param2 holds the exact byte length.
/// For Basic files, param1 holds total size (incl. variables),
/// but the sector count is the canonical on-disk size.
inline size_t file_data_length(const file_entry_t& e) {
    // The byte-accurate length field depends on type:
    //   'C' (Code):  param2 = length
    //   'D' (Data):  param2 = length
    //   'B' (Basic): param1 = total size including vars (minus trailing line bytes)
    //   '#' (Sequential): param2 = length
    // In all cases, clamp to sectors × 256.
    size_t sector_data = static_cast<size_t>(e.sectors) * SECTOR_SIZE;
    size_t byte_len;
    if (e.type == 'B')
        byte_len = e.param1;
    else
        byte_len = e.param2;
    return (byte_len > 0 && byte_len <= sector_data) ? byte_len : sector_data;
}

/// Get the load address for a file entry.
/// Only meaningful for Code ('C') files.
inline uint16_t load_address(const file_entry_t& e) {
    return (e.type == 'C') ? e.param1 : 0;
}

// ============================================================================
// Display Extensions
// ============================================================================

/// Map TR-DOS type byte to a human-readable display extension.
inline const char* type_extension(uint8_t type) {
    switch (type) {
        case 'B': return ".bas";
        case 'C': return ".cod";
        case 'D': return ".dat";
        case '#': return ".seq";
        default:  return ".bin";
    }
}

// ============================================================================
// Common Load Logic
// ============================================================================

/// Populate a format_load_result_t from a parsed entry + data pointer.
/// Shared by both SCL and TRD load callbacks.
/// @return true on success
inline bool load_best_entry(const file_entry_t* entries, int count,
                            const uint8_t* const* file_data_ptrs,
                            const size_t* file_data_sizes,
                            const char* format_name,
                            format_load_result_t* out) {
    if (count <= 0) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "%s: Invalid or empty container", format_name);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Strategy: first Code ('C') entry, fall back to Basic ('B'), then any.
    int best = -1;
    int basic_idx = -1;

    for (int i = 0; i < count; ++i) {
        if (entries[i].type == 'C' && entries[i].sectors > 0) {
            best = i;
            break;
        }
        if (entries[i].type == 'B' && entries[i].sectors > 0 && basic_idx < 0)
            basic_idx = i;
    }
    if (best < 0) best = (basic_idx >= 0) ? basic_idx : 0;

    const file_entry_t& e = entries[best];
    size_t payload_size = file_data_length(e);

    if (payload_size > file_data_sizes[best]) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "%s: File data extends beyond container", format_name);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    out->program.data = static_cast<uint8_t*>(std::malloc(payload_size));
    if (!out->program.data) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "%s: out of memory", format_name);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    std::memcpy(out->program.data, file_data_ptrs[best], payload_size);
    out->program.data_size = payload_size;
    out->program.load_addr = load_address(e);
    out->program.end_addr  = static_cast<uint16_t>(out->program.load_addr + payload_size);

    // Store the entry in metadata for the system's load handler
    static_assert(sizeof(file_entry_t) <= FORMAT_METADATA_MAX_SIZE,
                  "trdos::file_entry_t must fit in metadata blob");
    std::memcpy(out->metadata, &e, sizeof(e));
    out->metadata_size = sizeof(e);

    out->type = FORMAT_LOAD_PROGRAM;

    printf("%s: Loading '%s' (type=%c, addr=$%04X, len=%zu, sectors=%u)\n",
           format_name, e.name, e.type ? e.type : '?',
           out->program.load_addr,
           payload_size, e.sectors);
    return true;
}

/// Populate container directory entries from parsed file entries.
/// Shared by both SCL and TRD list_entries callbacks.
inline int list_entries_common(const file_entry_t* entries, int count,
                               format_container_entry_t* out_entries,
                               int max_entries) {
    int out_count = 0;
    for (int i = 0; i < count && out_count < max_entries; ++i) {
        format_container_entry_t& ce = out_entries[out_count];
        snprintf(ce.display_name, sizeof(ce.display_name), "%s%s",
                 entries[i].name, type_extension(entries[i].type));
        ce.size  = file_data_length(entries[i]);
        ce.index = i;
        out_count++;
    }
    return out_count;
}

}  // namespace trdos
