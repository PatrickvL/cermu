/**
 * Z80 Snapshot Format Handler — Implementation
 *
 * Supports v1, v2, and v3 .z80 snapshot files.
 * Loads as FORMAT_LOAD_RAW: RAM dump in program.data, z80_snapshot_header_t in metadata.
 *
 * For 48K snapshots: program.data = 48KB ($4000-$FFFF)
 * For 128K snapshots: program.data = 128KB (banks 0-7 concatenated, 16KB each)
 */

#include "core/cermu.hpp"
#include "core/formats/z80_snapshot_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Decompression (ED ED count byte run-length encoding)
// ============================================================================

size_t z80_decompress(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size) {
    size_t si = 0, di = 0;

    while (si < src_size && di < dst_size) {
        if (si + 3 < src_size && src[si] == 0xED && src[si + 1] == 0xED) {
            // Run: ED ED count byte
            uint8_t count = src[si + 2];
            uint8_t value = src[si + 3];
            si += 4;
            for (int j = 0; j < count && di < dst_size; ++j)
                dst[di++] = value;
        } else {
            dst[di++] = src[si++];
        }
    }
    return di;
}

// ============================================================================
// Header Parsing
// ============================================================================

bool z80_snapshot_parse_header(const uint8_t* data, size_t size, z80_snapshot_header_t* out) {
    if (!data || !out || size < 30) return false;
    std::memset(out, 0, sizeof(*out));

    // V1 header: 30 bytes
    out->af       = (uint16_t)(data[0] << 8) | data[1];       // A then F (big-endian!)
    out->bc       = format_read_le16(data + 2);
    out->hl       = format_read_le16(data + 4);
    out->pc       = format_read_le16(data + 6);
    out->sp       = format_read_le16(data + 8);
    out->i_reg    = data[10];
    out->r_reg    = data[11] & 0x7F;
    out->flags12  = data[12];
    if (out->flags12 == 255) out->flags12 = 1; // Back-compat

    // R bit 7 from flags12 bit 0
    out->r_reg |= (out->flags12 & 0x01) << 7;
    out->border   = (out->flags12 >> 1) & 0x07;

    out->de       = format_read_le16(data + 13);
    out->bc_prime = format_read_le16(data + 15);
    out->de_prime = format_read_le16(data + 17);
    out->hl_prime = format_read_le16(data + 19);
    // AF' is also stored big-endian (A' then F')
    out->af_prime = (uint16_t)(data[21] << 8) | data[22];
    out->iy       = format_read_le16(data + 23);
    out->ix       = format_read_le16(data + 25);
    out->iff1     = data[27] ? 1 : 0;
    out->iff2     = data[28] ? 1 : 0;
    out->flags29  = data[29];
    out->im_mode  = out->flags29 & 0x03;

    // Determine version
    if (out->pc != 0) {
        // V1 format — PC is valid
        out->version = 1;
        out->ext_length = 0;
        out->hw_mode = 0;
        out->is_128k = false;
    } else {
        // V2 or V3: extended header
        if (size < 32) return false;
        out->ext_length = format_read_le16(data + 30);

        if (out->ext_length == 23) {
            out->version = 2;
        } else if (out->ext_length == 54 || out->ext_length == 55) {
            out->version = 3;
        } else {
            // Unknown extension length — try to interpret anyway
            out->version = (out->ext_length >= 54) ? 3 : 2;
        }

        size_t ext_start = 32;
        if (ext_start + out->ext_length > size) return false;

        out->pc = format_read_le16(data + ext_start);
        out->hw_mode = data[ext_start + 2];

        // Hardware mode interpretation:
        // V2: 0=48K, 1=48K+IF1, 2=SamRam, 3=128K, 4=128K+IF1
        // V3: 0=48K, 1=48K+IF1, 2=SamRam, 3=48K+MGT, 4=128K, 5=128K+IF1, 6=128K+MGT
        if (out->version == 2) {
            out->is_128k = (out->hw_mode >= 3);
        } else {
            out->is_128k = (out->hw_mode >= 4);
        }

        // 128K specific fields
        if (out->is_128k) {
            out->port_7ffd = data[ext_start + 3];

            // AY register state
            out->port_fffd = data[ext_start + 5];
            if (ext_start + 6 + 16 <= size)
                std::memcpy(out->ay_regs, data + ext_start + 6, 16);
        } else {
            out->port_7ffd = data[ext_start + 3];
        }
    }

    return true;
}

