/**
 * SSD/DSD Format Handler — Implementation
 *
 * Parses Acorn DFS disc images (.ssd single-sided, .dsd double-sided).
 * Identifies by file size (multiples of 2560 = 10 sectors × 256 bytes)
 * and valid DFS catalog structure.
 *
 * Uses disk_image_common.hpp for shared loading logic.
 */

#include "core/formats/ssd_format.hpp"
#include "core/formats/disk_image_common.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t DFS_SECTOR_SIZE    = 256;
static constexpr size_t DFS_SECTORS_PER_TRACK = 10;
static constexpr size_t DFS_TRACK_SIZE     = DFS_SECTORS_PER_TRACK * DFS_SECTOR_SIZE;  // 2560
static constexpr int    DFS_MAX_FILES      = 31;   // DFS catalog max
static constexpr size_t DFS_CATALOG_SIZE   = 2 * DFS_SECTOR_SIZE;  // sectors 0-1

// Standard DFS disc sizes
static constexpr size_t SSD_40T = 40 * DFS_TRACK_SIZE;   // 100 KB
static constexpr size_t SSD_80T = 80 * DFS_TRACK_SIZE;   // 200 KB
static constexpr size_t DSD_40T = 2 * SSD_40T;           // 200 KB
static constexpr size_t DSD_80T = 2 * SSD_80T;           // 400 KB

// ============================================================================
// Internal: DFS Catalog Parsing
// ============================================================================

struct dfs_parsed_t {
    disk_image::file_entry_t entries[DFS_MAX_FILES];
    const uint8_t*           data_ptrs[DFS_MAX_FILES];
    size_t                   data_sizes[DFS_MAX_FILES];
    int                      count;
};

/// Is this a valid DFS disc size?
static bool is_valid_dfs_size(size_t size) {
    return size == SSD_40T || size == SSD_80T ||
           size == DSD_40T || size == DSD_80T;
}

/// Is this a double-sided disc?
static bool is_double_sided(size_t size) {
    return size == DSD_40T || size == DSD_80T;
}

/// Parse the DFS catalog from sector 0 and sector 1 of a side.
/// @param cat      Pointer to 512 bytes of catalog data (sectors 0+1)
/// @param img_data Pointer to the full disc image (for resolving file data)
/// @param img_size Size of the full disc image
/// @param dsd      True if DSD (interleaved), false if SSD
static bool dfs_parse_catalog(const uint8_t* cat, const uint8_t* img_data,
                              size_t img_size, bool dsd, dfs_parsed_t& out) {
    out.count = 0;

    // Number of catalog entries: sector 1 offset $05 / 8
    int num_entries = cat[0x105] / 8;
    if (num_entries > DFS_MAX_FILES) num_entries = DFS_MAX_FILES;
    if (num_entries <= 0) return true;  // empty disc (valid)

    for (int i = 0; i < num_entries; ++i) {
        disk_image::file_entry_t& e = out.entries[out.count];

        // Sector 0: filename at offset $008 + i*8
        size_t s0_off = 0x008 + static_cast<size_t>(i) * 8;
        // Sector 1: metadata at offset $108 + i*8
        size_t s1_off = 0x108 + static_cast<size_t>(i) * 8;

        // Filename: 7 chars + directory char (high bit of byte 7 = locked flag)
        char dir_char = static_cast<char>(cat[s0_off + 7] & 0x7F);
        bool locked = (cat[s0_off + 7] & 0x80) != 0;

        // Build filename: "D.NAME" format
        char fname[8];
        std::memcpy(fname, cat + s0_off, 7);
        fname[7] = '\0';
        // Trim trailing spaces
        for (int j = 6; j >= 0 && fname[j] == ' '; --j)
            fname[j] = '\0';

        if (dir_char != '$' && dir_char != ' ' && dir_char != '\0') {
            snprintf(e.name, sizeof(e.name), "%c.%s", dir_char, fname);
        } else {
            snprintf(e.name, sizeof(e.name), "%s", fname);
        }

        e.locked = locked;

        // Metadata from sector 1
        uint16_t load_lo = format_read_le16(cat + s1_off + 0);
        uint16_t exec_lo = format_read_le16(cat + s1_off + 2);
        uint16_t len_lo  = format_read_le16(cat + s1_off + 4);
        uint8_t  extra   = cat[s1_off + 6];
        uint8_t  start_sector = cat[s1_off + 7];

        // Extra byte packs high bits:
        //   bits 1-0: bits 17-16 of exec address
        //   bits 3-2: bits 17-16 of length
        //   bits 5-4: bits 17-16 of load address
        //   bits 7-6: bits 9-8 of start sector
        uint32_t load_addr = load_lo | (static_cast<uint32_t>(extra & 0x30) << 12);
        uint32_t exec_addr = exec_lo | (static_cast<uint32_t>(extra & 0x03) << 16);
        uint32_t file_len  = len_lo  | (static_cast<uint32_t>(extra & 0x0C) << 14);
        uint16_t start_sec = start_sector | (static_cast<uint16_t>(extra & 0xC0) << 2);

        e.load_addr = load_addr;
        e.exec_addr = exec_addr;
        e.size      = file_len;

        // Classify file type from load/exec addresses
        // BBC convention: BASIC programs load at $1900 (PAGE), execute at $8023 (BASIC entry)
        if (exec_addr == 0x008023 || exec_addr == 0x801F || exec_addr == 0x00001900)
            e.type = disk_image::DISK_FILE_BASIC;
        else if (exec_addr != 0 && exec_addr != 0xFFFFFFFF)
            e.type = disk_image::DISK_FILE_CODE;
        else
            e.type = disk_image::DISK_FILE_DATA;

        // Locate file data in the disc image
        // SSD: sectors are sequential (sector N = offset N*256)
        // DSD: tracks interleave sides (track T side S = offset (T*2 + S) * 2560)
        size_t data_offset;
        if (dsd) {
            // DSD interleaving: sector N maps to track T = N/10, sector S = N%10
            // But all side-0 data is on even track offsets
            // DSD layout: T0S0, T0S1, T1S0, T1S1, ...
            // Catalog is side 0, so sectors are on side 0
            uint16_t track = start_sec / DFS_SECTORS_PER_TRACK;
            uint16_t sect  = start_sec % DFS_SECTORS_PER_TRACK;
            data_offset = (static_cast<size_t>(track) * 2) * DFS_TRACK_SIZE
                        + static_cast<size_t>(sect) * DFS_SECTOR_SIZE;
        } else {
            data_offset = static_cast<size_t>(start_sec) * DFS_SECTOR_SIZE;
        }

        if (data_offset < img_size && file_len > 0) {
            out.data_ptrs[out.count] = img_data + data_offset;
            size_t avail = img_size - data_offset;
            out.data_sizes[out.count] = (file_len <= avail) ? file_len : avail;
        } else {
            out.data_ptrs[out.count] = nullptr;
            out.data_sizes[out.count] = 0;
        }

        out.count++;
    }

    return true;
}

