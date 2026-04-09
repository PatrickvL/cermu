/**
 * D81 Format Handler — 1581 Disk Image
 *
 * The Commodore 1581 is a 3.5" DD floppy drive using a linear sector layout:
 *   80 tracks × 40 sectors × 256 bytes = 819,200 bytes
 *
 * Directory structure:
 *   Track 40, Sector 0:  Header sector (disk name, ID)
 *   Track 40, Sectors 1-2: BAM (Block Availability Map)
 *   Track 40, Sector 3+: Directory chain (8 entries per sector, same format as D64)
 *
 * File types and directory entry format are identical to D64/D71.
 */

#include "core/cermu.hpp"
#include "core/formats/d64_format.hpp"   /* reuse file type constants, commodore_prg_t */
#include "core/formats/format_registry.hpp"
#include "systems/commodore/petscii.hpp"
#include <cstdlib>
#include <cstring>

// ============================================================================
// D81 Constants
// ============================================================================

#define D81_STANDARD_SIZE       819200  /**< 80 tracks × 40 sectors × 256 bytes */
#define D81_STANDARD_SIZE_ERR   822400  /**< with error bytes (3200 sectors) */
#define D81_SECTOR_SIZE         256
#define D81_SECTORS_PER_TRACK   40
#define D81_NUM_TRACKS          80
#define D81_HEADER_TRACK        40      /**< Track containing header + BAM + directory */
#define D81_HEADER_SECTOR       0       /**< Header sector (disk name, ID) */
#define D81_DIR_SECTOR          3       /**< First directory sector on track 40 */

// ============================================================================
// D81 Sector Access
// ============================================================================

static int d81_sector_offset(int track, int sector) {
    // Linear layout: all tracks have 40 sectors
    if (track < 1 || track > D81_NUM_TRACKS) return -1;
    if (sector < 0 || sector >= D81_SECTORS_PER_TRACK) return -1;
    return ((track - 1) * D81_SECTORS_PER_TRACK + sector) * D81_SECTOR_SIZE;
}

// ============================================================================
// Directory Parsing
// ============================================================================

struct d81_directory_t {
    commodore_d64_entry_t entries[D64_MAX_DIR_ENTRIES];
    int count;
    char disk_name[17];
    char disk_id[6];
};

static bool d81_read_directory(const uint8_t* data, size_t data_size, d81_directory_t* out) {
    if (!data || !out) return false;
    memset(out, 0, sizeof(*out));

    // Read header sector (track 40, sector 0) for disk name and ID
    int hdr_off = d81_sector_offset(D81_HEADER_TRACK, D81_HEADER_SECTOR);
    if (hdr_off < 0 || hdr_off + D81_SECTOR_SIZE > (int)data_size) return false;

    const uint8_t* hdr = data + hdr_off;
    memcpy(out->disk_name, hdr + 0x04, 16);
    out->disk_name[16] = '\0';
    petscii_trim_padding(out->disk_name, 16);

    memcpy(out->disk_id, hdr + 0x16, 5);
    out->disk_id[5] = '\0';

    // Directory chain starting at track 40, sector 3
    int dir_track  = D81_HEADER_TRACK;
    int dir_sector = D81_DIR_SECTOR;
    int entry_count = 0;

    for (int chain = 0; chain < 40 && dir_track != 0; chain++) {
        int offset = d81_sector_offset(dir_track, dir_sector);
        if (offset < 0 || offset + D81_SECTOR_SIZE > (int)data_size) break;

        const uint8_t* sector = data + offset;
        int next_track  = sector[0];
        int next_sector = sector[1];

        for (int e = 0; e < 8 && entry_count < D64_MAX_DIR_ENTRIES; e++) {
            const uint8_t* entry = sector + (e * 32);
            uint8_t file_type = entry[2];

            if ((file_type & D64_FTYPE_MASK) == D64_FTYPE_DEL && !(file_type & D64_FTYPE_CLOSED))
                continue;

            commodore_d64_entry_t* de = &out->entries[entry_count];
            de->file_type     = file_type;
            de->start_track   = entry[3];
            de->start_sector  = entry[4];
            de->size_blocks   = format_read_le16(entry + 30);

            memcpy(de->filename, entry + 5, 16);
            de->filename[16] = '\0';
            petscii_trim_padding(de->filename, 16);

            entry_count++;
        }

        dir_track  = next_track;
        dir_sector = next_sector;
    }

    out->count = entry_count;
    log_info("D81Format: Directory: \"%s\" [%s], %d entries\n",
             out->disk_name, out->disk_id, entry_count);
    return true;
}

// ============================================================================
// File Extraction
// ============================================================================

