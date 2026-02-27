#pragma once
/*
 * nes_ppu_palette.h — Static NES color palette (64-color LUT)
 *
 * Header-only: 64-entry RGB888 lookup table mapping the NES's 6-bit
 * palette index ($00-$3F) to 32-bit 0x00RRGGBB values.
 *
 * This is the "canonical" NES palette used by most emulators.
 * Future work may add selectable palettes (Composite, RGB, FBX, etc.).
 */

#include <cstdint>

namespace nes_palette {

// 64-color NES palette — indexed by (palette_ram_value & 0x3F)
inline constexpr uint32_t COLOR_TABLE[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFF6ECC, 0xFF8170, 0xFF9C12,
    0xD7B000, 0xA6C100, 0x79C900, 0x5ACA8A, 0x4BC0EA, 0x424242, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
};

// Look up an NES color index → 32-bit RGB
inline constexpr uint32_t to_rgb(uint8_t nes_color) {
    return COLOR_TABLE[nes_color & 0x3F];
}

} // namespace nes_palette
