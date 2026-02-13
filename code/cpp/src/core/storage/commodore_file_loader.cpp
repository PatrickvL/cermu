/**
 * Commodore File Loader - System-Independent File Format Handling
 * 
 * Shared file format parsers for Commodore 8-bit systems.
 * See commodore_file_loader.h for API documentation.
 */

#include "commodore_file_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif

// ============================================================================
// Internal Helpers
// ============================================================================

static uint16_t read_le16(const uint8_t* data) {
    return (uint16_t)(data[0] | (data[1] << 8));
}

static uint32_t read_le32(const uint8_t* data) {
    return (uint32_t)(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}

static uint16_t read_be16(const uint8_t* data) {
    return (uint16_t)((data[0] << 8) | data[1]);
}

static uint32_t read_be32(const uint8_t* data) {
    return (uint32_t)((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
}

/** Case-insensitive extension check */
static bool ext_match(const char* ext, const char* target) {
    if (!ext || !target) return false;
    while (*ext && *target) {
        if (tolower((unsigned char)*ext) != tolower((unsigned char)*target)) return false;
        ext++; target++;
    }
    return *ext == *target;  // Both must end
}

/** Read entire file into heap-allocated buffer. Returns NULL on failure. */
static uint8_t* read_entire_file(const char* filepath, size_t* out_size) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) { fclose(f); return NULL; }

    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

/** Trim trailing $A0 (shifted space) padding from PETSCII strings */
static void trim_petscii(char* str, int maxlen) {
    int end = maxlen - 1;
    while (end >= 0 && ((uint8_t)str[end] == 0xA0 || str[end] == ' ' || str[end] == '\0')) {
        str[end] = '\0';
        end--;
    }
}

// ============================================================================
// BASIC V2 Parameters — Pre-defined for Common Systems
// ============================================================================

const commodore_basic_params_t COMMODORE_BASIC_C64 = {
    .basic_start       = 0x0801,
    .sys_token         = 0x9E,
    .rem_token         = 0x8F,
    .peek_token        = 0xC2,
    .basic_start_ptr_lo = 0x2B,
    .basic_start_ptr_hi = 0x2C,
};

const commodore_basic_params_t COMMODORE_BASIC_VIC20 = {
    .basic_start       = 0x1001,
    .sys_token         = 0x9E,
    .rem_token         = 0x8F,
    .peek_token        = 0xC2,
    .basic_start_ptr_lo = 0x2B,
    .basic_start_ptr_hi = 0x2C,
};

const commodore_basic_params_t COMMODORE_BASIC_C16 = {
    .basic_start       = 0x1001,
    .sys_token         = 0x9E,  // SYS token is same in BASIC 3.5
    .rem_token         = 0x8F,
    .peek_token        = 0xC2,
    .basic_start_ptr_lo = 0x2B,
    .basic_start_ptr_hi = 0x2C,
};

// ============================================================================
// PRG Format Implementation
// ============================================================================

bool commodore_prg_load(const char* filepath, commodore_prg_t* out_prg) {
    if (!filepath || !out_prg) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    size_t file_size = 0;
    uint8_t* file_data = read_entire_file(filepath, &file_size);
    if (!file_data) {
        printf("CommodoreLoader: Cannot open PRG file: %s\n", filepath);
        return false;
    }

    bool ok = commodore_prg_parse(file_data, file_size, out_prg);
    free(file_data);
    return ok;
}

bool commodore_prg_parse(const uint8_t* buffer, size_t size, commodore_prg_t* out_prg) {
    if (!buffer || !out_prg || size < 2) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    uint16_t load_addr = read_le16(buffer);
    size_t data_size = size - 2;

    if ((uint32_t)load_addr + data_size > 0x10000) {
        printf("CommodoreLoader: PRG data exceeds 64KB (load=$%04X, size=%zu)\n",
               load_addr, data_size);
        return false;
    }

    out_prg->data = (uint8_t*)malloc(data_size);
    if (!out_prg->data) return false;

    memcpy(out_prg->data, buffer + 2, data_size);
    out_prg->data_size = data_size;
    out_prg->load_addr = load_addr;
    out_prg->end_addr  = (uint16_t)(load_addr + data_size);

    return true;
}

void commodore_prg_free(commodore_prg_t* prg) {
    if (prg && prg->data) {
        free(prg->data);
        prg->data = NULL;
        prg->data_size = 0;
    }
}

// ============================================================================
// BIN Format Implementation
// ============================================================================

bool commodore_bin_load(const char* filepath, uint8_t** out_data, size_t* out_size) {
    if (!filepath || !out_data || !out_size) return false;
    *out_data = read_entire_file(filepath, out_size);
    return (*out_data != NULL);
}

// ============================================================================
// BASIC V2 SYS Parsing — Shared Across All Commodore Systems
// ============================================================================

/**
 * Evaluate a BASIC expression following a SYS token.
 * Handles:
 *   - Simple numeric values: SYS 2061
 *   - PEEK expressions: SYS PEEK(43)+PEEK(44)*256+offset
 *   - Tokenized PEEK ($C2)
 */
static uint16_t evaluate_sys_expression(const uint8_t* expr, size_t len,
                                        uint16_t basic_start,
                                        uint8_t peek_token) {
    size_t pos = 0;

    // Skip leading spaces
    while (pos < len && expr[pos] == ' ') pos++;
    if (pos >= len) return 0;

    // Case 1: Simple numeric address (ASCII digits)
    if (isdigit(expr[pos])) {
        uint32_t addr = 0;
        while (pos < len && isdigit(expr[pos])) {
            addr = addr * 10 + (expr[pos] - '0');
            pos++;
        }
        return (addr <= 0xFFFF) ? (uint16_t)addr : 0;
    }

    // Case 2: Tokenized PEEK expression ($C2 = PEEK token)
    if (expr[pos] == peek_token) {
        // Complex PEEK expression: PEEK(lo)+PEEK(hi)*256[+offset]
        // Standard pattern computes basic_start + offset.
        // Since we know basic_start, find the last "offset" number.
        uint16_t offset = 0;

        // Scan backwards for the last number that isn't a zero-page address or 256
        for (int i = (int)len - 1; i >= 0; i--) {
            if (isdigit(expr[i])) {
                int j = i;
                uint32_t num = 0;
                uint32_t mult = 1;
                while (j >= 0 && isdigit(expr[j])) {
                    num += (expr[j] - '0') * mult;
                    mult *= 10;
                    j--;
                }
                // Filter out known non-offset values
                if (num != 43 && num != 44 && num != 256 &&
                    num != 45 && num != 46) {
                    offset = (num <= 0xFFFF) ? (uint16_t)num : 0;
                    break;
                }
                i = j + 1;
            }
        }
        return basic_start + offset;
    }

    // Case 3: Text "PEEK" (un-tokenized or from detokenizer)
    if (pos + 4 <= len && strncasecmp((const char*)&expr[pos], "PEEK", 4) == 0) {
        uint16_t offset = 0;
        for (int i = (int)len - 1; i >= 0; i--) {
            if (isdigit(expr[i])) {
                int j = i;
                uint32_t num = 0;
                uint32_t mult = 1;
                while (j >= 0 && isdigit(expr[j])) {
                    num += (expr[j] - '0') * mult;
                    mult *= 10;
                    j--;
                }
                if (num != 43 && num != 44 && num != 256 &&
                    num != 45 && num != 46) {
                    offset = (num <= 0xFFFF) ? (uint16_t)num : 0;
                    break;
                }
                i = j + 1;
            }
        }
        return basic_start + offset;
    }

    return 0;
}

bool commodore_basic_parse_sys(commodore_mem_read_fn mem_read, void* mem_ctx,
                               uint16_t start_addr,
                               const commodore_basic_params_t* params,
                               int max_lines,
                               commodore_basic_sys_t* out_sys) {
    if (!mem_read || !params || !out_sys) return false;
    memset(out_sys, 0, sizeof(*out_sys));

    uint16_t current = start_addr;

    for (int line = 0; line < max_lines; line++) {
        // Read next-line pointer (little-endian)
        if (current + 4 >= 0xFFFF) break;
        uint16_t next_line = mem_read(mem_ctx, current) |
                             (mem_read(mem_ctx, current + 1) << 8);

        if (next_line == 0x0000) break;  // End of BASIC program

        // Read line number
        uint16_t line_number = mem_read(mem_ctx, current + 2) |
                               (mem_read(mem_ctx, current + 3) << 8);

        uint16_t line_data = current + 4;

        // Check if this is a REM line (skip it)
        bool is_rem = false;
        for (uint16_t p = line_data; p < next_line; p++) {
            uint8_t b = mem_read(mem_ctx, p);
            if (b == params->rem_token) { is_rem = true; break; }
            if (b == 0x00) break;
            if (b == params->sys_token) break;  // Found SYS before REM
            if (b != ' ' && b != ':') break;    // Non-whitespace, non-colon → check further
        }
        if (is_rem) { current = next_line; continue; }

        // Scan for SYS token in line data
        for (uint16_t pos = line_data; pos < next_line; pos++) {
            uint8_t b = mem_read(mem_ctx, pos);
            if (b == 0x00) break;  // End of line

            if (b == params->sys_token) {
                pos++;
                // Skip spaces after SYS
                while (pos < next_line && mem_read(mem_ctx, pos) == ' ') pos++;

                // Collect expression bytes until end-of-line, colon, or zero
                uint8_t expr_buf[256];
                size_t expr_len = 0;
                while (pos < next_line && expr_len < sizeof(expr_buf) - 1) {
                    uint8_t eb = mem_read(mem_ctx, pos);
                    if (eb == 0x00 || eb == ':') break;
                    expr_buf[expr_len++] = eb;
                    pos++;
                }

                uint16_t sys_addr = evaluate_sys_expression(
                    expr_buf, expr_len, start_addr, params->peek_token);

                if (sys_addr != 0) {
                    out_sys->sys_address = sys_addr;
                    out_sys->line_number = line_number;
                    out_sys->found = true;
                    printf("CommodoreLoader: BASIC line %u: SYS %u ($%04X)\n",
                           line_number, sys_addr, sys_addr);
                    return true;
                }
            }
        }

        current = next_line;
    }

    return false;
}

// ============================================================================
// D64 Format Implementation
// ============================================================================

/**
 * D64 track/sector layout.
 * Returns the byte offset in the D64 image for a given track and sector.
 * Track numbers are 1-based (1-35 for standard, 1-40 for extended).
 */
static int d64_track_offset(int track) {
    // Sectors per track for 1541:
    //   Tracks  1-17: 21 sectors
    //   Tracks 18-24: 19 sectors
    //   Tracks 25-30: 18 sectors
    //   Tracks 31-35: 17 sectors
    //   Tracks 36-40: 17 sectors (extended)
    static const int sectors_per_track[] = {
        0,  // Track 0 doesn't exist
        21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  // 1-17
        19, 19, 19, 19, 19, 19, 19,                                            // 18-24
        18, 18, 18, 18, 18, 18,                                                // 25-30
        17, 17, 17, 17, 17,                                                    // 31-35
        17, 17, 17, 17, 17                                                     // 36-40
    };

    int offset = 0;
    for (int t = 1; t < track && t <= 40; t++) {
        offset += sectors_per_track[t] * D64_SECTOR_SIZE;
    }
    return offset;
}

static int d64_sector_offset(int track, int sector) {
    return d64_track_offset(track) + sector * D64_SECTOR_SIZE;
}

static int d64_max_sector(int track) {
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

bool commodore_d64_open(const char* filepath, commodore_d64_t* out_d64) {
    if (!filepath || !out_d64) return false;
    memset(out_d64, 0, sizeof(*out_d64));

    out_d64->data = read_entire_file(filepath, &out_d64->data_size);
    if (!out_d64->data) {
        printf("CommodoreLoader: Cannot open D64 file: %s\n", filepath);
        return false;
    }
    out_d64->owns_data = true;

    // Determine geometry from file size
    switch (out_d64->data_size) {
        case D64_STANDARD_SIZE:
            out_d64->num_tracks = 35;
            out_d64->has_errors = false;
            break;
        case D64_STANDARD_SIZE_ERR:
            out_d64->num_tracks = 35;
            out_d64->has_errors = true;
            break;
        case D64_EXTENDED_SIZE:
            out_d64->num_tracks = 40;
            out_d64->has_errors = false;
            break;
        case D64_EXTENDED_SIZE_ERR:
            out_d64->num_tracks = 40;
            out_d64->has_errors = true;
            break;
        default:
            printf("CommodoreLoader: Unrecognized D64 file size: %zu bytes\n", out_d64->data_size);
            free(out_d64->data);
            out_d64->data = NULL;
            return false;
    }

    printf("CommodoreLoader: Opened D64: %d tracks, %s error bytes\n",
           out_d64->num_tracks, out_d64->has_errors ? "with" : "no");
    return true;
}

bool commodore_d64_open_mem(const uint8_t* data, size_t size, commodore_d64_t* out_d64) {
    if (!data || !out_d64) return false;
    memset(out_d64, 0, sizeof(*out_d64));

    out_d64->data = (uint8_t*)data;
    out_d64->data_size = size;
    out_d64->owns_data = false;

    if (size == D64_STANDARD_SIZE || size == D64_STANDARD_SIZE_ERR) {
        out_d64->num_tracks = 35;
        out_d64->has_errors = (size == D64_STANDARD_SIZE_ERR);
    } else if (size == D64_EXTENDED_SIZE || size == D64_EXTENDED_SIZE_ERR) {
        out_d64->num_tracks = 40;
        out_d64->has_errors = (size == D64_EXTENDED_SIZE_ERR);
    } else {
        return false;
    }

    return true;
}

bool commodore_d64_read_directory(const commodore_d64_t* d64, commodore_d64_directory_t* out_dir) {
    if (!d64 || !d64->data || !out_dir) return false;
    memset(out_dir, 0, sizeof(*out_dir));

    // Read BAM sector (track 18, sector 0) for disk name and ID
    int bam_offset = d64_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    if (bam_offset + D64_SECTOR_SIZE > (int)d64->data_size) return false;

    const uint8_t* bam = d64->data + bam_offset;
    // Disk name at offset 0x90 (16 bytes), disk ID at 0xA2 (5 bytes)
    memcpy(out_dir->disk_name, bam + 0x90, 16);
    out_dir->disk_name[16] = '\0';
    trim_petscii(out_dir->disk_name, 16);

    memcpy(out_dir->disk_id, bam + 0xA2, 5);
    out_dir->disk_id[5] = '\0';

    // Read directory chain starting at track 18, sector 1
    int dir_track = D64_DIR_TRACK;
    int dir_sector = D64_DIR_SECTOR;
    int entry_count = 0;

    // Follow directory sector chain (max 18 sectors × 8 entries = 144)
    for (int chain = 0; chain < 18 && dir_track != 0; chain++) {
        int offset = d64_sector_offset(dir_track, dir_sector);
        if (offset + D64_SECTOR_SIZE > (int)d64->data_size) break;

        const uint8_t* sector = d64->data + offset;

        // Each sector has 8 directory entries of 32 bytes each
        // First 2 bytes are track/sector of next directory sector
        int next_track = sector[0];
        int next_sector = sector[1];

        for (int e = 0; e < 8 && entry_count < D64_MAX_DIR_ENTRIES; e++) {
            const uint8_t* entry = sector + (e * 32);
            uint8_t file_type = entry[2];

            // Skip empty entries (file type = 0)
            if ((file_type & D64_FTYPE_MASK) == D64_FTYPE_DEL && !(file_type & D64_FTYPE_CLOSED)) {
                continue;
            }

            commodore_d64_entry_t* de = &out_dir->entries[entry_count];
            de->file_type = file_type;
            de->start_track = entry[3];
            de->start_sector = entry[4];
            de->size_blocks = read_le16(entry + 30);

            // Copy filename (16 bytes at offset 5)
            memcpy(de->filename, entry + 5, 16);
            de->filename[16] = '\0';
            trim_petscii(de->filename, 16);

            entry_count++;
        }

        dir_track = next_track;
        dir_sector = next_sector;
    }

    out_dir->count = entry_count;
    printf("CommodoreLoader: D64 directory: \"%s\" [%s], %d entries\n",
           out_dir->disk_name, out_dir->disk_id, entry_count);
    return true;
}

bool commodore_d64_extract_file(const commodore_d64_t* d64, int entry_idx,
                                uint8_t** out_data, size_t* out_size) {
    if (!d64 || !d64->data || !out_data || !out_size) return false;

    // Read directory to get entry info
    commodore_d64_directory_t dir;
    if (!commodore_d64_read_directory(d64, &dir)) return false;
    if (entry_idx < 0 || entry_idx >= dir.count) return false;

    const commodore_d64_entry_t* entry = &dir.entries[entry_idx];

    // Follow track/sector chain to collect file data
    // Each sector: byte 0 = next track (0 = last), byte 1 = next sector
    // Data bytes: 2-255 (254 bytes per sector, except last which uses byte 1 as count)
    size_t capacity = (size_t)entry->size_blocks * 254 + 256;  // Over-allocate slightly
    uint8_t* data = (uint8_t*)malloc(capacity);
    if (!data) return false;

    size_t total = 0;
    int track = entry->start_track;
    int sector = entry->start_sector;

    // Safety: limit chain following to prevent infinite loops
    for (int chain = 0; chain < 1000 && track != 0; chain++) {
        if (track < 1 || track > d64->num_tracks || sector >= d64_max_sector(track)) {
            printf("CommodoreLoader: D64 bad track/sector: %d/%d\n", track, sector);
            free(data);
            return false;
        }

        int offset = d64_sector_offset(track, sector);
        if (offset + D64_SECTOR_SIZE > (int)d64->data_size) {
            free(data);
            return false;
        }

        const uint8_t* sec = d64->data + offset;
        int next_track = sec[0];
        int next_sector = sec[1];

        if (next_track == 0) {
            // Last sector: next_sector is the number of used bytes (1-based)
            int used = next_sector;
            if (used < 1) used = 254;
            if (total + used > capacity) {
                capacity = total + used + 256;
                data = (uint8_t*)realloc(data, capacity);
                if (!data) return false;
            }
            memcpy(data + total, sec + 2, used - 1);
            total += used - 1;
        } else {
            // Non-last sector: 254 data bytes
            if (total + 254 > capacity) {
                capacity += 256 * 8;
                data = (uint8_t*)realloc(data, capacity);
                if (!data) return false;
            }
            memcpy(data + total, sec + 2, 254);
            total += 254;
        }

        track = next_track;
        sector = next_sector;
    }

    *out_data = data;
    *out_size = total;
    printf("CommodoreLoader: D64 extracted \"%s\": %zu bytes\n", entry->filename, total);
    return true;
}

bool commodore_d64_extract_first_prg(const commodore_d64_t* d64, commodore_prg_t* out_prg) {
    if (!d64 || !out_prg) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    commodore_d64_directory_t dir;
    if (!commodore_d64_read_directory(d64, &dir)) return false;

    // Find first closed PRG file
    for (int i = 0; i < dir.count; i++) {
        uint8_t ft = dir.entries[i].file_type;
        if ((ft & D64_FTYPE_MASK) == D64_FTYPE_PRG && (ft & D64_FTYPE_CLOSED)) {
            uint8_t* raw = NULL;
            size_t raw_size = 0;
            if (commodore_d64_extract_file(d64, i, &raw, &raw_size)) {
                // The extracted data IS a PRG (first 2 bytes are load address)
                bool ok = commodore_prg_parse(raw, raw_size, out_prg);
                free(raw);
                if (ok) {
                    printf("CommodoreLoader: D64 first PRG: \"%s\" load=$%04X size=%zu\n",
                           dir.entries[i].filename, out_prg->load_addr, out_prg->data_size);
                    return true;
                }
            }
        }
    }

    printf("CommodoreLoader: D64 no PRG files found in directory\n");
    return false;
}

void commodore_d64_close(commodore_d64_t* d64) {
    if (d64) {
        if (d64->owns_data && d64->data) {
            free(d64->data);
        }
        memset(d64, 0, sizeof(*d64));
    }
}

// ============================================================================
// T64 Format Implementation
// ============================================================================

bool commodore_t64_open(const char* filepath, commodore_t64_t* out_t64) {
    if (!filepath || !out_t64) return false;
    memset(out_t64, 0, sizeof(*out_t64));

    out_t64->data = read_entire_file(filepath, &out_t64->data_size);
    if (!out_t64->data) {
        printf("CommodoreLoader: Cannot open T64 file: %s\n", filepath);
        return false;
    }
    out_t64->owns_data = true;

    // Verify T64 signature: "C64 tape image file" or "C64S tape image file"
    // Some emulators use different prefixes, check first 3 bytes "C64"
    if (out_t64->data_size < T64_HEADER_SIZE ||
        (memcmp(out_t64->data, "C64", 3) != 0)) {
        printf("CommodoreLoader: Invalid T64 signature\n");
        free(out_t64->data);
        out_t64->data = NULL;
        return false;
    }

    printf("CommodoreLoader: Opened T64: %zu bytes\n", out_t64->data_size);
    return true;
}

bool commodore_t64_read_directory(const commodore_t64_t* t64, commodore_t64_directory_t* out_dir) {
    if (!t64 || !t64->data || !out_dir) return false;
    memset(out_dir, 0, sizeof(*out_dir));

    const uint8_t* hdr = t64->data;

    // Header layout:
    //   0x00-0x1F: Signature (32 bytes)
    //   0x20-0x21: Version (LE)
    //   0x22-0x23: Max entries (LE)
    //   0x24-0x25: Used entries (LE)
    //   0x26-0x27: Reserved
    //   0x28-0x3F: Tape name (24 bytes)
    out_dir->version = read_le16(hdr + 0x20);
    out_dir->max_entries = read_le16(hdr + 0x22);
    out_dir->used_entries = read_le16(hdr + 0x24);

    memcpy(out_dir->tape_name, hdr + 0x28, 24);
    out_dir->tape_name[24] = '\0';
    trim_petscii(out_dir->tape_name, 24);

    // Read directory entries (each 32 bytes starting at offset 0x40)
    int count = 0;
    int max = out_dir->max_entries;
    if (max > T64_MAX_ENTRIES) max = T64_MAX_ENTRIES;

    for (int i = 0; i < max && count < T64_MAX_ENTRIES; i++) {
        size_t entry_offset = T64_HEADER_SIZE + i * T64_ENTRY_SIZE;
        if (entry_offset + T64_ENTRY_SIZE > t64->data_size) break;

        const uint8_t* e = t64->data + entry_offset;

        uint8_t c64s_type = e[0];
        uint8_t file_type = e[1];
        uint16_t start = read_le16(e + 2);
        uint16_t end   = read_le16(e + 4);
        // e[6-7] unused
        uint32_t data_off = read_le32(e + 8);

        // Skip empty entries
        if (c64s_type == 0) continue;

        commodore_t64_entry_t* te = &out_dir->entries[count];
        te->c64s_file_type = c64s_type;
        te->file_type = file_type;
        te->start_addr = start;
        te->end_addr = end;
        te->data_offset = data_off;
        te->data_size = (end > start) ? (end - start) : 0;

        memcpy(te->filename, e + 16, 16);
        te->filename[16] = '\0';
        trim_petscii(te->filename, 16);

        count++;
    }

    out_dir->count = count;
    printf("CommodoreLoader: T64 directory: \"%s\", %d entries\n",
           out_dir->tape_name, count);
    return true;
}

bool commodore_t64_extract_file(const commodore_t64_t* t64, int entry_idx, commodore_prg_t* out_prg) {
    if (!t64 || !out_prg) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    commodore_t64_directory_t dir;
    if (!commodore_t64_read_directory(t64, &dir)) return false;
    if (entry_idx < 0 || entry_idx >= dir.count) return false;

    const commodore_t64_entry_t* entry = &dir.entries[entry_idx];

    if (entry->data_size == 0 || entry->data_offset == 0) {
        printf("CommodoreLoader: T64 entry %d has no data\n", entry_idx);
        return false;
    }

    if (entry->data_offset + entry->data_size > t64->data_size) {
        printf("CommodoreLoader: T64 entry %d data extends beyond file\n", entry_idx);
        return false;
    }

    out_prg->load_addr = entry->start_addr;
    out_prg->end_addr = entry->end_addr;
    out_prg->data_size = entry->data_size;
    out_prg->data = (uint8_t*)malloc(entry->data_size);
    if (!out_prg->data) return false;

    memcpy(out_prg->data, t64->data + entry->data_offset, entry->data_size);

    printf("CommodoreLoader: T64 extracted \"%s\": load=$%04X size=%u\n",
           entry->filename, entry->start_addr, entry->data_size);
    return true;
}

bool commodore_t64_extract_first_prg(const commodore_t64_t* t64, commodore_prg_t* out_prg) {
    if (!t64 || !out_prg) return false;

    commodore_t64_directory_t dir;
    if (!commodore_t64_read_directory(t64, &dir)) return false;

    for (int i = 0; i < dir.count; i++) {
        if (dir.entries[i].c64s_file_type == 1 && dir.entries[i].data_size > 0) {
            return commodore_t64_extract_file(t64, i, out_prg);
        }
    }

    printf("CommodoreLoader: T64 no valid entries found\n");
    return false;
}

void commodore_t64_close(commodore_t64_t* t64) {
    if (t64) {
        if (t64->owns_data && t64->data) {
            free(t64->data);
        }
        memset(t64, 0, sizeof(*t64));
    }
}

// ============================================================================
// TAP Format Implementation (Header/Identification Only)
// ============================================================================

bool commodore_tap_read_header(const char* filepath, commodore_tap_header_t* out_header) {
    if (!filepath || !out_header) return false;
    memset(out_header, 0, sizeof(*out_header));

    FILE* f = fopen(filepath, "rb");
    if (!f) return false;

    uint8_t raw[20];
    if (fread(raw, 1, 20, f) != 20) {
        fclose(f);
        return false;
    }
    fclose(f);

    memcpy(out_header->signature, raw, 12);
    out_header->version = raw[12];
    out_header->platform = raw[13];
    out_header->video_standard = raw[14];
    out_header->reserved = raw[15];
    out_header->data_size = read_le32(raw + 16);

    // Validate signature
    if (memcmp(raw, "C64-TAPE-RAW", 12) != 0 &&
        memcmp(raw, "C16-TAPE-RAW", 12) != 0) {
        printf("CommodoreLoader: Invalid TAP signature: %.12s\n", raw);
        return false;
    }

    printf("CommodoreLoader: TAP: sig=%.12s ver=%d platform=%d std=%d size=%u\n",
           out_header->signature, out_header->version, out_header->platform,
           out_header->video_standard, out_header->data_size);
    return true;
}

int commodore_tap_identify_platform(const char* filepath) {
    commodore_tap_header_t hdr;
    if (!commodore_tap_read_header(filepath, &hdr)) return -1;

    if (memcmp(hdr.signature, "C64-TAPE-RAW", 12) == 0) {
        // Platform byte: 0=C64, 1=VIC-20
        return hdr.platform;
    }
    if (memcmp(hdr.signature, "C16-TAPE-RAW", 12) == 0) {
        return 2;  // C16/Plus4
    }
    return -1;
}

// ============================================================================
// CRT Format Implementation (Header Only)
// ============================================================================

bool commodore_crt_read_header(const char* filepath, commodore_crt_header_t* out_header) {
    if (!filepath || !out_header) return false;
    memset(out_header, 0, sizeof(*out_header));

    FILE* f = fopen(filepath, "rb");
    if (!f) return false;

    uint8_t raw[64];
    if (fread(raw, 1, 64, f) != 64) {
        fclose(f);
        return false;
    }
    fclose(f);

    // Validate signature
    if (memcmp(raw, "C64 CARTRIDGE   ", 16) != 0) {
        printf("CommodoreLoader: Invalid CRT signature\n");
        return false;
    }

    memcpy(out_header->signature, raw, 16);
    out_header->header_length = read_be32(raw + 16);
    out_header->version = read_be16(raw + 20);
    out_header->hardware_type = read_be16(raw + 22);
    out_header->exrom = raw[24];
    out_header->game = raw[25];
    memcpy(out_header->reserved, raw + 26, 6);
    memcpy(out_header->name, raw + 32, 32);
    out_header->name[31] = '\0';

    printf("CommodoreLoader: CRT: \"%s\" hw_type=%d EXROM=%d GAME=%d\n",
           out_header->name, out_header->hardware_type,
           out_header->exrom, out_header->game);
    return true;
}

// ============================================================================
// LNX (Lynx Archive) Format Implementation
// ============================================================================

/**
 * Lynx archive format (created by cbmconvert, Will Corley's original, etc.):
 *
 * 1. BASIC dissolve stub — a short C64/VIC-20 BASIC program that prints
 *    "USE LYNX TO DISSOLVE THIS FILE".  Ends with \0\0\0\r pattern.
 * 2. Directory header:
 *      " <block_count>  <signature containing 'LYNX'>\r <file_count> \r"
 *    where block_count = number of 254-byte blocks the header occupies,
 *    file_count = number of archived files.
 * 3. Per-file directory entry (repeated file_count times):
 *      <filename up to 16 chars>\r  <blocks>\r <type_char>\r  <last_block_len>\r
 * 4. Data section starts at byte offset (block_count × 254).
 *    Files are stored sequentially, each occupying (blocks × 254) bytes.
 *    The actual useful data in a file is: (blocks-1)*254 + last_block_len bytes
 *    (for blocks > 0); formula: last_block_len + blocks*254 - 255.
 *
 * Reference: cbmconvert lynx.c by Marko Mäkelä.
 */

bool commodore_lynx_open(const char* filepath, commodore_lynx_t* out_lynx) {
    if (!filepath || !out_lynx) return false;
    memset(out_lynx, 0, sizeof(*out_lynx));

    out_lynx->data = read_entire_file(filepath, &out_lynx->data_size);
    if (!out_lynx->data) {
        printf("CommodoreLoader: LNX: Failed to read file: %s\n", filepath);
        return false;
    }
    out_lynx->owns_data = true;

    // Quick sanity: needs to be large enough for a minimal header
    if (out_lynx->data_size < 100) {
        printf("CommodoreLoader: LNX: File too small (%zu bytes)\n", out_lynx->data_size);
        free(out_lynx->data);
        out_lynx->data = NULL;
        return false;
    }

    printf("CommodoreLoader: Opened LNX archive: %zu bytes\n", out_lynx->data_size);
    return true;
}

bool commodore_lynx_read_directory(const commodore_lynx_t* lynx, commodore_lynx_directory_t* out_dir) {
    if (!lynx || !lynx->data || !out_dir) return false;
    memset(out_dir, 0, sizeof(*out_dir));

    const uint8_t* buf = lynx->data;
    size_t size = lynx->data_size;

    // Step 1: Skip the BASIC dissolve header by finding \0\0\0\r pattern
    size_t pos = 4;
    size_t max_header = (size < LNX_MAX_BASIC_LENGTH) ? size : LNX_MAX_BASIC_LENGTH;
    bool found_header_end = false;
    for (; pos < max_header; pos++) {
        if (buf[pos-4] == 0x00 && buf[pos-3] == 0x00 &&
            buf[pos-2] == 0x00 && buf[pos-1] == 0x0D) {
            found_header_end = true;
            break;
        }
    }
    if (!found_header_end) {
        // Try without BASIC header (some archives may not have one)
        pos = 0;
    }

    // Step 2: Parse " <blkcount>  <signature>\r <fcount> \r"
    // Read block count
    while (pos < size && buf[pos] == ' ') pos++;
    unsigned blkcount = 0;
    while (pos < size && buf[pos] >= '0' && buf[pos] <= '9') {
        blkcount = blkcount * 10 + (buf[pos] - '0');
        pos++;
    }
    if (blkcount == 0) {
        printf("CommodoreLoader: LNX: Invalid block count\n");
        return false;
    }

    // Skip spaces and signature text until CR
    while (pos < size && buf[pos] != 0x0D) pos++;
    if (pos >= size) return false;

    // Verify "LYNX" appeared in the signature area
    bool has_lynx_sig = false;
    {
        size_t scan_start = (pos > 30) ? pos - 30 : 0;
        for (size_t s = scan_start; s + 3 < pos; s++) {
            if (buf[s] == 'L' && buf[s+1] == 'Y' && buf[s+2] == 'N' && buf[s+3] == 'X') {
                has_lynx_sig = true;
                break;
            }
        }
    }
    if (!has_lynx_sig) {
        printf("CommodoreLoader: LNX: LYNX signature not found\n");
        return false;
    }
    pos++; // skip CR after signature

    // Read file count
    while (pos < size && (buf[pos] == ' ' || buf[pos] == 0x0D)) pos++;
    unsigned fcount = 0;
    while (pos < size && buf[pos] >= '0' && buf[pos] <= '9') {
        fcount = fcount * 10 + (buf[pos] - '0');
        pos++;
    }
    // Skip trailing spaces/CRs
    while (pos < size && (buf[pos] == ' ' || buf[pos] == 0x0D)) pos++;

    if (fcount == 0 || fcount > LNX_MAX_FILES) {
        printf("CommodoreLoader: LNX: Invalid file count: %u\n", fcount);
        return false;
    }

    out_dir->header_blocks = blkcount;
    out_dir->file_count = fcount;

    printf("CommodoreLoader: LNX: header_blocks=%u, files=%u, data_start=%u\n",
           blkcount, fcount, blkcount * 254);

    // Step 3: Parse per-file entries
    // Format: filename\r blocks\r type_char\r last_block_len\r
    size_t archive_pos = (size_t)blkcount * 254;  // data section start

    for (unsigned f = 0; f < fcount && pos < size; f++) {
        commodore_lynx_entry_t* entry = &out_dir->entries[f];

        // Read filename (up to 16 chars, CR-terminated)
        int name_len = 0;
        while (pos < size && buf[pos] != 0x0D && name_len < 16) {
            entry->filename[name_len++] = (char)buf[pos++];
        }
        entry->filename[name_len] = '\0';
        // Trim trailing $A0 padding from filename
        for (int t = name_len - 1; t >= 0; t--) {
            if ((uint8_t)entry->filename[t] == 0xA0 || entry->filename[t] == ' ')
                entry->filename[t] = '\0';
            else
                break;
        }
        if (pos < size && buf[pos] == 0x0D) pos++; // skip CR

        // Read block count
        while (pos < size && buf[pos] == ' ') pos++;
        unsigned blocks = 0;
        while (pos < size && buf[pos] >= '0' && buf[pos] <= '9') {
            blocks = blocks * 10 + (buf[pos] - '0');
            pos++;
        }
        while (pos < size && (buf[pos] == ' ' || buf[pos] == 0x0D)) pos++;

        // Read file type character
        char ftype = 'P';
        if (pos < size && buf[pos] != 0x0D) {
            ftype = (char)buf[pos++];
        }
        if (pos < size && buf[pos] == 0x0D) pos++;

        // Read last block length (or record length for REL)
        while (pos < size && buf[pos] == ' ') pos++;
        unsigned last_len = 0;
        while (pos < size && buf[pos] >= '0' && buf[pos] <= '9') {
            last_len = last_len * 10 + (buf[pos] - '0');
            pos++;
        }
        while (pos < size && (buf[pos] == ' ' || buf[pos] == 0x0D)) pos++;

        entry->file_type = ftype;
        entry->blocks = blocks;
        entry->last_block_len = last_len;
        entry->data_offset = archive_pos;

        // Compute actual data length: (blocks-1)*254 + last_block_len
        // Which is: last_block_len + blocks*254 - 255
        if (blocks > 0 && last_len > 0) {
            entry->data_length = (size_t)last_len + (size_t)blocks * 254 - 255;
        } else {
            entry->data_length = 0;
        }

        printf("CommodoreLoader: LNX[%u]: \"%s\" type=%c blocks=%u lastlen=%u len=%zu off=%zu\n",
               f, entry->filename, ftype, blocks, last_len,
               entry->data_length, entry->data_offset);

        // Advance archive position by this file's block count
        archive_pos += (size_t)blocks * 254;
    }

    return true;
}

bool commodore_lynx_extract_file(const commodore_lynx_t* lynx,
                                 const commodore_lynx_directory_t* dir,
                                 int entry_idx, commodore_prg_t* out_prg) {
    if (!lynx || !lynx->data || !dir || !out_prg) return false;
    if (entry_idx < 0 || entry_idx >= (int)dir->file_count) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    const commodore_lynx_entry_t* entry = &dir->entries[entry_idx];

    if (entry->data_length < 2) {
        printf("CommodoreLoader: LNX: File \"%s\" too small (%zu bytes)\n",
               entry->filename, entry->data_length);
        return false;
    }

    if (entry->data_offset + entry->data_length > lynx->data_size) {
        // Allow truncation for last file (common in Lynx archives)
        size_t avail = lynx->data_size - entry->data_offset;
        if (avail < 2) return false;
        printf("CommodoreLoader: LNX: File \"%s\" truncated (%zu of %zu bytes available)\n",
               entry->filename, avail, entry->data_length);
        return commodore_prg_parse(lynx->data + entry->data_offset, avail, out_prg);
    }

    return commodore_prg_parse(lynx->data + entry->data_offset,
                               entry->data_length, out_prg);
}

bool commodore_lynx_extract_all_prgs(const commodore_lynx_t* lynx,
                                     commodore_prg_t* out_prgs, int max_prgs,
                                     int* out_count) {
    if (!lynx || !out_prgs || !out_count) return false;
    *out_count = 0;

    commodore_lynx_directory_t dir;
    if (!commodore_lynx_read_directory(lynx, &dir)) return false;

    for (unsigned i = 0; i < dir.file_count && *out_count < max_prgs; i++) {
        if (dir.entries[i].file_type != 'P') continue;  // Only extract PRG files
        if (dir.entries[i].data_length < 2) continue;    // Skip empty/tiny files

        commodore_prg_t prg = {};
        if (commodore_lynx_extract_file(lynx, &dir, (int)i, &prg)) {
            out_prgs[*out_count] = prg;
            (*out_count)++;
            printf("CommodoreLoader: LNX: Extracted PRG \"%s\": $%04X-$%04X (%zu bytes)\n",
                   dir.entries[i].filename, prg.load_addr, prg.end_addr, prg.data_size);
        }
    }

    printf("CommodoreLoader: LNX: Extracted %d PRG files\n", *out_count);
    return *out_count > 0;
}

void commodore_lynx_close(commodore_lynx_t* lynx) {
    if (lynx) {
        if (lynx->owns_data && lynx->data) {
            free(lynx->data);
        }
        memset(lynx, 0, sizeof(*lynx));
    }
}

// ============================================================================
// Unified Load Function
// ============================================================================

const char* commodore_load_type_name(commodore_load_type_t type) {
    switch (type) {
        case COMMODORE_LOAD_PRG:  return "PRG";
        case COMMODORE_LOAD_D64:  return "D64";
        case COMMODORE_LOAD_T64:  return "T64";
        case COMMODORE_LOAD_TAP:  return "TAP";
        case COMMODORE_LOAD_CRT:  return "CRT";
        case COMMODORE_LOAD_BIN:  return "BIN";
        case COMMODORE_LOAD_LNX:  return "LNX";
        case COMMODORE_LOAD_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

bool commodore_load_file(const char* filepath, commodore_load_result_t* out) {
    if (!filepath || !out) return false;
    memset(out, 0, sizeof(*out));
    out->type = COMMODORE_LOAD_ERROR;

    const char* ext = strrchr(filepath, '.');
    if (!ext) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "No file extension found: %s", filepath);
        return false;
    }

    // === PRG ===
    if (ext_match(ext, ".prg")) {
        if (commodore_prg_load(filepath, &out->prg)) {
            out->type = COMMODORE_LOAD_PRG;
            printf("CommodoreLoader: Loaded PRG: $%04X-$%04X (%zu bytes)\n",
                   out->prg.load_addr, out->prg.end_addr, out->prg.data_size);
            return true;
        }
        snprintf(out->error_msg, sizeof(out->error_msg), "Failed to load PRG: %s", filepath);
        return false;
    }

    // === LNX (Lynx archive — multi-file CBM container) ===
    if (ext_match(ext, ".lnx")) {
        commodore_lynx_t lynx;
        if (commodore_lynx_open(filepath, &lynx)) {
            int count = 0;
            if (commodore_lynx_extract_all_prgs(&lynx, out->lynx_files,
                                                COMMODORE_LOAD_MAX_LNX_FILES, &count)) {
                out->lynx_file_count = count;
                out->type = COMMODORE_LOAD_LNX;
                commodore_lynx_close(&lynx);
                return true;
            }
            commodore_lynx_close(&lynx);
            snprintf(out->error_msg, sizeof(out->error_msg),
                     "LNX opened but no PRG files found: %s", filepath);
        } else {
            snprintf(out->error_msg, sizeof(out->error_msg),
                     "Failed to open LNX archive: %s", filepath);
        }
        return false;
    }

    // === D64 ===
    if (ext_match(ext, ".d64")) {
        commodore_d64_t d64;
        if (commodore_d64_open(filepath, &d64)) {
            if (commodore_d64_extract_first_prg(&d64, &out->prg)) {
                out->type = COMMODORE_LOAD_D64;
                commodore_d64_close(&d64);
                return true;
            }
            commodore_d64_close(&d64);
            snprintf(out->error_msg, sizeof(out->error_msg),
                     "D64 opened but no PRG found: %s", filepath);
        } else {
            snprintf(out->error_msg, sizeof(out->error_msg),
                     "Failed to open D64: %s", filepath);
        }
        return false;
    }

    // === T64 ===
    if (ext_match(ext, ".t64")) {
        commodore_t64_t t64;
        if (commodore_t64_open(filepath, &t64)) {
            if (commodore_t64_extract_first_prg(&t64, &out->prg)) {
                out->type = COMMODORE_LOAD_T64;
                commodore_t64_close(&t64);
                return true;
            }
            commodore_t64_close(&t64);
            snprintf(out->error_msg, sizeof(out->error_msg),
                     "T64 opened but no entry found: %s", filepath);
        } else {
            snprintf(out->error_msg, sizeof(out->error_msg),
                     "Failed to open T64: %s", filepath);
        }
        return false;
    }

    // === TAP (identification only — decoding requires cycle-accurate tape emulation) ===
    if (ext_match(ext, ".tap")) {
        if (commodore_tap_read_header(filepath, &out->tap_header)) {
            out->type = COMMODORE_LOAD_TAP;
            return true;
        }
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Failed to read TAP header: %s", filepath);
        return false;
    }

    // === CRT (header only — chip loading is system-specific) ===
    if (ext_match(ext, ".crt")) {
        if (commodore_crt_read_header(filepath, &out->crt_header)) {
            out->type = COMMODORE_LOAD_CRT;
            return true;
        }
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Failed to read CRT header: %s", filepath);
        return false;
    }

    // === BIN (raw binary, no header) ===
    if (ext_match(ext, ".bin")) {
        uint8_t* data = NULL;
        size_t data_size = 0;
        if (commodore_bin_load(filepath, &data, &data_size)) {
            out->prg.data = data;
            out->prg.data_size = data_size;
            out->prg.load_addr = 0;
            out->prg.end_addr = (uint16_t)(data_size > 0xFFFF ? 0xFFFF : data_size);
            out->type = COMMODORE_LOAD_BIN;
            return true;
        }
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Failed to load BIN: %s", filepath);
        return false;
    }

    snprintf(out->error_msg, sizeof(out->error_msg),
             "Unsupported Commodore file extension: %s", ext);
    return false;
}

void commodore_load_result_free(commodore_load_result_t* result) {
    if (!result) return;
    commodore_prg_free(&result->prg);
    // Free all Lynx extracted PRGs
    for (int i = 0; i < result->lynx_file_count; i++) {
        commodore_prg_free(&result->lynx_files[i]);
    }
    memset(result, 0, sizeof(*result));
}
