/**
 * TRD Format Handler — Implementation
 *
 * Parses ZX Spectrum .trd raw disk images (TR-DOS filesystem).
 * Reads the catalog from track 0 (sectors 0-8) and the disk info
 * sector (sector 9) to enumerate files and extract their data.
 *
 * Uses the shared trdos_common.hpp types for parsing, loading,
 * and container browsing.
 */

#include "core/formats/trd_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t TRD_SECTOR_SIZE    = trdos::SECTOR_SIZE;  // 256
static constexpr size_t TRD_SECTORS_PER_TRACK = trdos::SECTORS_PER_TRACK; // 16
static constexpr size_t TRD_TRACK_SIZE     = TRD_SECTORS_PER_TRACK * TRD_SECTOR_SIZE; // 4096

// Catalog occupies sectors 0-7 of track 0 (first 8 sectors = 2048 bytes)
static constexpr size_t TRD_CATALOG_SECTORS = 8;
static constexpr size_t TRD_CATALOG_SIZE   = TRD_CATALOG_SECTORS * TRD_SECTOR_SIZE; // 2048

// Disk info is at sector 9 of track 0 (offset 0x0900 = 2304)
static constexpr size_t TRD_DISKINFO_OFFSET = 9 * TRD_SECTOR_SIZE;

// Disk info field offsets (within the 256-byte sector)
static constexpr size_t DI_FIRST_FREE_SEC  = 0xE1;
static constexpr size_t DI_DISK_TYPE       = 0xE3;
static constexpr size_t DI_FILE_COUNT      = 0xE4;
static constexpr size_t DI_FREE_SECTORS    = 0xE5;
static constexpr size_t DI_TRDOS_ID        = 0xE7;
static constexpr size_t DI_LABEL            = 0xF5;

// Known disk sizes (bytes)
static constexpr size_t TRD_SIZE_DS80 = 2 * 80 * TRD_TRACK_SIZE;  // 655360
static constexpr size_t TRD_SIZE_DS40 = 2 * 40 * TRD_TRACK_SIZE;  // 327680
static constexpr size_t TRD_SIZE_SS80 = 1 * 80 * TRD_TRACK_SIZE;  // 327680
static constexpr size_t TRD_SIZE_SS40 = 1 * 40 * TRD_TRACK_SIZE;  // 163840

// ============================================================================
// Internal: Resolve logical track + sector to byte offset
// ============================================================================

/// Convert a logical track number and sector to a byte offset in the image.
/// Logical track numbering for double-sided: 0=H0T0, 1=H1T0, 2=H0T1, ...
static size_t trd_offset(uint8_t track, uint8_t sector) {
    return static_cast<size_t>(track) * TRD_TRACK_SIZE
         + static_cast<size_t>(sector) * TRD_SECTOR_SIZE;
}

// ============================================================================
// Internal: Parse catalog
// ============================================================================

struct trd_parsed_t {
    trdos::file_entry_t entries[trdos::MAX_FILES];
    const uint8_t*      data_ptrs[trdos::MAX_FILES];
    size_t              data_sizes[trdos::MAX_FILES];
    int                 count;
};

/// Parse the TRD catalog and resolve data pointers for each file.
static bool trd_parse(const uint8_t* data, size_t size, trd_parsed_t& out) {
    out.count = 0;
    if (size < TRD_DISKINFO_OFFSET + TRD_SECTOR_SIZE) return false;

    // Validate TR-DOS ID byte
    const uint8_t* disk_info = data + TRD_DISKINFO_OFFSET;
    if (disk_info[DI_TRDOS_ID] != 0x10) return false;

    int catalog_entries = trdos::MAX_FILES;
    int count = 0;

    for (int i = 0; i < catalog_entries; ++i) {
        const uint8_t* raw = data + i * trdos::TRD_ENTRY_SIZE;

        // End of catalog
        if (trdos::is_end_of_catalog(raw)) break;

        // Skip deleted entries
        if (trdos::is_deleted(raw)) continue;

        trdos::file_entry_t& e = out.entries[count];
        trdos::parse_entry(raw, trdos::TRD_ENTRY_SIZE, e);

        // Resolve data location from the entry's start track/sector
        size_t file_offset = trd_offset(e.start_track, e.start_sector);
        size_t sector_data = static_cast<size_t>(e.sectors) * TRD_SECTOR_SIZE;

        if (file_offset + sector_data > size) {
            // File extends beyond image — truncate or skip
            if (file_offset >= size) continue;
            sector_data = size - file_offset;
        }

        out.data_ptrs[count]  = data + file_offset;
        out.data_sizes[count] = sector_data;
        count++;
    }

    out.count = count;
    return count > 0;
}

