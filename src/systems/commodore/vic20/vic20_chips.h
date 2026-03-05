#pragma once

#include <cstddef>
#include <cstdint>
// ============================================================================
// VIC-20 Memory Bank Map - Optimized Encoded Bank Type Design
// ============================================================================
//
// OPTIMIZED DESIGN: Single encoded byte per 1KB bank (64 bytes total)
// Replaces dual bitmap approach for fewer operations per access.
//
// Each byte encodes both read and write types:
//   - Bits [3:0] = read_type  (0=UNMAPPED, 1=IO, 2=ROM, 3=RAM)
//   - Bits [7:4] = write_type (0=UNMAPPED/ROM, 1=IO, 3=RAM)
//
// Memory type encoding:
//   UNMAPPED = 0 - Floating bus on read, writes ignored
//   IO       = 1 - I/O region, dispatch to handler
//   ROM      = 2 - Read from buffer, writes ignored
//   RAM      = 3 - Read/write to buffer
//
// Performance advantages over dual bitmap:
//   - Single array lookup (vs 2 bitmap extractions)
//   - Simpler decode: (type >= 2) catches both RAM and ROM
//   - 64 bytes fits in single cache line
//   - Unified with C64's encoded bank approach
//

// =============================
// Memory Base Address Constants
// =============================

// VIC-20 memory base addresses in the 64KB address space
#define VIC20_BASE_CHARROM    0x8000  // Character ROM at $8000-$8FFF (4KB)
#define VIC20_BASE_IO         0x9000  // I/O region at $9000-$9FFF (4KB)
#define VIC20_BASE_COLOR_RAM  0x9400  // Color RAM at $9400-$97FF (1KB, 4-bit wide)
#define VIC20_BASE_CART       0xA000  // Cartridge ROM at $A000-$BFFF (8KB)
#define VIC20_BASE_BASIC      0xC000  // BASIC ROM at $C000-$DFFF (8KB)
#define VIC20_BASE_KERNAL     0xE000  // KERNAL ROM at $E000-$FFFF (8KB)

// =============================
// Bank Type Encoding Constants
// =============================

// Memory type values (4 bits each for read and write)
#define VIC20_TYPE_UNMAPPED   0   // Floating bus on read, writes ignored
#define VIC20_TYPE_IO         1   // I/O region, dispatch to handler
#define VIC20_TYPE_ROM        2   // Read from buffer, writes ignored
#define VIC20_TYPE_RAM        3   // Read/write to buffer
#define VIC20_TYPE_CHARROM    4   // VIC-only: Character ROM (maps $1xxx to $8xxx in buffer)

// Encoding helpers: bits[3:0]=read_type, bits[7:4]=write_type
#define VIC20_ENCODE_BANK_TYPE(read_type, write_type) \
    (((write_type) << 4) | ((read_type) & 0x0F))

// Decoding helpers (inline for zero overhead)
static inline uint8_t vic20_decode_read_type(uint8_t encoded) {
    return encoded & 0x0F;
}
static inline uint8_t vic20_decode_write_type(uint8_t encoded) {
    return encoded >> 4;
}

// Pre-encoded bank type constants for common configurations
#define VIC20_BANK_TYPE_UNMAPPED  VIC20_ENCODE_BANK_TYPE(VIC20_TYPE_UNMAPPED, VIC20_TYPE_UNMAPPED)
#define VIC20_BANK_TYPE_IO        VIC20_ENCODE_BANK_TYPE(VIC20_TYPE_IO, VIC20_TYPE_IO)
#define VIC20_BANK_TYPE_ROM       VIC20_ENCODE_BANK_TYPE(VIC20_TYPE_ROM, VIC20_TYPE_UNMAPPED)
#define VIC20_BANK_TYPE_RAM       VIC20_ENCODE_BANK_TYPE(VIC20_TYPE_RAM, VIC20_TYPE_RAM)
#define VIC20_BANK_TYPE_CHARROM   VIC20_ENCODE_BANK_TYPE(VIC20_TYPE_CHARROM, VIC20_TYPE_UNMAPPED)

// =============================
// Expansion RAM Configuration
// =============================

// Expansion RAM block presence flags
enum vic20_expansion_flags_t {
    VIC20_EXP_NONE          = 0x00,  // Unexpanded VIC-20 (5KB: 1KB + 4KB)
    VIC20_EXP_BLOCK0        = 0x01,  // 3KB expansion at $0400-$0FFF
    VIC20_EXP_BLOCK2        = 0x04,  // 8KB expansion at $2000-$3FFF
    VIC20_EXP_BLOCK3        = 0x08,  // 8KB expansion at $4000-$5FFF
    VIC20_EXP_BLOCK5        = 0x10,  // 8KB expansion at $6000-$7FFF
    
