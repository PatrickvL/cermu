#pragma once
// m680x0_opcode_tables.inc.hpp — Opcode dispatch tables
//
// The 68000 uses a 4-bit group dispatch (bits 15-12) plus sub-field
// decoding within each group.  The primary dispatch is a switch in
// handle_decode().  This file provides supplementary lookup tables
// for cycle counts, instruction sizes, and condition code evaluation.
//
// Included from m680x0.hpp before the class definition.

#include <array>
#include <cstdint>

namespace m680x0 {

// ── Cycle count tables ──────────────────────────────────────────
// Base cycle counts for common EA modes (does not include instruction-specific timing)
// Index by EAMode enum value

namespace timing {

// Effective address calculation times (in clock cycles)
// [mode] → clocks for .B/.W and .L
struct EATiming {
    uint8_t bw;   // Byte/Word access
    uint8_t l;    // Long access
};

constexpr std::array<EATiming, 12> ea_calc_cycles = {{
    // Mode                        .B/.W   .L
    { 0,  0},   // 0: Dn            0      0
    { 0,  0},   // 1: An            0      0
    { 4,  8},   // 2: (An)          4      8
    { 4,  8},   // 3: (An)+         4      8
    { 6, 10},   // 4: -(An)         6     10
    { 8, 12},   // 5: (d16,An)      8     12
    {10, 14},   // 6: (d8,An,Xn)   10     14
    { 8, 12},   // 7/0: Abs.W       8     12
    {12, 16},   // 7/1: Abs.L      12     16
    { 8, 12},   // 7/2: (d16,PC)    8     12
    {10, 14},   // 7/3: (d8,PC,Xn) 10     14
    { 4,  8},   // 7/4: #imm        4      8
}};

/// Map EA mode+reg to timing table index
inline constexpr uint8_t ea_timing_index(uint8_t mode, uint8_t reg) {
    if (mode < 7) return mode;
    return 7 + reg;  // 7..11 for special modes
}

} // namespace timing

} // namespace m680x0