// ============================================================================
// Format Identification
// ============================================================================

static float trd_identify(const uint8_t* data, size_t file_size,
                           const char* extension) {
    bool ext_match = extension && format_ext_match(extension, ".trd");

    // Must be large enough for at least the disk info sector
    if (file_size < TRD_DISKINFO_OFFSET + TRD_SECTOR_SIZE) {
        return ext_match ? 0.2f : 0.0f;
    }

    // Check TR-DOS ID byte at disk info sector offset 0xE7
    const uint8_t* disk_info = data + TRD_DISKINFO_OFFSET;
    bool has_trdos_id = (disk_info[DI_TRDOS_ID] == 0x10);

    // Check disk type byte is in valid range
    uint8_t disk_type = disk_info[DI_DISK_TYPE];
    bool valid_type = (disk_type >= 0x16 && disk_type <= 0x19);

    // Check common image sizes
    bool valid_size = (file_size == TRD_SIZE_DS80 || file_size == TRD_SIZE_DS40 ||
                       file_size == TRD_SIZE_SS80 || file_size == TRD_SIZE_SS40);

    if (has_trdos_id && valid_type) {
        if (ext_match && valid_size) return 0.95f;
        if (ext_match) return 0.90f;
        if (valid_size) return 0.80f;
        return 0.70f;
    }

    // TR-DOS ID present but type byte unusual
    if (has_trdos_id) {
        return ext_match ? 0.7f : 0.4f;
    }

    if (ext_match && valid_size) return 0.5f;
    if (ext_match) return 0.3f;
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool trd_load(const uint8_t* data, size_t size,
                     format_load_result_t* out) {
    trd_parsed_t parsed;
    if (!trd_parse(data, size, parsed)) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "TRD: Invalid disk image or empty catalog");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    return trdos::load_best_entry(parsed.entries, parsed.count,
                                  parsed.data_ptrs, parsed.data_sizes,
                                  "TRD", out);
}

// ============================================================================
// Container: List Entries
// ============================================================================

static int trd_list_entries(const uint8_t* data, size_t size,
                            format_container_entry_t* out_entries,
                            int max_entries) {
    trd_parsed_t parsed;
    if (!trd_parse(data, size, parsed)) return -1;
    return trdos::list_entries_common(parsed.entries, parsed.count,
                                      out_entries, max_entries);
}

// ============================================================================
// Container: Extract Entry
// ============================================================================

static bool trd_extract_entry(const uint8_t* data, size_t size,
                              int entry_index, uint8_t** out_data,
                              size_t* out_size) {
    trd_parsed_t parsed;
    if (!trd_parse(data, size, parsed)) return false;
    if (entry_index < 0 || entry_index >= parsed.count) return false;

    const trdos::file_entry_t& e = parsed.entries[entry_index];
    size_t payload_size = trdos::file_data_length(e);
    if (payload_size > parsed.data_sizes[entry_index])
        payload_size = parsed.data_sizes[entry_index];

    *out_data = static_cast<uint8_t*>(std::malloc(payload_size));
    if (!*out_data) return false;

    std::memcpy(*out_data, parsed.data_ptrs[entry_index], payload_size);
    *out_size = payload_size;
    return true;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* trd_extensions[] = { ".trd", nullptr };

const format_descriptor_t TRD_FORMAT_DESCRIPTOR = {
    "TRD",
    "ZX Spectrum TR-DOS Disk Image",
    trd_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER,
    trd_identify,
    trd_load,
    trd_list_entries,
    trd_extract_entry
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(TRD, &TRD_FORMAT_DESCRIPTOR)
