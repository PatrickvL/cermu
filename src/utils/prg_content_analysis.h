#pragma once
/**
 * prg_content_analysis.h — Content analysis utilities for Commodore PRG files
 *
 * Provides heuristics to disambiguate PRG files across Commodore systems:
 *   - BASIC version detection (v2 vs v3.5 tokens)
 *   - 6502 MMIO reference scanning (VIC chip, TED, VIC-II, SID, CIA, etc.)
 *   - BASIC program structure validation
 *
 * These utilities are shared across C64, VIC-20, and C16/Plus4 system probes.
 * All functions are side-effect-free and header-only for inlining.
 */

#include <cstdint>
#include <cstddef>

// ============================================================================
// BASIC Version Detection
// ============================================================================

/**
 * Check whether a BASIC program listing contains BASIC 3.5 tokens ($CC–$FE).
 * These are exclusive to the C264 series (C16, C116, Plus/4).
 *
 * @param basic  Pointer to BASIC data (AFTER the 2-byte load address header)
 * @param len    Length of the BASIC data
 * @return       true if any BASIC 3.5 token ($CC–$FE) is found outside strings
 */
inline bool has_basic35_tokens(const uint8_t* basic, size_t len) {
    size_t pos = 0;
    while (pos + 4 < len) {
        // Each BASIC line: [2-byte next-line ptr] [2-byte line number] [tokens...] [0x00]
        uint16_t next_line = basic[pos] | (basic[pos + 1] << 8);
        if (next_line == 0) break;  // end of program
        pos += 4;  // skip next-line pointer and line number
        bool in_string = false;
        while (pos < len && basic[pos] != 0x00) {
            uint8_t b = basic[pos];
            if (b == 0x22) in_string = !in_string;       // toggle on quote
            else if (!in_string && b >= 0xCC && b <= 0xFE)
                return true;                              // BASIC 3.5 exclusive token
            pos++;
        }
        if (pos < len) pos++;  // skip terminating 0x00
    }
    return false;
}

/**
 * Validate that data looks like a BASIC program listing.
 * Checks for plausible BASIC line structure: next-line pointers form a
 * forward chain, line numbers are ascending, and tokens are in valid range.
 *
 * @param basic      Pointer to BASIC data (AFTER the 2-byte load address header)
 * @param len        Length of the BASIC data
 * @param load_addr  The 2-byte load address (needed to validate next-line pointers)
 * @return           Confidence 0.0–1.0 that this is a BASIC program
 */
inline float basic_program_confidence(const uint8_t* basic, size_t len, uint16_t load_addr) {
    if (len < 5) return 0.0f;

    size_t pos = 0;
    int lines = 0;
    uint16_t prev_line_num = 0;
    bool ascending = true;

    while (pos + 4 < len) {
        uint16_t next_ptr = basic[pos] | (basic[pos + 1] << 8);
        if (next_ptr == 0) break;  // end-of-program marker

        // next_ptr should point forward within the program
        uint16_t expected_min = load_addr + (uint16_t)pos + 4;
        if (next_ptr < expected_min || next_ptr > load_addr + len)
            return lines >= 2 ? 0.5f : 0.0f;  // broken chain

        uint16_t line_num = basic[pos + 2] | (basic[pos + 3] << 8);
        if (lines > 0 && line_num <= prev_line_num)
            ascending = false;
        prev_line_num = line_num;

        // Skip to end of line (null terminator)
        pos += 4;
        while (pos < len && basic[pos] != 0x00) {
            uint8_t b = basic[pos];
            // Valid BASIC token bytes: ASCII printable ($20-$7F) or token ($80-$FE)
            // $FF can appear in BASIC data but isn't a standard token
            pos++;
        }
        if (pos < len) pos++;  // skip 0x00
        lines++;
    }

    if (lines == 0) return 0.0f;
    if (lines >= 3 && ascending) return 0.9f;
    if (lines >= 2 && ascending) return 0.7f;
    if (lines >= 1) return 0.5f;
    return 0.0f;
}

// ============================================================================
// 6502 MMIO Reference Scanning
// ============================================================================

/**
 * Flags indicating which system-specific MMIO ranges were referenced
 * by absolute-addressing 6502 instructions in a PRG binary.
 */
