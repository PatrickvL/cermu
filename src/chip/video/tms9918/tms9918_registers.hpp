#pragma once
/*
 * tms9918_registers.hpp — Register declarations for TMS9918 VDP family
 *
 * X-macro DECL pattern (REG/FLD) for the TMS9918 register set.
 * Base set (R0–R7) shared by all variants; extended sets for Sega and V9938+.
 *
 * Register-related macros/constants are declared ABOVE the chip struct so
 * that inline methods can reference them at parse time.
 */

#include "core/chip_debug_registry.hpp"
#include <cstdint>

namespace tms9918 {

// ============================================================================
// BASE REGISTERS (R0–R7) — shared by all TMS9918 family members
// ============================================================================
//
// TMS9918 I/O model:
//   Port 0: VRAM data (read/write with auto-increment)
//   Port 1: Register/address latch (two-byte write sequence)
//
// Register write: byte1 = value, byte2 = (1 << 7) | register_number
// VRAM address:   byte1 = addr_lo, byte2 = (0 << 7) | (rw << 6) | addr_hi[5:0]

#define TMS9918_BASE_DECL(REG, FLD, CMP)                                      \
    /* R0 — Mode Control 0 */                                                  \
    REG(0, R0, "Mode Control 0")                                               \
      FLD(R0, EXTVID, 0:0, "External video input",  Flag, 0, 0)               \
      FLD(R0, M3,     1:1, "Mode bit 3 (M3)",       Flag, 0, 0)               \
    /* R1 — Mode Control 1 */                                                  \
    REG(1, R1, "Mode Control 1")                                               \
      FLD(R1, MAG,    0:0, "Sprite magnification",  Flag, 0, 0)               \
      FLD(R1, SI,     1:1, "Sprite size (16×16)",   Flag, 0, 0)               \
      FLD(R1, M2,     3:3, "Mode bit 2 (M2)",       Flag, 0, 0)               \
      FLD(R1, M1,     4:4, "Mode bit 1 (M1)",       Flag, 0, 0)               \
      FLD(R1, IE,     5:5, "Interrupt enable",       Flag, 0, 0)               \
      FLD(R1, BL,     6:6, "Blank (screen on/off)",  Flag, 0, 0)              \
      FLD(R1, VRAM16K,7:7, "16K VRAM select",       Flag, 0, 0)               \
    /* R2 — Name Table Base Address */                                         \
    REG(2, R2, "Name Table Base")                                              \
      FLD(R2, NT,     3:0, "Name table base [13:10]", Value, 0, 0)            \
    /* R3 — Color Table Base Address */                                        \
    REG(3, R3, "Color Table Base")                                             \
    /* R4 — Pattern Generator Base Address */                                  \
    REG(4, R4, "Pattern Generator Base")                                       \
      FLD(R4, PG,     2:0, "Pattern gen base [13:11]", Value, 0, 0)           \
    /* R5 — Sprite Attribute Table Base Address */                             \
    REG(5, R5, "Sprite Attribute Base")                                        \
      FLD(R5, SA,     6:0, "Sprite attr base [13:7]", Value, 0, 0)            \
    /* R6 — Sprite Pattern Generator Base Address */                           \
    REG(6, R6, "Sprite Pattern Base")                                          \
      FLD(R6, SG,     2:0, "Sprite patt base [13:11]", Value, 0, 0)           \
    /* R7 — Text/Backdrop Color */                                             \
    REG(7, R7, "Text / Backdrop Color")                                        \
      FLD(R7, BD,     3:0, "Backdrop color",          Value, 0, 0)             \
      FLD(R7, TC,     7:4, "Text color",              Value, 0, 0)

// ============================================================================
// SEGA-EXTENDED REGISTERS (R8–R10) — for 315-5124 / 315-5246
// ============================================================================

#define TMS9918_SEGA_DECL(REG, FLD, CMP)                                       \
    REG(8,  R8,  "H-Scroll Value")                                             \
    REG(9,  R9,  "V-Scroll Value")                                             \
    REG(10, R10, "Line Counter")

// ============================================================================
// REGISTER INDEX CONSTANTS
// ============================================================================

namespace reg {
    // Extract register-index constants from the base DECL
    TMS9918_BASE_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)