// ============================================================================
// Identification
// ============================================================================

static float ssd_identify(const uint8_t* data, size_t file_size,
                          const char* extension) {
    float confidence = 0.0f;

    // Extension check
    if (extension) {
        if (format_ext_match(extension, ".ssd")) confidence = 0.8f;
        else if (format_ext_match(extension, ".dsd")) confidence = 0.8f;
    }

    // Content validation: valid DFS size + plausible catalog
    if (data && is_valid_dfs_size(file_size)) {
        // Check that catalog entry count is plausible
        int num_entries = data[0x105] / 8;
        if (num_entries >= 0 && num_entries <= DFS_MAX_FILES) {
            // Additional sanity: sector count should be non-zero
            uint16_t total_sectors = (static_cast<uint16_t>(data[0x107] & 0x03) << 8)
                                   | data[0x106];
            if (total_sectors > 0 && total_sectors <= 800) {
                confidence = (confidence > 0.7f) ? 0.92f : 0.6f;
            }
        }
    }

    return confidence;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool ssd_load(const uint8_t* data, size_t size,
                     format_load_result_t* out) {
    if (!data || size < DFS_CATALOG_SIZE) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "SSD/DSD: File too small for DFS catalog");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    bool dsd = is_double_sided(size);
    dfs_parsed_t parsed{};

    // Parse catalog from side 0 (sectors 0-1 are at the start)
    if (!dfs_parse_catalog(data, data, size, dsd, parsed)) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "SSD/DSD: Failed to parse DFS catalog");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    return disk_image::load_best_entry(parsed.entries, parsed.count,
                                       parsed.data_ptrs, parsed.data_sizes,
                                       dsd ? "DSD" : "SSD", out);
}

// ============================================================================
// List Entries
// ============================================================================

static int ssd_list(const uint8_t* data, size_t size,
                    format_container_entry_t* entries, int max_entries) {
    if (!data || size < DFS_CATALOG_SIZE) return -1;

    bool dsd = is_double_sided(size);
    dfs_parsed_t parsed{};

    if (!dfs_parse_catalog(data, data, size, dsd, parsed))
        return -1;

    return disk_image::list_entries_common(parsed.entries, parsed.count,
                                           entries, max_entries);
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* ssd_extensions[] = { ".ssd", ".dsd", nullptr };

const format_descriptor_t SSD_FORMAT_DESCRIPTOR = {
    "SSD/DSD",
    "Acorn DFS Disc Image",
    ssd_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER | FORMAT_CAP_METADATA,
    ssd_identify,
    ssd_load,
    ssd_list,
    nullptr     // extract_entry
};

REGISTER_FORMAT(SSD, &SSD_FORMAT_DESCRIPTOR)
