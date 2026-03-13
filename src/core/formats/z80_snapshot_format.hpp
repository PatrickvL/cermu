#pragma once

/**
 * Z80 Snapshot Format Handler — ZX Spectrum .z80 Files
 *
 * The .z80 format was created by Gerton Lunter for his Z80 emulator.
 * It is the most widely used Spectrum snapshot format, supporting
 * v1 (48K only), v2 (48K/128K), and v3 (48K/128K/+3) revisions.
 *
 * Key features:
 *   - All Z80 registers stored directly in the header (PC included)
 *   - RAM is optionally compressed (simple run-length encoding)
 *   - v2/v3 use separate compressed memory pages with a page directory
 *
 * Header layout:
 *   v1: 30 bytes.  If header[12]==255, treated as 1 (back-compat).
 *       If PC==0 in the v1 header -> actually v2/v3 (extended header follows).
 *
 *   v2 extension: +23 bytes (total 55)
 *   v3 extension: +54 or +55 bytes (total 86 or 87)
 *
 * Compression (v1):
 *   $ED $ED <count> <byte> → repeat <byte> <count> times
 *   End sentinel: $00 $ED $ED $00  (only v1)
 *
 * Page format (v2/v3):
 *   2 bytes: compressed page length (or $FFFF = 16384 for uncompressed)
 *   1 byte:  page number
 *   N bytes: (compressed) page data
 *
 * Page numbers:
 *   48K: page 4 → $8000-$BFFF, page 5 → $C000-$FFFF, page 8 → $4000-$7FFF
 *   128K: pages 3-10 → RAM banks 0-7
 */

#include "core/formats/format_handler.hpp"

// ============================================================================
// Z80 Snapshot Header
// ============================================================================

struct z80_snapshot_header_t {
    // V1 header (30 bytes)
    uint16_t af;
    uint16_t bc;
    uint16_t hl;
    uint16_t pc;         // 0 means v2/v3 (extended header present)
    uint16_t sp;
    uint8_t  i_reg;
    uint8_t  r_reg;      // Bit 7 replaced by header byte 12 bit 0
    uint8_t  flags12;    // Bits 0: R bit 7, 1-3: border, 4: basic SamROM, 5: data compressed (v1), 6-7: unused
    uint16_t de;
    uint16_t bc_prime;
    uint16_t de_prime;
    uint16_t hl_prime;
    uint16_t af_prime;
    uint16_t iy;
    uint16_t ix;
    uint8_t  iff1;
    uint8_t  iff2;
    uint8_t  flags29;    // Bits 0-1: IM mode, 2: Issue 2 emulation, 3: double int freq, 4-5: video sync, 6-7: joystick

    // V2/V3 extension
    uint8_t  version;    // Derived: 1, 2, or 3
    uint16_t ext_length; // Additional header length (23=v2, 54/55=v3)
    uint8_t  hw_mode;    // Hardware mode (determines 48K vs 128K)
    uint8_t  port_7ffd;  // Last value written to $7FFD (128K banking)
    uint8_t  port_fffd;  // Last value written to $FFFD (128K AY register)
    uint8_t  ay_regs[16]; // AY register values (128K)
    bool     is_128k;    // Derived: true if hardware mode indicates 128K+

    // Derived fields
    uint8_t  border;     // Extracted from flags12 bits 1-3
    uint8_t  im_mode;    // Extracted from flags29 bits 0-1
};

/**
 * Parse a Z80 snapshot header from raw data.
 * @return true on success.
 */
bool z80_snapshot_parse_header(const uint8_t* data, size_t size, z80_snapshot_header_t* out);

/**
 * Decompress Z80 v1 data (ED ED count byte run-length encoding).
 * @param src       Compressed data
 * @param src_size  Size of compressed data
 * @param dst       Output buffer (must be pre-allocated)
 * @param dst_size  Size of output buffer
 * @return Number of bytes written to dst, or 0 on error.
 */
size_t z80_decompress(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size);

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t Z80_SNAPSHOT_FORMAT_DESCRIPTOR;
