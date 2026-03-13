#pragma once
/*
 * nes_bus_chips.h — NES memory block layout for flat mem addressing
 *
 * All NES memory lives in a single contiguous buffer of 1KB blocks.
 * Hot-path address calculation: flat_mem[(block << BLOCK_SHIFT) | sub_addr]
 *
 *   CPU dispatch:  block = cpu_read_block[addr >> 12], sub_addr = addr & 0x0FFF
 *   PPU dispatch:  block = ppu_read_block[addr >> 10], sub_addr = addr & 0x03FF
 *
 * Fixed blocks (always present, independent of cartridge):
 *   0-1:    CPU WRAM (2KB internal RAM)
 *   2-3:    CIRAM (2KB PPU nametable VRAM)
 *   4:      Reserved (palette is PPU-internal, not on the bus)
 *   5-12:   PRG-RAM (8KB cartridge work RAM)
 *
 * Dynamic blocks (block 13+, sized at cartridge load):
 *   13..N:     CHR data (CHR-ROM or CHR-RAM, variable size)
 *   N+1..M:    PRG-ROM data (variable size)
 *
 * Block numbers >= BLOCK_SENTINEL_MIN are sentinel values that trigger
 * I/O dispatch (PPU regs, APU, open bus) instead of buffer access.
 */

#include <cstdint>

namespace nes_bus {

// ============================================================================
// Block sizing — 1KB blocks (smallest NES banking granularity: PPU CHR)
// ============================================================================

inline constexpr uint32_t BLOCK_SHIFT = 10;
inline constexpr uint32_t BLOCK_SIZE  = 1u << BLOCK_SHIFT;   // 1024
inline constexpr uint32_t BLOCK_MASK  = BLOCK_SIZE - 1;      // 0x3FF

// ============================================================================
// Fixed block assignments
// ============================================================================

inline constexpr uint16_t BLOCK_WRAM     = 0;    // 2 blocks  (2KB CPU internal RAM)
inline constexpr uint16_t BLOCK_CIRAM    = 2;    // 2 blocks  (2KB PPU nametable VRAM)
inline constexpr uint16_t BLOCK_RESERVED = 4;    // 1 block   (padding)
inline constexpr uint16_t BLOCK_PRG_RAM  = 5;    // 8 blocks  (8KB cartridge work RAM)
inline constexpr uint16_t BLOCK_DYNAMIC  = 13;   // First dynamic block

// Fixed region totals
inline constexpr uint32_t FIXED_BLOCKS   = 13;
inline constexpr uint32_t FIXED_SIZE     = FIXED_BLOCKS * BLOCK_SIZE;  // 13312 bytes

// Memory region sizes (hardware constants)
inline constexpr uint32_t WRAM_SIZE      = 2048;
inline constexpr uint32_t CIRAM_SIZE     = 2048;
inline constexpr uint32_t PRG_RAM_MAX    = 8192;

// ============================================================================
// Sentinel block values — trigger special dispatch, not buffer access
// ============================================================================

inline constexpr uint16_t BLOCK_SENTINEL_MIN = 0xFFF0;
inline constexpr uint16_t BLOCK_PPU_REGS     = 0xFFFD;  // PPU register I/O ($2000-$3FFF)
inline constexpr uint16_t BLOCK_APU_IO       = 0xFFFE;  // APU + controller I/O ($4000-$4FFF)
inline constexpr uint16_t BLOCK_OPEN_BUS     = 0xFFFF;  // Unmapped / open bus

} // namespace nes_bus