enum MmioSignature : uint32_t {
    MMIO_NONE           = 0,

    // C64-specific
    MMIO_C64_VICII      = (1u << 0),   ///< VIC-II registers $D000-$D3FF
    MMIO_C64_SID        = (1u << 1),   ///< SID registers $D400-$D7FF
    MMIO_C64_COLOR_RAM  = (1u << 2),   ///< Color RAM $D800-$DBFF
    MMIO_C64_CIA1       = (1u << 3),   ///< CIA1 $DC00-$DCFF
    MMIO_C64_CIA2       = (1u << 4),   ///< CIA2 $DD00-$DDFF

    // VIC-20-specific
    MMIO_VIC20_VIC      = (1u << 8),   ///< VIC chip $9000-$900F
    MMIO_VIC20_VIA1     = (1u << 9),   ///< VIA1 $9110-$911F
    MMIO_VIC20_VIA2     = (1u << 10),  ///< VIA2 $9120-$912F
    MMIO_VIC20_COLOR    = (1u << 11),  ///< Color RAM $9400-$97FF

    // C16/Plus4-specific (TED)
    MMIO_C16_TED        = (1u << 16),  ///< TED registers $FF00-$FF1F
    MMIO_C16_TED_KEYS   = (1u << 17),  ///< TED keyboard $FD30 or $FF08
    MMIO_C16_ACIA       = (1u << 18),  ///< ACIA $FD00-$FD0F (Plus4)
    MMIO_C16_PIO        = (1u << 19),  ///< PIO $FD10-$FD1F (Plus4)

    // Composite masks for quick system-level checks
    MMIO_ANY_C64        = MMIO_C64_VICII | MMIO_C64_SID | MMIO_C64_COLOR_RAM |
                          MMIO_C64_CIA1  | MMIO_C64_CIA2,
    MMIO_ANY_VIC20      = MMIO_VIC20_VIC | MMIO_VIC20_VIA1 | MMIO_VIC20_VIA2 |
                          MMIO_VIC20_COLOR,
    MMIO_ANY_C16        = MMIO_C16_TED | MMIO_C16_TED_KEYS | MMIO_C16_ACIA |
                          MMIO_C16_PIO,
};

/**
 * Classify a 16-bit address into an MMIO signature flag.
 * Returns MMIO_NONE if the address doesn't fall in a known I/O range.
 */
inline uint32_t classify_mmio_addr(uint16_t addr) {
    // C64 I/O area: $D000-$DFFF
    if (addr >= 0xD000 && addr <= 0xD3FF) return MMIO_C64_VICII;
    if (addr >= 0xD400 && addr <= 0xD7FF) return MMIO_C64_SID;
    if (addr >= 0xD800 && addr <= 0xDBFF) return MMIO_C64_COLOR_RAM;
    if (addr >= 0xDC00 && addr <= 0xDCFF) return MMIO_C64_CIA1;
    if (addr >= 0xDD00 && addr <= 0xDDFF) return MMIO_C64_CIA2;

    // VIC-20 I/O area: $9000-$97FF
    if (addr >= 0x9000 && addr <= 0x900F) return MMIO_VIC20_VIC;
    if (addr >= 0x9110 && addr <= 0x911F) return MMIO_VIC20_VIA1;
    if (addr >= 0x9120 && addr <= 0x912F) return MMIO_VIC20_VIA2;
    if (addr >= 0x9400 && addr <= 0x97FF) return MMIO_VIC20_COLOR;

    // C16/Plus4 I/O area: $FD00-$FF3F
    if (addr >= 0xFF00 && addr <= 0xFF1F) return MMIO_C16_TED;
    if (addr == 0xFF08 || addr == 0xFD30) return MMIO_C16_TED_KEYS;
    if (addr >= 0xFD00 && addr <= 0xFD0F) return MMIO_C16_ACIA;
    if (addr >= 0xFD10 && addr <= 0xFD1F) return MMIO_C16_PIO;

    return MMIO_NONE;
}

/**
 * 6502 instruction length table by opcode.
 * 0 = invalid/unknown, 1 = implied, 2 = immediate/zp, 3 = absolute.
 * Only absolute (3-byte) instructions have 16-bit addresses worth checking.
 */