    constexpr uint8_t BASE_COUNT = 8;
    constexpr uint8_t BASE_MASK  = 0x07;

    // Status register bits (read-only, separate from control registers)
    constexpr uint8_t STATUS_F    = 0x80;  // Frame (vblank) interrupt flag
    constexpr uint8_t STATUS_5S   = 0x40;  // 5th sprite flag (or 9th on V9938)
    constexpr uint8_t STATUS_C    = 0x20;  // Sprite collision flag
    constexpr uint8_t STATUS_5NUM = 0x1F;  // 5th sprite number (or 9th)
}

// ============================================================================
// BITFIELD ACCESSOR CONSTANTS — fld::REG_FLD (mask), fld::REG_FLD_S (shift)
// ============================================================================

namespace fld {
#define TMS9918_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
TMS9918_BASE_DECL(DECL_REG_NOP, TMS9918_X_FLD_NS_, DECL_CMP_NOP)
#undef TMS9918_X_FLD_NS_
} // namespace fld

// ============================================================================
// REGISTER INFO (for debug UI)
// ============================================================================

DECL_EXTRACT(TMS9918, TMS9918_BASE_DECL)

// ============================================================================
// SCREEN MODE DECODING
// ============================================================================
//
// M1 (R1 bit 4), M2 (R1 bit 3), M3 (R0 bit 1) select the screen mode:
//
// M1 M2 M3 | Mode
// ---------+------
//  0  0  0 | 0  Graphics I      (256×192, 8×8 tiles, 1 color per 8 chars)
//  1  0  0 | 1  Text             (240×192, 6×8 font, global fg/bg)
//  0  0  1 | 2  Graphics II      (256×192, 8×8 tiles, per-row color)
//  0  1  0 | 3  Multicolor        (64×48, 4×4 blocks)
//  (other) | Undocumented / extended (Sega mode 4, V9938 modes)
//
// Screen mode is runtime-decoded from register bits, not a trait axis
// (the same physical chip can switch modes via register writes).

inline constexpr uint8_t decode_screen_mode(uint8_t r0, uint8_t r1) {
    const uint8_t m1 = (r1 >> 4) & 1;
    const uint8_t m2 = (r1 >> 3) & 1;
    const uint8_t m3 = (r0 >> 1) & 1;
    const uint8_t m4 = (r0 >> 2) & 1;  // Sega Mode 4 flag
    // Standard modes (0–7); M4=1 → values 8+ (Sega Mode 4 variants)
    return static_cast<uint8_t>((m4 << 3) | (m1 << 2) | (m3 << 1) | m2);
}

// Standard mode IDs returned by decode_screen_mode()
namespace ScreenMode {
    inline constexpr uint8_t GRAPHIC_I      = 0;   // M1=0 M3=0 M2=0
    inline constexpr uint8_t TEXT           = 4;   // M1=1 M3=0 M2=0
    inline constexpr uint8_t GRAPHIC_II     = 2;   // M1=0 M3=1 M2=0
    inline constexpr uint8_t MULTICOLOR     = 1;   // M1=0 M3=0 M2=1
    inline constexpr uint8_t SEGA_MODE4     = 8;   // M4=1, M2=0 — 192 lines
    inline constexpr uint8_t SEGA_MODE4_224 = 9;   // M4=1, M2=1, M1=0 — 224 lines (315-5246)
    inline constexpr uint8_t SEGA_MODE4_240 = 13;  // M4=1, M2=1, M1=1 — 240 lines (315-5246)
}

} // namespace tms9918
