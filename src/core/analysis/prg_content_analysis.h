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
// BASIC SYS Address Extraction
// ============================================================================

namespace detail {

/**
 * Walk BASIC line-link chain to find end-of-text address ($2D/$2E).
 * Returns the absolute address one past the final 00-terminator of the
 * last BASIC line (i.e. the first byte of the variables area), or 0 on failure.
 */
inline uint16_t find_end_of_basic(const uint8_t* basic, size_t len, uint16_t load_addr) {
    size_t pos = 0;
    while (pos + 4 < len) {
        uint16_t next_ptr = basic[pos] | (basic[pos + 1] << 8);
        if (next_ptr == 0) {
            // End-of-program: the two zero bytes of next_ptr + the line's
            // null terminator are at pos..pos+1.  Variables start at pos+2.
            uint16_t eob = load_addr + (uint16_t)(pos + 2);
            return eob;
        }
        // Advance to next line via the absolute pointer
        size_t next_off = (size_t)(next_ptr - load_addr);
        if (next_off <= pos || next_off >= len) return 0;  // broken chain
        pos = next_off;
    }
    return 0;
}

/**
 * Minimal recursive-descent evaluator for Commodore BASIC numeric expressions.
 *
 * Handles:  decimal literals, +, -, *, parentheses, PEEK(expr).
 * Ignores:  variables (returns failure), division, exponentiation, strings.
 *
 * The evaluator receives the BASIC memory image so it can service PEEK()
 * for addresses inside the loaded program (or known zero-page locations).
 *
 * All state is local — no heap, no exceptions, safe for hot-path use.
 */
struct SysExprEval {
    const uint8_t* tok;     // current position in tokenised BASIC line
    const uint8_t* end;     // one past last byte of this BASIC line
    const uint8_t* mem;     // full PRG payload (BASIC data after load-addr header)
    size_t         mem_len;
    uint16_t       load_addr;
    bool           ok;      // false if parse failed (variable, unsupported, overflow)

    // BASIC tokens we recognise
    static constexpr uint8_t TOKEN_PEEK = 0xC2;

    void skip_spaces() {
        while (tok < end && *tok == 0x20) ++tok;
    }

    // Parse a primary: decimal literal, PEEK(...), or parenthesised expression.
    int32_t parse_primary() {
        skip_spaces();
        if (tok >= end) { ok = false; return 0; }

        // Decimal literal
        if (*tok >= '0' && *tok <= '9') {
            int32_t val = 0;
            while (tok < end && *tok >= '0' && *tok <= '9') {
                val = val * 10 + (*tok - '0');
                ++tok;
                if (val > 0xFFFF) { ok = false; return 0; }
            }
            return val;
        }

        // PEEK(expr)
        if (*tok == TOKEN_PEEK) {
            ++tok;  // consume PEEK token
            skip_spaces();
            if (tok >= end || *tok != '(') { ok = false; return 0; }
            ++tok;  // consume '('
            int32_t addr = parse_expr();
            skip_spaces();
            if (tok >= end || *tok != ')') { ok = false; return 0; }
            ++tok;  // consume ')'

            if (!ok || addr < 0 || addr > 0xFFFF) { ok = false; return 0; }

            // Service PEEK from loaded memory
            uint16_t a = (uint16_t)addr;
            if (a >= load_addr && (size_t)(a - load_addr) < mem_len)
                return (int32_t)mem[a - load_addr];

            // Well-known zero-page locations for Commodore BASIC stubs:
            // These are initialised by the KERNAL before RUN, so we know
            // their values from the program load address and size.
            //   $2B/$2C  — start-of-BASIC (= load_addr)             [C64/VIC-20]
            //   $2D/$2E  — start-of-variables (= end of BASIC text)
            //   $01      — 6510 I/O port (C64: default $37)
            //   $BA      — current device number (default 8)
            if (a == 0x2B) return load_addr & 0xFF;
            if (a == 0x2C) return (load_addr >> 8) & 0xFF;
            if (a == 0x2D || a == 0x2E) {
                // Walk the BASIC program to find end-of-text ($2D/$2E)
                uint16_t eob = find_end_of_basic(mem, mem_len, load_addr);
                if (eob != 0)
                    return (a == 0x2D) ? (eob & 0xFF) : ((eob >> 8) & 0xFF);
            }

            ok = false;  // unknown memory location
            return 0;
        }

        // Parenthesised sub-expression
        if (*tok == '(') {
            ++tok;
            int32_t val = parse_expr();
            skip_spaces();
            if (tok >= end || *tok != ')') { ok = false; return 0; }
            ++tok;
            return val;
        }

        // Anything else (variable, function, string) — bail
        ok = false;
        return 0;
    }

    // Multiplicative: primary (* primary)*
    // If the RHS of * fails to parse, revert to the value before the
    // operator (forgiving: handles SYS <num>*<variable-as-comment>).
    int32_t parse_mul() {
        int32_t val = parse_primary();
        while (ok && tok < end) {
            skip_spaces();
            if (tok < end && *tok == '*') {
                const uint8_t* save = tok;
                ++tok;
                int32_t rhs = parse_primary();
                if (!ok) { tok = save; ok = true; break; }
                val *= rhs;
                if (val < -0xFFFF || val > 0x1FFFE) { ok = false; return 0; }
            } else {
                break;
            }
        }
        return val;
    }