// ============================================================================
// V2/V3 Page Loading
// ============================================================================

/**
 * Map a .z80 page number to a RAM bank index (0-7).
 * For 48K:  page 4 → bank 2 ($8000), page 5 → bank 0 ($C000), page 8 → bank 5 ($4000)
 * For 128K: pages 3-10 → banks 0-7
 * Returns -1 for unsupported/ROM pages.
 */
static int z80_page_to_bank(int page_num, bool is_128k) {
    if (is_128k) {
        // Pages 3-10 map to banks 0-7
        if (page_num >= 3 && page_num <= 10)
            return page_num - 3;
        return -1;
    } else {
        // 48K: specific page mapping
        switch (page_num) {
            case 4: return 2;  // $8000-$BFFF
            case 5: return 0;  // $C000-$FFFF
            case 8: return 5;  // $4000-$7FFF (screen)
            default: return -1;
        }
    }
}

/**
 * Map a 48K bank index to address offset within the 48K RAM region ($0000-$BFFF → 0-based).
 * bank 5 → offset $0000 ($4000-$7FFF)
 * bank 2 → offset $4000 ($8000-$BFFF)
 * bank 0 → offset $8000 ($C000-$FFFF)
 */
static int z80_bank_to_48k_offset(int bank) {
    switch (bank) {
        case 5: return 0x0000;   // $4000
        case 2: return 0x4000;   // $8000
        case 0: return 0x8000;   // $C000
        default: return -1;
    }
}

// ============================================================================
// Format Identification
// ============================================================================