static bool d81_extract_file(const uint8_t* data, size_t data_size,
                             const commodore_d64_entry_t* entry,
                             uint8_t** out_data, size_t* out_size) {
    if (!data || !entry || !out_data || !out_size) return false;

    size_t capacity = (size_t)entry->size_blocks * 254 + 256;
    uint8_t* buf = (uint8_t*)malloc(capacity);
    if (!buf) return false;

    size_t total = 0;
    int track  = entry->start_track;
    int sector = entry->start_sector;

    for (int chain = 0; chain < 10000 && track != 0; chain++) {
        int offset = d81_sector_offset(track, sector);
        if (offset < 0 || offset + D81_SECTOR_SIZE > (int)data_size) {
            log_info("D81Format: Bad track/sector: %d/%d\n", track, sector);
            free(buf);
            return false;
        }

        const uint8_t* sec = data + offset;
        int next_track  = sec[0];
        int next_sector = sec[1];

        if (next_track == 0) {
            int used = next_sector;
            if (used < 1) used = 254;
            if (total + (size_t)used > capacity) {
                capacity = total + used + 256;
                buf = (uint8_t*)realloc(buf, capacity);
                if (!buf) return false;
            }
            memcpy(buf + total, sec + 2, used - 1);
            total += used - 1;
        } else {
            if (total + 254 > capacity) {
                capacity += 256 * 8;
                buf = (uint8_t*)realloc(buf, capacity);
                if (!buf) return false;
            }
            memcpy(buf + total, sec + 2, 254);
            total += 254;
        }

        track  = next_track;
        sector = next_sector;
    }

    *out_data = buf;
    *out_size = total;
    return true;
}

// ============================================================================
// Format Identification
// ============================================================================

static float d81_identify(const uint8_t* /*data*/, size_t file_size, const char* extension) {
    if (file_size == D81_STANDARD_SIZE || file_size == D81_STANDARD_SIZE_ERR) {
        return 0.95f;
    }
    if (extension && format_ext_match(extension, ".d81")) return 0.8f;
    return 0.0f;
}

// ============================================================================
// Load Callback — extract first PRG
// ============================================================================

static bool d81_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    d81_directory_t dir{};
    if (!d81_read_directory(data, size, &dir)) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Failed to read D81 directory");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Find and extract first PRG file
    for (int i = 0; i < dir.count; i++) {
        uint8_t ft = dir.entries[i].file_type;
        if ((ft & D64_FTYPE_MASK) == D64_FTYPE_PRG && (ft & D64_FTYPE_CLOSED)) {
            uint8_t* raw = nullptr;
            size_t raw_size = 0;
            if (d81_extract_file(data, size, &dir.entries[i], &raw, &raw_size)) {
                bool ok = commodore_prg_parse(raw, raw_size, &out->program);
                free(raw);
                if (ok) {
                    out->type = FORMAT_LOAD_PROGRAM;
                    log_info("D81Format: First PRG: \"%s\" load=$%04X size=%zu\n",
                             dir.entries[i].filename, out->program.load_addr,
                             out->program.data_size);
                    return true;
                }
            }
        }
    }

    snprintf(out->error_msg, sizeof(out->error_msg),
             "D81 opened but no PRG found");
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

// ============================================================================
// Container Interface
// ============================================================================

static int d81_list_entries(const uint8_t* data, size_t size,
                            format_container_entry_t* entries, int max_entries) {
    d81_directory_t dir{};
    if (!d81_read_directory(data, size, &dir)) return -1;

    int count = 0;
    for (int i = 0; i < dir.count && count < max_entries; ++i) {
        const auto& de = dir.entries[i];
        if ((de.file_type & D64_FTYPE_MASK) == D64_FTYPE_DEL) continue;

        char name_buf[17]{};
        memcpy(name_buf, de.filename, 16);
        for (int j = 0; j < 16 && name_buf[j]; ++j)
            name_buf[j] = petscii_to_ascii(static_cast<uint8_t>(name_buf[j]));

        auto& entry = entries[count];
        const char* type_ext = ".prg";
        switch (de.file_type & D64_FTYPE_MASK) {
            case D64_FTYPE_SEQ: type_ext = ".seq"; break;
            case D64_FTYPE_USR: type_ext = ".usr"; break;
            case D64_FTYPE_REL: type_ext = ".rel"; break;
            default: break;
        }
        snprintf(entry.display_name, sizeof(entry.display_name),
                 "%s%s", name_buf, type_ext);
        entry.size  = static_cast<size_t>(de.size_blocks) * 254;
        entry.index = i;
        ++count;
    }
    return count;
}

static bool d81_extract_entry(const uint8_t* data, size_t size,
                              int entry_index, uint8_t** out_data, size_t* out_size) {
    d81_directory_t dir{};
    if (!d81_read_directory(data, size, &dir)) return false;
    if (entry_index < 0 || entry_index >= dir.count) return false;
    return d81_extract_file(data, size, &dir.entries[entry_index], out_data, out_size);
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* d81_extensions[] = { ".d81", NULL };

static const format_descriptor_t D81_FORMAT_DESCRIPTOR = {
    "D81",
    "1581 Disk Image",
    d81_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER | FORMAT_CAP_VOLUME,
    d81_identify,
    d81_load,
    d81_list_entries,
    d81_extract_entry
};

REGISTER_FORMAT(D81, &D81_FORMAT_DESCRIPTOR)