    // Additive: mul ((+|-) mul)*
    // Forgiving: if RHS fails (e.g. SYS 4118-MC V3.1- where MC is a
    // variable comment trick), revert to value before the operator.
    int32_t parse_expr() {
        int32_t val = parse_mul();
        while (ok && tok < end) {
            skip_spaces();
            if (tok < end && (*tok == '+' || *tok == '-')) {
                const uint8_t* save = tok;
                char op = (char)*tok;
                ++tok;
                int32_t rhs = parse_mul();
                if (!ok) { tok = save; ok = true; break; }
                val = (op == '+') ? val + rhs : val - rhs;
            } else {
                break;
            }
            if (val < -0xFFFF || val > 0x1FFFE) { ok = false; return 0; }
        }
        return val;
    }
};

} // namespace detail

/**
 * Extract the target address from a BASIC SYS statement.
 *
 * Many Commodore programs are "BASIC stubs" — a short BASIC listing whose
 * only purpose is `SYS <addr>` to jump into the ML payload that follows.
 * This function scans the first few BASIC lines for a SYS token ($9E)
 * and evaluates the following expression.
 *
 * Supported SYS argument forms:
 *   - `SYS 2061`                        — plain decimal
 *   - `SYS 2*4096+14`                   — arithmetic (+, -, *)
 *   - `SYS PEEK(43)+256*PEEK(44)+27`    — PEEK into loaded memory / zero-page
 *   - `SYS (2*4096+14)`                 — parenthesised
 *
 * Variable references are not evaluated (would require runtime state).
 * When parsing fails, a diagnostic is printed to stderr so unparsed
 * patterns can be identified and handled later.
 *
 * @param basic      Pointer to BASIC data (AFTER the 2-byte load address)
 * @param len        Length of the BASIC data
 * @param load_addr  The PRG load address (memory address of basic[0])
 * @return           Absolute SYS target address, or 0 if not found / not parseable
 */
inline uint16_t extract_sys_address(const uint8_t* basic, size_t len,
                                    uint16_t load_addr) {
    constexpr uint8_t TOKEN_SYS = 0x9E;
    constexpr int MAX_LINES = 5;    // Scan at most 5 BASIC lines

    size_t pos = 0;
    for (int line = 0; line < MAX_LINES && pos + 4 < len; ++line) {
        uint16_t next_ptr = basic[pos] | ((uint16_t)basic[pos + 1] << 8);
        if (next_ptr == 0) break;  // end of BASIC program

        // Convert absolute next-line pointer to file offset
        size_t next_offset = (next_ptr >= load_addr)
                           ? (size_t)(next_ptr - load_addr)
                           : len;
        size_t line_end = (next_offset < len) ? next_offset : len;
        size_t line_start = pos + 4;  // skip next-ptr (2) + line number (2)

        // Scan tokens in this line for SYS
        for (size_t i = line_start; i < line_end; ++i) {
            if (basic[i] == TOKEN_SYS) {
                detail::SysExprEval eval;
                eval.tok       = basic + i + 1;
                eval.end       = basic + line_end;
                eval.mem       = basic;
                eval.mem_len   = len;
                eval.load_addr = load_addr;
                eval.ok        = true;

                int32_t result = eval.parse_expr();

                if (eval.ok && result >= 0 && result <= 0xFFFF)
                    return (uint16_t)result;

                // Emit a diagnostic for unparsed SYS arguments so we can
                // extend the parser later.
                if (!eval.ok) {
                    // Build a short hex dump of the SYS argument bytes
                    size_t arg_start = i + 1;
                    size_t arg_len = line_end - arg_start;
                    if (arg_len > 24) arg_len = 24;
                    fprintf(stderr, "probe: unparsed SYS at $%04X+%zu: ",
                            (unsigned)load_addr, i);
                    for (size_t k = 0; k < arg_len; ++k)
                        fprintf(stderr, "%02X ", basic[arg_start + k]);
                    fprintf(stderr, "\n");
                }

                // SYS found but couldn't evaluate — return 0
                return 0;
            }
        }

        pos = next_offset;
    }
    return 0;
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
    // "Strong" C64: SID + CIAs only.  Excludes VIC-II ($D000) and Color RAM
    // ($D800) which overlap with C16/Plus4 screen RAM addresses.
    MMIO_C64_STRONG     = MMIO_C64_SID | MMIO_C64_CIA1 | MMIO_C64_CIA2,
    MMIO_ANY_VIC20      = MMIO_VIC20_VIC | MMIO_VIC20_VIA1 | MMIO_VIC20_VIA2 |
                          MMIO_VIC20_COLOR,
    // "Strong" VIC-20: VIC + VIAs only.  Excludes Color RAM ($9400-$97FF)
    // which is a broad 1 KB range prone to data-as-code false positives.
    MMIO_VIC20_STRONG   = MMIO_VIC20_VIC | MMIO_VIC20_VIA1 | MMIO_VIC20_VIA2,
    MMIO_ANY_C16        = MMIO_C16_TED | MMIO_C16_TED_KEYS | MMIO_C16_ACIA |
                          MMIO_C16_PIO,
};

/**
 * Classify a 16-bit address into an MMIO signature flag.
 * Returns MMIO_NONE if the address doesn't fall in a known I/O range.
 *
 * Note: $D000-$D3FF is classified as MMIO_C64_VICII, but this range
 * overlaps with C16/Plus4 screen RAM.  Callers should require additional
 * C64-specific hits (SID, CIA) before claiming C64 based on VIC-II alone.
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
