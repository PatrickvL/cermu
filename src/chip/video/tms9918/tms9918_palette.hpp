#pragma once
/*
 * tms9918_palette.hpp — Fixed color palettes for TMS9918 VDP family
 *
 * The base TMS9918/A/28/29 chips have a fixed 15-color palette (+ transparent).
 * Multiple community-measured alternatives are provided as NamedPalette entries.
 *
 * Color format: 0xAABBGGRR — GL_RGBA little-endian convention.
 * (On little-endian: memory bytes are [R, G, B, A], matching GL_RGBA + GL_UNSIGNED_BYTE.)
 */

#include "chip/video/video_chip_base.hpp"
#include <cstdint>

namespace tms9918 {

// ============================================================================
// TI DATASHEET PALETTE — canonical TMS9918A colors
// ============================================================================
// Source: TMS9918A datasheet (TI, 1981), signal-level RGB approximation.

inline constexpr uint32_t PALETTE_DATASHEET[16] = {
    0x00000000,  //  0: Transparent
    0xFF000000,  //  1: Black
    0xFF3EB847,  //  2: Medium Green
    0xFF6FCF74,  //  3: Light Green
    0xFFEF5350,  //  4: Dark Blue
    0xFFFF6F6F,  //  5: Light Blue
    0xFF4E51B8,  //  6: Dark Red
    0xFFFFD26C,  //  7: Cyan
    0xFF5254FB,  //  8: Medium Red
    0xFF7A79FF,  //  9: Light Red
    0xFF50BED2,  // 10: Dark Yellow
    0xFF6DCFE0,  // 11: Light Yellow
    0xFF3DA241,  // 12: Dark Green
    0xFFC468B6,  // 13: Magenta
    0xFFCCCCCC,  // 14: Gray
    0xFFFFFFFF,  // 15: White
};

// ============================================================================
// TMSEMU PALETTE — measured from real hardware
// ============================================================================
// Source: tms9918a.net measurements by Marat Fayzullin / Sean Young.

inline constexpr uint32_t PALETTE_TMSEMU[16] = {
    0x00000000,  //  0: Transparent
    0xFF000000,  //  1: Black
    0xFF47B73E,  //  2: Medium Green
    0xFF7CCF6F,  //  3: Light Green
    0xFFEF5350,  //  4: Dark Blue
    0xFFFF7D6F,  //  5: Light Blue
    0xFF5054B6,  //  6: Dark Red
    0xFFFFD06A,  //  7: Cyan
    0xFF5456FB,  //  8: Medium Red
    0xFF7C7AFF,  //  9: Light Red
    0xFF53BFD4,  // 10: Dark Yellow
    0xFF70CFE0,  // 11: Light Yellow
    0xFF3EA23D,  // 12: Dark Green
    0xFFBA64B6,  // 13: Magenta
    0xFFCCCCCC,  // 14: Gray
    0xFFFFFFFF,  // 15: White
};

// ============================================================================
// OPENMSX PALETTE — used by the openMSX emulator
// ============================================================================
// Source: openMSX project, adjusted for better perceived accuracy.

inline constexpr uint32_t PALETTE_OPENMSX[16] = {
    0x00000000,  //  0: Transparent
    0xFF000000,  //  1: Black
    0xFF3EB849,  //  2: Medium Green
    0xFF74D07D,  //  3: Light Green
    0xFFEC5353,  //  4: Dark Blue
    0xFFFF7575,  //  5: Light Blue
    0xFF5151B8,  //  6: Dark Red
    0xFFFFD26C,  //  7: Cyan
    0xFF5555FC,  //  8: Medium Red
    0xFF7F7FFF,  //  9: Light Red
    0xFF54BFD4,  // 10: Dark Yellow
    0xFF71CFE1,  // 11: Light Yellow
    0xFF3EA241,  // 12: Dark Green
    0xFFBE6DB7,  // 13: Magenta
    0xFFCCCCCC,  // 14: Gray
    0xFFFFFFFF,  // 15: White
};

// ============================================================================
// NAMED PALETTE REGISTRY
// ============================================================================

inline const NamedPalette NAMED_PALETTES[] = {
    { "datasheet", "TI Datasheet",       PALETTE_DATASHEET, 16 },
    { "tmsemu",    "TMSEmu (Measured)",   PALETTE_TMSEMU,    16 },
    { "openmsx",   "openMSX",            PALETTE_OPENMSX,   16 },
};

inline constexpr int NAMED_PALETTE_COUNT =
    sizeof(NAMED_PALETTES) / sizeof(NAMED_PALETTES[0]);

} // namespace tms9918
