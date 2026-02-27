#pragma once
/*
 * nes_bus_chips.h — NES chip IDs for unified buffer addressing
 *
 * Buffer layout (PAGE_SHIFT = 11, 2KB pages):
 *
 *   CHIP_WRAM      = 0   →  offset 0x0000   (2KB CPU WRAM)
 *   CHIP_CIRAM     = 1   →  offset 0x0800   (2KB PPU nametable VRAM)
 *   CHIP_PALETTE   = 2   →  offset 0x1000   (32 bytes PPU palette, padded to 2KB page)
 *   CHIP_PRG_RAM   = 3   →  offset 0x1800   (8KB cartridge WRAM, extends 4 pages)
 *   CHIP_CHR_RAM   = 7   →  offset 0x3800   (8KB CHR-RAM, extends 4 pages)
 *
 * For PRG-ROM and CHR-ROM, we do NOT store them in the unified buffer
 * (they can be megabytes).  Instead the bank map points into the
 * cartridge's own ROM vector.  Only WRAM, CIRAM, palette, PRG-RAM,
 * and CHR-RAM live in the unified buffer.
 *
 * See NES_MIGRATION_PLAN.md Part 2 for the full rationale.
 */

#include <cstdint>

namespace nes_bus {

// Page shift: 11 bits → 2KB pages
inline constexpr uint32_t PAGE_SHIFT = 11;
inline constexpr uint32_t PAGE_SIZE  = 1u << PAGE_SHIFT;   // 2048
inline constexpr uint32_t PAGE_MASK  = PAGE_SIZE - 1;      // 0x7FF

// Chip IDs — each encodes a base offset into the unified buffer via (id << PAGE_SHIFT)
enum nes_chip_id_t : uint8_t {
    // Fixed regions in unified buffer
    NES_CHIP_WRAM       = 0,    // 2KB CPU internal RAM ($0000-$07FF, mirrored to $1FFF)
    NES_CHIP_CIRAM      = 1,    // 2KB PPU internal VRAM (nametables)
    NES_CHIP_PALETTE    = 2,    // 32B palette RAM (page-padded to 2KB)
    NES_CHIP_PRG_RAM    = 3,    // 8KB cartridge work RAM ($6000-$7FFF), extends 4 pages [3..6]
    NES_CHIP_CHR_RAM    = 7,    // 8KB CHR-RAM (PPU pattern tables), extends 4 pages [7..10]

    // Bank-mapped (pointer-based, not in unified buffer)
    NES_CHIP_PRG_ROM    = 11,   // Cartridge PRG-ROM (variable size, up to 512KB+)
    NES_CHIP_CHR_ROM    = 12,   // Cartridge CHR-ROM (variable size, up to 512KB+)

    // Special dispatch (not backed by buffer — handled via I/O function pointers)
    NES_CHIP_PPU_REGS   = 13,   // PPU register I/O ($2000-$3FFF mirrored every 8 bytes)
    NES_CHIP_APU_IO     = 14,   // APU + I/O registers ($4000-$401F)
    NES_CHIP_UNMAPPED   = 15,   // Open bus / unmapped region
};

// Unified buffer size calculation:
//   WRAM:     1 page  ×  2KB =  2KB   (chip 0)
//   CIRAM:    1 page  ×  2KB =  2KB   (chip 1)
//   Palette:  1 page  ×  2KB =  2KB   (chip 2, only 32 bytes used)
//   PRG-RAM:  4 pages ×  2KB =  8KB   (chips 3-6)
//   CHR-RAM:  4 pages ×  2KB =  8KB   (chips 7-10)
//   Total: 11 pages = 22KB
inline constexpr uint32_t UNIFIED_BUFFER_PAGES = 11;
inline constexpr uint32_t UNIFIED_BUFFER_SIZE  = UNIFIED_BUFFER_PAGES * PAGE_SIZE;  // 22528

// Byte offsets into the unified buffer for each chip region
inline constexpr uint32_t WRAM_OFFSET    = NES_CHIP_WRAM    * PAGE_SIZE;  // 0x0000
inline constexpr uint32_t CIRAM_OFFSET   = NES_CHIP_CIRAM   * PAGE_SIZE;  // 0x0800
inline constexpr uint32_t PALETTE_OFFSET = NES_CHIP_PALETTE * PAGE_SIZE;  // 0x1000
inline constexpr uint32_t PRG_RAM_OFFSET = NES_CHIP_PRG_RAM * PAGE_SIZE;  // 0x1800
inline constexpr uint32_t CHR_RAM_OFFSET = NES_CHIP_CHR_RAM * PAGE_SIZE;  // 0x3800

} // namespace nes_bus
