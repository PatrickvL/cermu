/**
 * Apple II DSK Format Handler — Implementation
 *
 * Parses Apple II disc images (.dsk, .do, .po, .nib, .2mg).
 * Identifies by file size (DSK/NIB) or "2IMG" magic (2MG).
 *
 * Supports DOS 3.3 catalog parsing for file extraction.
 * Falls back to raw disc load if no filesystem is recognized.
 *
 * Uses disk_image_common.hpp for shared loading logic.
 */

#include "core/formats/apple_dsk_format.hpp"
#include "core/formats/disk_image_common.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t DSK_SIZE_16SEC   = 143360;   // 35 × 16 × 256
static constexpr size_t DSK_SIZE_13SEC   = 116480;   // 35 × 13 × 256 (DOS 3.2)
static constexpr size_t NIB_SIZE         = 232960;   // 35 × 6656
static constexpr size_t SECTOR_SIZE      = 256;
static constexpr int    TRACKS           = 35;
static constexpr int    SECTORS_PER_TRACK = 16;

// DOS 3.3 VTOC is at track 17, sector 0
static constexpr int    VTOC_TRACK       = 17;
static constexpr int    VTOC_SECTOR      = 0;

// DOS 3.3 catalog starts at track 17, sector 15 (typical)
// VTOC byte $01 = catalog track, $02 = catalog sector

// DOS 3.3 file types
static constexpr uint8_t DOS33_TYPE_TEXT       = 0x00;
static constexpr uint8_t DOS33_TYPE_INTEGER    = 0x01;
static constexpr uint8_t DOS33_TYPE_APPLESOFT  = 0x02;
static constexpr uint8_t DOS33_TYPE_BINARY     = 0x04;
static constexpr uint8_t DOS33_TYPE_LOCKED     = 0x80;