    // Common configurations
    VIC20_EXP_3K            = VIC20_EXP_BLOCK0,                              // 3KB expansion (8KB total)
    VIC20_EXP_8K            = VIC20_EXP_BLOCK3,                              // 8KB expansion (13KB total)
    VIC20_EXP_16K           = VIC20_EXP_BLOCK2 | VIC20_EXP_BLOCK3,           // 16KB expansion (21KB total)
    VIC20_EXP_24K           = VIC20_EXP_BLOCK2 | VIC20_EXP_BLOCK3 | VIC20_EXP_BLOCK5, // 24KB (29KB total)
    VIC20_EXP_FULL          = VIC20_EXP_BLOCK0 | VIC20_EXP_BLOCK2 | VIC20_EXP_BLOCK3 | VIC20_EXP_BLOCK5, // All (32KB total)
};

// =============================
// Optimized Encoded Bank Map
// =============================

// Bank map structure - 64 bytes for single cache line
// Uses encoded bank type per 1KB bank instead of dual bitmaps
struct alignas(64) vic20_bank_map_t {
    uint8_t bank_type[64];  // Encoded type per 1KB bank: bits[3:0]=read, bits[7:4]=write
};

// Legacy bank map accessors (for compatibility during transition)
// These interpret the encoded bank types as boolean flags

// Get read type for a bank (returns true if readable: ROM or RAM)
static inline bool vic20_bank_readable(const vic20_bank_map_t* map, uint8_t bank) {
    return vic20_decode_read_type(map->bank_type[bank]) >= VIC20_TYPE_ROM;
}

// Get write type for a bank (returns true if writable: RAM only)
static inline bool vic20_bank_writable(const vic20_bank_map_t* map, uint8_t bank) {
    return vic20_decode_write_type(map->bank_type[bank]) == VIC20_TYPE_RAM;
}

// Set bank as RAM (readable and writable)
static inline void vic20_set_bank_ram(vic20_bank_map_t* map, uint8_t bank) {
    map->bank_type[bank] = VIC20_BANK_TYPE_RAM;
}

// Set bank as ROM (readable only)
static inline void vic20_set_bank_rom(vic20_bank_map_t* map, uint8_t bank) {
    map->bank_type[bank] = VIC20_BANK_TYPE_ROM;
}

// Set bank as I/O (special handler dispatch)
static inline void vic20_set_bank_io(vic20_bank_map_t* map, uint8_t bank) {
    map->bank_type[bank] = VIC20_BANK_TYPE_IO;
}

// Set bank as unmapped (floating bus)
static inline void vic20_set_bank_unmapped(vic20_bank_map_t* map, uint8_t bank) {
    map->bank_type[bank] = VIC20_BANK_TYPE_UNMAPPED;
}

// Set bank as Character ROM (VIC-only: maps $1xxx VIC addresses to $8xxx buffer)
static inline void vic20_set_bank_charrom(vic20_bank_map_t* map, uint8_t bank) {
    map->bank_type[bank] = VIC20_BANK_TYPE_CHARROM;
}

// =============================
// Bank Map Initialization
// =============================
// Initialize bank map with default VIC-20 memory layout
// Configures RAM, ROM, IO, and UNMAPPED regions based on expansion flags
void vic20_bank_map_init(vic20_bank_map_t* map, uint8_t expansion_flags, bool cartridge_present);

// Initialize bank map for VIC chip's 14-bit address space (16KB window)
// VIC uses only banks 0-15, with Character ROM appearing at $1000-$1FFF
void vic20_bank_map_init_vic(vic20_bank_map_t* map, uint8_t expansion_flags);
// =============================
// VIC-20 Memory Map Reference
// =============================
//
// Stock unexpanded VIC-20 memory map:
// Bank type encoding: R=read_type, W=write_type (0=UNMAPPED, 1=IO, 2=ROM, 3=RAM)
//
// Bank  Address Range   R  W  Type     Description
// ----  -------------   -  -  ------   -----------
// 0     $0000-$03FF     3  3  RAM      Zero page, stack, system variables
// 1     $0400-$07FF     0  0  UNMAPPED Expansion block 0 (3KB) - or RAM
// 2     $0800-$0BFF     0  0  UNMAPPED Expansion block 0 (continued)
// 3     $0C00-$0FFF     0  0  UNMAPPED Expansion block 0 (continued)
// 4-7   $1000-$1FFF     3  3  RAM      Main BASIC RAM (4KB, always present)
// 8-15  $2000-$3FFF     0  0  UNMAPPED Expansion block 2 (8KB) - or RAM
// 16-23 $4000-$5FFF     0  0  UNMAPPED Expansion block 3 (8KB) - or RAM
// 24-31 $6000-$7FFF     0  0  UNMAPPED Expansion block 5 (8KB) - or RAM
// 32-35 $8000-$8FFF     2  0  ROM      Character ROM (4KB)
// 36-39 $9000-$9FFF     1  1  IO       I/O region (VIC, VIAs, Color RAM, expansion)
// 40-47 $A000-$BFFF     0  0  UNMAPPED Cartridge ROM (8KB) - or ROM
// 48-55 $C000-$DFFF     2  0  ROM      BASIC ROM (8KB)
// 56-63 $E000-$FFFF     2  0  ROM      KERNAL ROM (8KB)