static float z80_snapshot_identify(const uint8_t* data, size_t file_size,
                                   const char* extension) {
    bool ext_match = extension && format_ext_match(extension, ".z80");

    if (data && file_size >= 30) {
        // Basic header validation
        uint8_t im_mode = data[29] & 0x03;
        uint8_t flags12 = data[12];
        if (flags12 == 255) flags12 = 1;
        uint8_t border = (flags12 >> 1) & 0x07;

        bool valid_im = (im_mode <= 2);
        bool valid_border = (border <= 7);  // Always true with 3-bit mask, but explicit

        // Check for v2/v3 marker (PC==0 in v1 header)
        uint16_t pc = format_read_le16(data + 6);
        bool valid_ext = true;
        if (pc == 0 && file_size >= 32) {
            uint16_t ext_len = format_read_le16(data + 30);
            valid_ext = (ext_len == 23 || ext_len == 54 || ext_len == 55);
        }

        if (ext_match && valid_im && valid_border && valid_ext)
            return 0.92f;
        if (valid_im && valid_ext && file_size > 100)
            return 0.3f;
    }

    if (ext_match) return 0.6f;
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool z80_snapshot_load(const uint8_t* data, size_t size,
                              format_load_result_t* out) {
    z80_snapshot_header_t header;
    if (!z80_snapshot_parse_header(data, size, &header)) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Z80: Failed to parse header");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    if (header.is_128k) {
        // 128K: 8 banks × 16KB = 128KB output
        constexpr size_t BANK_SIZE = 16384;
        constexpr size_t TOTAL_RAM = 8 * BANK_SIZE;

        out->program.data = static_cast<uint8_t*>(std::calloc(1, TOTAL_RAM));
        if (!out->program.data) {
            snprintf(out->error_msg, sizeof(out->error_msg), "Z80: out of memory");
            out->type = FORMAT_LOAD_ERROR;
            return false;
        }

        // Parse v2/v3 pages
        size_t pos = 32 + header.ext_length;
        while (pos + 3 <= size) {
            uint16_t page_len = format_read_le16(data + pos);
            uint8_t page_num = data[pos + 2];
            pos += 3;

            int bank = z80_page_to_bank(page_num, true);
            if (bank < 0 || bank > 7) {
                // Skip unknown page
                if (page_len == 0xFFFF) pos += BANK_SIZE;
                else pos += page_len;
                continue;
            }

            uint8_t* dst = out->program.data + bank * BANK_SIZE;
            if (page_len == 0xFFFF) {
                // Uncompressed
                if (pos + BANK_SIZE > size) break;
                std::memcpy(dst, data + pos, BANK_SIZE);
                pos += BANK_SIZE;
            } else {
                if (pos + page_len > size) break;
                z80_decompress(data + pos, page_len, dst, BANK_SIZE);
                pos += page_len;
            }
        }

        out->program.data_size = TOTAL_RAM;
        out->program.load_addr = 0x0000;  // Banks 0-7 sequentially
        out->program.end_addr  = 0xFFFF;

    } else if (header.version == 1) {
        // V1: 48K data starts at offset 30
        constexpr size_t RAM_48K = 49152;
        out->program.data = static_cast<uint8_t*>(std::calloc(1, RAM_48K));
        if (!out->program.data) {
            snprintf(out->error_msg, sizeof(out->error_msg), "Z80: out of memory");
            out->type = FORMAT_LOAD_ERROR;
            return false;
        }

        const uint8_t* src = data + 30;
        size_t src_size = size - 30;

        bool compressed = (header.flags12 & 0x20) != 0;
        if (compressed) {
            z80_decompress(src, src_size, out->program.data, RAM_48K);
        } else {
            size_t copy_size = (src_size < RAM_48K) ? src_size : RAM_48K;
            std::memcpy(out->program.data, src, copy_size);
        }

        out->program.data_size = RAM_48K;
        out->program.load_addr = 0x4000;
        out->program.end_addr  = 0xFFFF;

    } else {
        // V2/V3 48K: pages
        constexpr size_t RAM_48K = 49152;
        out->program.data = static_cast<uint8_t*>(std::calloc(1, RAM_48K));
        if (!out->program.data) {
            snprintf(out->error_msg, sizeof(out->error_msg), "Z80: out of memory");
            out->type = FORMAT_LOAD_ERROR;
            return false;
        }

        constexpr size_t BANK_SIZE = 16384;
        size_t pos = 32 + header.ext_length;

        while (pos + 3 <= size) {
            uint16_t page_len = format_read_le16(data + pos);
            uint8_t page_num = data[pos + 2];
            pos += 3;

            int bank = z80_page_to_bank(page_num, false);
            if (bank < 0) {
                // Skip unknown page
                if (page_len == 0xFFFF) pos += BANK_SIZE;
                else pos += page_len;
                continue;
            }

            int offset = z80_bank_to_48k_offset(bank);
            if (offset < 0) {
                if (page_len == 0xFFFF) pos += BANK_SIZE;
                else pos += page_len;
                continue;
            }

            uint8_t* dst = out->program.data + offset;
            if (page_len == 0xFFFF) {
                if (pos + BANK_SIZE > size) break;
                std::memcpy(dst, data + pos, BANK_SIZE);
                pos += BANK_SIZE;
            } else {
                if (pos + page_len > size) break;
                z80_decompress(data + pos, page_len, dst, BANK_SIZE);
                pos += page_len;
            }
        }

        out->program.data_size = RAM_48K;
        out->program.load_addr = 0x4000;
        out->program.end_addr  = 0xFFFF;
    }

    // Store header in metadata
    static_assert(sizeof(z80_snapshot_header_t) <= FORMAT_METADATA_MAX_SIZE,
                  "z80_snapshot_header_t must fit in metadata blob");
    std::memcpy(out->metadata, &header, sizeof(header));
    out->metadata_size = sizeof(header);

    out->type = FORMAT_LOAD_RAW;
    log_info("Z80: Parsed v%d %s snapshot (PC=$%04X, SP=$%04X, IM=%d, border=%d)\n",
           header.version, header.is_128k ? "128K" : "48K",
           header.pc, header.sp, header.im_mode, header.border);
    return true;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* z80_snapshot_extensions[] = { ".z80", nullptr };

const format_descriptor_t Z80_SNAPSHOT_FORMAT_DESCRIPTOR = {
    "Z80",
    "Z80 Spectrum Snapshot",
    z80_snapshot_extensions,
    FORMAT_CAP_LOADABLE,
    z80_snapshot_identify,
    z80_snapshot_load,
    nullptr,  // list_entries
    nullptr   // extract_entry
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(Z80_SNAPSHOT, &Z80_SNAPSHOT_FORMAT_DESCRIPTOR)