// DOS-order sector interleave (logical sector → physical sector)
static constexpr int DOS_SECTOR_MAP[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

// ============================================================================
// Internal: Sector access
// ============================================================================

/// Read a sector from a DSK image (DOS 3.3 order).
static const uint8_t* dsk_read_sector(const uint8_t* img, size_t img_size,
                                      int track, int sector) {
    if (track < 0 || track >= TRACKS || sector < 0 || sector >= SECTORS_PER_TRACK)
        return nullptr;
    size_t offset = (static_cast<size_t>(track) * SECTORS_PER_TRACK
                   + static_cast<size_t>(sector)) * SECTOR_SIZE;
    if (offset + SECTOR_SIZE > img_size) return nullptr;
    return img + offset;
}

// ============================================================================
// Internal: DOS 3.3 Catalog Parsing
// ============================================================================

struct apple_parsed_t {
    disk_image::file_entry_t entries[disk_image::MAX_FILES];
    uint8_t*                 data_bufs[disk_image::MAX_FILES];  // Owned
    size_t                   data_sizes[disk_image::MAX_FILES];
    int                      count;
};

static void apple_parsed_free(apple_parsed_t& p) {
    for (int i = 0; i < p.count; ++i) {
        std::free(p.data_bufs[i]);
        p.data_bufs[i] = nullptr;
    }
}

/// Follow a track/sector list to collect file data.
static uint8_t* collect_file_data(const uint8_t* img, size_t img_size,
                                  int ts_track, int ts_sector,
                                  size_t* out_size) {
    // Allocate generous buffer (max 400 sectors × 256 = ~100KB)
    size_t alloc = 128 * 1024;
    uint8_t* buf = static_cast<uint8_t*>(std::malloc(alloc));
    if (!buf) return nullptr;

    size_t total = 0;
    int visited = 0;

    while (ts_track != 0 && visited < 400) {
        const uint8_t* ts_sec = dsk_read_sector(img, img_size, ts_track, ts_sector);
        if (!ts_sec) break;
        visited++;

        // Track/sector list format:
        //   $00: unused
        //   $01: next T/S list track
        //   $02: next T/S list sector
        //   $0C-$FF: pairs of (track, sector) for data sectors
        int next_track  = ts_sec[0x01];
        int next_sector = ts_sec[0x02];

        for (int i = 0x0C; i < 0xFF; i += 2) {
            int dt = ts_sec[i];
            int ds = ts_sec[i + 1];
            if (dt == 0) continue;  // unused entry

            const uint8_t* data_sector = dsk_read_sector(img, img_size, dt, ds);
            if (!data_sector) continue;

            if (total + SECTOR_SIZE > alloc) {
                alloc *= 2;
                uint8_t* bigger = static_cast<uint8_t*>(std::realloc(buf, alloc));
                if (!bigger) { std::free(buf); *out_size = 0; return nullptr; }
                buf = bigger;
            }

            std::memcpy(buf + total, data_sector, SECTOR_SIZE);
            total += SECTOR_SIZE;
        }

        ts_track  = next_track;
        ts_sector = next_sector;
    }

    *out_size = total;
    return buf;
}

/// Parse DOS 3.3 catalog and extract files.
static bool dos33_parse(const uint8_t* img, size_t img_size, apple_parsed_t& out) {
    out.count = 0;

    // Read VTOC at track 17, sector 0
    const uint8_t* vtoc = dsk_read_sector(img, img_size, VTOC_TRACK, VTOC_SECTOR);
    if (!vtoc) return false;

    // Catalog track/sector from VTOC
    int cat_track  = vtoc[0x01];
    int cat_sector = vtoc[0x02];

    // Walk catalog sectors
    int visited = 0;
    while (cat_track != 0 && visited < 20 && out.count < disk_image::MAX_FILES) {
        const uint8_t* cat = dsk_read_sector(img, img_size, cat_track, cat_sector);
        if (!cat) break;
        visited++;

        // Next catalog sector
        int next_track  = cat[0x01];
        int next_sector = cat[0x02];

        // File entries at offsets $0B, $2E, $51, $74, $97, $BA, $DD (7 per sector)
        for (int slot = 0; slot < 7 && out.count < disk_image::MAX_FILES; ++slot) {
            int off = 0x0B + slot * 0x23;
            if (off + 0x23 > static_cast<int>(SECTOR_SIZE)) break;

            int ts_track  = cat[off + 0x00];  // First T/S list track
            int ts_sector = cat[off + 0x01];  // First T/S list sector

            if (ts_track == 0 || ts_track == 0xFF) continue;  // empty or deleted

            uint8_t raw_type = cat[off + 0x02];
            bool locked = (raw_type & DOS33_TYPE_LOCKED) != 0;
            uint8_t file_type = raw_type & 0x7F;

            // Filename: 30 bytes at offset $03, high bit set on each char
            disk_image::file_entry_t& e = out.entries[out.count];
            for (int j = 0; j < 30; ++j)
                e.name[j] = static_cast<char>(cat[off + 0x03 + j] & 0x7F);
            e.name[30] = '\0';
            // Trim trailing spaces
            for (int j = 29; j >= 0 && e.name[j] == ' '; --j)
                e.name[j] = '\0';

            // File size in sectors
            uint16_t file_sectors = format_read_le16(cat + off + 0x21);
            (void)file_sectors;

            e.locked = locked;

            switch (file_type) {
                case DOS33_TYPE_APPLESOFT:
                    e.type = disk_image::DISK_FILE_BASIC;
                    break;
                case DOS33_TYPE_INTEGER:
                    e.type = disk_image::DISK_FILE_BASIC;
                    break;
                case DOS33_TYPE_BINARY:
                    e.type = disk_image::DISK_FILE_CODE;
                    break;
                case DOS33_TYPE_TEXT:
                    e.type = disk_image::DISK_FILE_TEXT;
                    break;
                default:
                    e.type = disk_image::DISK_FILE_UNKNOWN;
                    break;
            }

            // Collect file data from T/S list
            size_t data_size = 0;
            uint8_t* data = collect_file_data(img, img_size, ts_track, ts_sector,
                                              &data_size);
            if (!data || data_size == 0) {
                std::free(data);
                continue;
            }

            // For binary files, first 4 bytes are: load_addr(2) + length(2)
            if (file_type == DOS33_TYPE_BINARY && data_size >= 4) {
                e.load_addr = format_read_le16(data);
                uint16_t byte_len = format_read_le16(data + 2);
                e.exec_addr = e.load_addr;  // Binary entry = load address
                e.size = byte_len;
                if (byte_len + 4 <= data_size) {
                    // Strip the 4-byte header
                    std::memmove(data, data + 4, byte_len);
                    data_size = byte_len;
                }
            } else if (file_type == DOS33_TYPE_APPLESOFT && data_size >= 2) {
                e.load_addr = 0x0801;  // Standard Applesoft BASIC start
                e.exec_addr = 0;
                // First 2 bytes: length
                uint16_t byte_len = format_read_le16(data);
                e.size = byte_len;
                if (byte_len + 2 <= data_size) {
                    std::memmove(data, data + 2, byte_len);
                    data_size = byte_len;
                }
            } else {
                e.load_addr = 0x0800;  // Default
                e.exec_addr = 0;
                e.size = static_cast<uint32_t>(data_size);
            }

            out.data_bufs[out.count] = data;
            out.data_sizes[out.count] = data_size;
            out.count++;
        }

        cat_track  = next_track;
        cat_sector = next_sector;
    }

    return out.count > 0;
}

// ============================================================================
// Internal: 2MG Header Parsing
// ============================================================================

/// Extract the raw disc image from a 2MG wrapper.
/// Returns pointer into the input buffer and sets size/format.
static const uint8_t* parse_2mg(const uint8_t* data, size_t size,
                                size_t* out_img_size) {
    if (size < 64) return nullptr;
    // Verify "2IMG" magic
    if (data[0] != '2' || data[1] != 'I' || data[2] != 'M' || data[3] != 'G')
        return nullptr;

    uint32_t data_offset = format_read_le32(data + 0x18);
    uint32_t data_length = format_read_le32(data + 0x1C);

    if (data_offset + data_length > size) return nullptr;

    *out_img_size = data_length;
    return data + data_offset;
}

// ============================================================================
// Identification
// ============================================================================

static float apple_dsk_identify(const uint8_t* data, size_t file_size,
                                const char* extension) {
    // Content checks
    if (data) {
        // 2MG magic
        if (file_size >= 64 &&
            data[0] == '2' && data[1] == 'I' && data[2] == 'M' && data[3] == 'G')
            return 0.95f;

        // Standard DSK size
        if (file_size == DSK_SIZE_16SEC || file_size == DSK_SIZE_13SEC)
            return 0.5f;   // Valid size but ambiguous without extension

        // NIB size
        if (file_size == NIB_SIZE)
            return 0.5f;
    }

    // Extension checks
    if (extension) {
        if (format_ext_match(extension, ".dsk")) return 0.5f;   // shared
        if (format_ext_match(extension, ".do"))  return 0.85f;   // DOS-order
        if (format_ext_match(extension, ".po"))  return 0.85f;   // ProDOS-order
        if (format_ext_match(extension, ".nib")) return 0.85f;
        if (format_ext_match(extension, ".2mg")) return 0.90f;
    }

    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool apple_dsk_load(const uint8_t* data, size_t size,
                           format_load_result_t* out) {
    if (!data || size < SECTOR_SIZE) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Apple DSK: File too small");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    const uint8_t* img = data;
    size_t img_size = size;

    // Handle 2MG wrapper
    if (data[0] == '2' && data[1] == 'I' && data[2] == 'M' && data[3] == 'G') {
        img = parse_2mg(data, size, &img_size);
        if (!img) {
            snprintf(out->error_msg, sizeof(out->error_msg),
                     "Apple DSK: Invalid 2MG header");
            out->type = FORMAT_LOAD_ERROR;
            return false;
        }
    }

    // NIB format: not yet supported for file extraction
    // (requires GCR 6-and-2 decoding — complex)
    if (img_size == NIB_SIZE) {
        // Load as raw for now — system can mount it directly
        out->program.data = static_cast<uint8_t*>(std::malloc(img_size));
        if (!out->program.data) {
            snprintf(out->error_msg, sizeof(out->error_msg),
                     "Apple DSK: Out of memory");
            out->type = FORMAT_LOAD_ERROR;
            return false;
        }
        std::memcpy(out->program.data, img, img_size);
        out->program.data_size = img_size;
        out->program.load_addr = 0;
        out->program.end_addr = 0;
        out->type = FORMAT_LOAD_RAW;
        log_info("Apple DSK: Loaded NIB image (%zu bytes, raw)\n", img_size);
        return true;
    }

    // Try DOS 3.3 catalog extraction
    if (img_size == DSK_SIZE_16SEC || img_size == DSK_SIZE_13SEC) {
        apple_parsed_t parsed{};
        if (dos33_parse(img, img_size, parsed)) {
            const uint8_t* ptrs[disk_image::MAX_FILES];
            for (int i = 0; i < parsed.count; ++i)
                ptrs[i] = parsed.data_bufs[i];

            bool ok = disk_image::load_best_entry(parsed.entries, parsed.count,
                                                   ptrs, parsed.data_sizes,
                                                   "Apple DSK", out);
            apple_parsed_free(parsed);
            return ok;
        }
    }

    // Fallback: load entire disc as raw
    out->program.data = static_cast<uint8_t*>(std::malloc(img_size));
    if (!out->program.data) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Apple DSK: Out of memory");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }
    std::memcpy(out->program.data, img, img_size);
    out->program.data_size = img_size;
    out->program.load_addr = 0;
    out->program.end_addr = 0;
    out->type = FORMAT_LOAD_RAW;
    log_info("Apple DSK: Loaded disc image (%zu bytes, raw)\n", img_size);
    return true;
}

// ============================================================================
// List Entries
// ============================================================================

static int apple_dsk_list(const uint8_t* data, size_t size,
                          format_container_entry_t* entries, int max_entries) {
    if (!data || size < SECTOR_SIZE) return -1;

    const uint8_t* img = data;
    size_t img_size = size;

    if (data[0] == '2' && data[1] == 'I' && data[2] == 'M' && data[3] == 'G') {
        img = parse_2mg(data, size, &img_size);
        if (!img) return -1;
    }

    if (img_size != DSK_SIZE_16SEC && img_size != DSK_SIZE_13SEC)
        return -1;  // NIB doesn't support directory listing

    apple_parsed_t parsed{};
    if (!dos33_parse(img, img_size, parsed))
        return -1;

    int result = disk_image::list_entries_common(parsed.entries, parsed.count,
                                                  entries, max_entries);
    apple_parsed_free(parsed);
    return result;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* apple_dsk_extensions[] = {
    ".dsk", ".do", ".po", ".nib", ".2mg", nullptr
};

const format_descriptor_t APPLE_DSK_FORMAT_DESCRIPTOR = {
    "Apple DSK",
    "Apple II Disc Image (DSK/NIB/2MG)",
    apple_dsk_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER | FORMAT_CAP_METADATA,
    apple_dsk_identify,
    apple_dsk_load,
    apple_dsk_list,
    nullptr     // extract_entry
};

REGISTER_FORMAT(APPLE_DSK, &APPLE_DSK_FORMAT_DESCRIPTOR)
