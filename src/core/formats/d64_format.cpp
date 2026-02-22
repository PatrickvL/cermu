/**
 * D64 Format Handler — Implementation
 */

#include "d64_format.h"
#include "format_registry.h"
#include "../encoding/petscii.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// D64 Track / Sector Geometry
// ============================================================================

static int d64_track_offset(int track) {
    /* Sectors per track for 1541:
     *   Tracks  1-17 : 21 sectors
     *   Tracks 18-24 : 19 sectors
     *   Tracks 25-30 : 18 sectors
     *   Tracks 31-35 : 17 sectors
     *   Tracks 36-40 : 17 sectors (extended) */
    static const int sectors_per_track[] = {
        0,
        21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
        19,19,19,19,19,19,19,
        18,18,18,18,18,18,
        17,17,17,17,17,
        17,17,17,17,17
    };
    int offset = 0;
    for (int t = 1; t < track && t <= 40; t++)
        offset += sectors_per_track[t] * D64_SECTOR_SIZE;
    return offset;
}

int d64_sector_offset(int track, int sector) {
    return d64_track_offset(track) + sector * D64_SECTOR_SIZE;
}

int d64_max_sector(int track) {
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

// ============================================================================
// Open / Close
// ============================================================================

bool commodore_d64_s::open(const char* filepath) {
    if (!filepath) return false;
    memset(this, 0, sizeof(*this));

    data = format_read_entire_file(filepath, &data_size);
    if (!data) {
        printf("D64Format: Cannot open D64 file: %s\n", filepath);
        return false;
    }
    owns_data = true;

    switch (data_size) {
        case D64_STANDARD_SIZE:     num_tracks = 35; has_errors = false; break;
        case D64_STANDARD_SIZE_ERR: num_tracks = 35; has_errors = true;  break;
        case D64_EXTENDED_SIZE:     num_tracks = 40; has_errors = false; break;
        case D64_EXTENDED_SIZE_ERR: num_tracks = 40; has_errors = true;  break;
        default:
            printf("D64Format: Unrecognized D64 file size: %zu bytes\n", data_size);
            free(data);
            data = NULL;
            return false;
    }

    printf("D64Format: Opened D64: %d tracks, %s error bytes\n",
           num_tracks, has_errors ? "with" : "no");
    return true;
}

bool commodore_d64_s::open_mem(const uint8_t* buf, size_t size) {
    if (!buf) return false;
    memset(this, 0, sizeof(*this));

    data = (uint8_t*)buf;
    data_size = size;
    owns_data = false;

    if (size == D64_STANDARD_SIZE || size == D64_STANDARD_SIZE_ERR) {
        num_tracks = 35;
        has_errors = (size == D64_STANDARD_SIZE_ERR);
    } else if (size == D64_EXTENDED_SIZE || size == D64_EXTENDED_SIZE_ERR) {
        num_tracks = 40;
        has_errors = (size == D64_EXTENDED_SIZE_ERR);
    } else {
        return false;
    }
    return true;
}

void commodore_d64_s::close() {
    if (owns_data && data) free(data);
    memset(this, 0, sizeof(*this));
}

// ============================================================================
// Directory
// ============================================================================

bool commodore_d64_s::read_directory(commodore_d64_directory_t* out_dir) const {
    if (!data || !out_dir) return false;
    memset(out_dir, 0, sizeof(*out_dir));

    /* BAM sector (track 18, sector 0) — disk name and ID */
    int bam_off = d64_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    if (bam_off + D64_SECTOR_SIZE > (int)data_size) return false;

    const uint8_t* bam = data + bam_off;
    memcpy(out_dir->disk_name, bam + 0x90, 16);
    out_dir->disk_name[16] = '\0';
    petscii_trim_padding(out_dir->disk_name, 16);

    memcpy(out_dir->disk_id, bam + 0xA2, 5);
    out_dir->disk_id[5] = '\0';

    /* Directory chain starting at track 18, sector 1 */
    int dir_track  = D64_DIR_TRACK;
    int dir_sector = D64_DIR_SECTOR;
    int entry_count = 0;

    for (int chain = 0; chain < 18 && dir_track != 0; chain++) {
        int offset = d64_sector_offset(dir_track, dir_sector);
        if (offset + D64_SECTOR_SIZE > (int)data_size) break;

        const uint8_t* sector = data + offset;
        int next_track  = sector[0];
        int next_sector = sector[1];

        for (int e = 0; e < 8 && entry_count < D64_MAX_DIR_ENTRIES; e++) {
            const uint8_t* entry = sector + (e * 32);
            uint8_t file_type = entry[2];

            if ((file_type & D64_FTYPE_MASK) == D64_FTYPE_DEL && !(file_type & D64_FTYPE_CLOSED))
                continue;

            commodore_d64_entry_t* de = &out_dir->entries[entry_count];
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

    out_dir->count = entry_count;
    printf("D64Format: Directory: \"%s\" [%s], %d entries\n",
           out_dir->disk_name, out_dir->disk_id, entry_count);
    return true;
}

// ============================================================================
// File Extraction
// ============================================================================

bool commodore_d64_s::extract_file(int entry_idx,
                                   uint8_t** out_data, size_t* out_size) const {
    if (!data || !out_data || !out_size) return false;

    commodore_d64_directory_t dir;
    if (!read_directory(&dir)) return false;
    if (entry_idx < 0 || entry_idx >= dir.count) return false;

    const commodore_d64_entry_t* entry = &dir.entries[entry_idx];

    size_t capacity = (size_t)entry->size_blocks * 254 + 256;
    uint8_t* buf = (uint8_t*)malloc(capacity);
    if (!buf) return false;

    size_t total = 0;
    int track  = entry->start_track;
    int sector = entry->start_sector;

    for (int chain = 0; chain < 1000 && track != 0; chain++) {
        if (track < 1 || track > num_tracks || sector >= d64_max_sector(track)) {
            printf("D64Format: Bad track/sector: %d/%d\n", track, sector);
            free(buf);
            return false;
        }

        int offset = d64_sector_offset(track, sector);
        if (offset + D64_SECTOR_SIZE > (int)data_size) {
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
    printf("D64Format: Extracted \"%s\": %zu bytes\n", entry->filename, total);
    return true;
}

bool commodore_d64_s::extract_first_prg(commodore_prg_t* out_prg) const {
    if (!out_prg) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    commodore_d64_directory_t dir;
    if (!read_directory(&dir)) return false;

    for (int i = 0; i < dir.count; i++) {
        uint8_t ft = dir.entries[i].file_type;
        if ((ft & D64_FTYPE_MASK) == D64_FTYPE_PRG && (ft & D64_FTYPE_CLOSED)) {
            uint8_t* raw = NULL;
            size_t raw_size = 0;
            if (extract_file(i, &raw, &raw_size)) {
                bool ok = commodore_prg_parse(raw, raw_size, out_prg);
                free(raw);
                if (ok) {
                    printf("D64Format: First PRG: \"%s\" load=$%04X size=%zu\n",
                           dir.entries[i].filename, out_prg->load_addr, out_prg->data_size);
                    return true;
                }
            }
        }
    }

    printf("D64Format: No PRG files found in directory\n");
    return false;
}

// ============================================================================
// Format Identification
// ============================================================================

static float d64_identify(const uint8_t* data, size_t file_size, const char* extension) {
    /* D64 identification is primarily by file size */
    if (file_size == D64_STANDARD_SIZE || file_size == D64_STANDARD_SIZE_ERR ||
        file_size == D64_EXTENDED_SIZE || file_size == D64_EXTENDED_SIZE_ERR) {
        return 0.95f;
    }
    if (extension && format_ext_match(extension, ".d64")) return 0.8f;
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool d64_load(const char* filepath, format_load_result_t* out) {
    commodore_d64_t d64;
    if (d64.open(filepath)) {
        if (d64.extract_first_prg(&out->program)) {
            out->type = FORMAT_LOAD_PROGRAM;
            d64.close();
            return true;
        }
        d64.close();
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "D64 opened but no PRG found: %s", filepath);
    } else {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Failed to open D64: %s", filepath);
    }
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* d64_extensions[] = { ".d64", NULL };

const format_descriptor_t D64_FORMAT_DESCRIPTOR = {
    "D64",
    "1541 Disk Image",
    d64_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER | FORMAT_CAP_VOLUME,
    d64_identify,
    d64_load
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(D64, &D64_FORMAT_DESCRIPTOR)