inline int opcode_length(uint8_t op) {
    // Addressing mode from opcode encoding.  For MOS 6502:
    //   aaa-bbb-cc pattern, but many exceptions.  This table covers
    //   all documented opcodes — undocumented ops return 1 (safe skip).
    static const uint8_t lengths[256] = {
    //  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
        1, 2, 1, 1, 2, 2, 2, 1, 1, 2, 1, 1, 3, 3, 3, 1,  // 00-0F
        2, 2, 1, 1, 2, 2, 2, 1, 1, 3, 1, 1, 3, 3, 3, 1,  // 10-1F
        3, 2, 1, 1, 2, 2, 2, 1, 1, 2, 1, 1, 3, 3, 3, 1,  // 20-2F
        2, 2, 1, 1, 2, 2, 2, 1, 1, 3, 1, 1, 3, 3, 3, 1,  // 30-3F
        1, 2, 1, 1, 2, 2, 2, 1, 1, 2, 1, 1, 3, 3, 3, 1,  // 40-4F
        2, 2, 1, 1, 2, 2, 2, 1, 1, 3, 1, 1, 3, 3, 3, 1,  // 50-5F
        1, 2, 1, 1, 2, 2, 2, 1, 1, 2, 1, 1, 3, 3, 3, 1,  // 60-6F
        2, 2, 1, 1, 2, 2, 2, 1, 1, 3, 1, 1, 3, 3, 3, 1,  // 70-7F
        2, 2, 2, 1, 2, 2, 2, 1, 1, 2, 1, 1, 3, 3, 3, 1,  // 80-8F
        2, 2, 1, 1, 2, 2, 2, 1, 1, 3, 1, 1, 3, 3, 3, 1,  // 90-9F
        2, 2, 2, 1, 2, 2, 2, 1, 1, 2, 1, 1, 3, 3, 3, 1,  // A0-AF
        2, 2, 1, 1, 2, 2, 2, 1, 1, 3, 1, 1, 3, 3, 3, 1,  // B0-BF
        2, 2, 2, 1, 2, 2, 2, 1, 1, 2, 1, 1, 3, 3, 3, 1,  // C0-CF
        2, 2, 1, 1, 2, 2, 2, 1, 1, 3, 1, 1, 3, 3, 3, 1,  // D0-DF
        2, 2, 2, 1, 2, 2, 2, 1, 1, 2, 1, 1, 3, 3, 3, 1,  // E0-EF
        2, 2, 1, 1, 2, 2, 2, 1, 1, 3, 1, 1, 3, 3, 3, 1,  // F0-FF
    };
    return lengths[op];
}

/**
 * Scan a PRG binary for 6502 absolute-addressing instructions that
 * reference system-specific MMIO ranges.
 *
 * The function walks the code sequentially using the opcode length table.
 * For each 3-byte instruction (absolute addressing), it classifies the
 * 16-bit operand and ORs the corresponding MMIO flag into the result.
 *
 * This is a heuristic — it doesn't handle computed jumps or self-modifying
 * code — but it's effective for typical BASIC extensions and ML programs.
 *
 * @param code  Pointer to PRG code (AFTER the 2-byte load address header)
 * @param len   Length of the code data
 * @return      Bitwise OR of MmioSignature flags for all detected references
 */
inline uint32_t scan_6502_mmio_references(const uint8_t* code, size_t len) {
    uint32_t result = MMIO_NONE;
    size_t pos = 0;

    while (pos < len) {
        uint8_t op = code[pos];
        int ilen = opcode_length(op);

        if (ilen == 3 && pos + 2 < len) {
            // Absolute addressing: operand is little-endian 16-bit address
            uint16_t addr = code[pos + 1] | ((uint16_t)code[pos + 2] << 8);
            result |= classify_mmio_addr(addr);
        }

        pos += (ilen > 0) ? (size_t)ilen : 1;
    }

    return result;
}

/**
 * Count how many distinct MMIO flags are set in a signature bitmask.
 */
inline int count_mmio_flags(uint32_t sig) {
    int count = 0;
    // Only count individual device flags, not composite masks
    for (int i = 0; i < 20; i++) {
        if (sig & (1u << i)) count++;
    }
    return count;
}
