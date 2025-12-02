#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declaration for bus structure
typedef struct c64_bus_s c64_bus_t;

// ============================================================================
// C64 CHIP MANAGEMENT - Basic CHIP Definitions and Management
// ============================================================================

// =============================
// Basic CHIP Type Definitions
// =============================

// CHIP IDs strategically numbered for branchless unified memory buffer address calculation
typedef enum {
    // Strategic numbering with 4KB step size: offset = chip << 12 (chip * 4096)
    // 8KB regions take 2 steps each: ROML(0), ROMH(2), KERNAL(4), BASIC(6), CHARROM(8), RAM(9)
    // Buffer: ROML(0x0000) + ROMH(0x2000) + KERNAL(0x4000) + BASIC(0x6000) + CHARROM(0x8000) + RAM(0x9000)
    CHIP_ROML         = 0,   // 8KB ROM Low (cartridge) - maps to offset 0x0000 (0 << 12 = 0x0000)
    CHIP_ROMH         = 2,   // 8KB ROM High (cartridge) - maps to offset 0x2000 (2 << 12 = 0x2000)
    CHIP_KERNAL       = 4,   // 8KB KERNAL ROM - maps to offset 0x4000 (4 << 12 = 0x4000)
    CHIP_BASIC        = 6,   // 8KB BASIC ROM - maps to offset 0x6000 (6 << 12 = 0x6000)
    CHIP_CHARROM      = 8,   // 4KB Character ROM - maps to offset 0x8000 (8 << 12 = 0x8000)
    CHIP_RAM          = 9,   // 64KB RAM - maps to offset 0x9000 (9 << 12 = 0x9000)

    // Non-offset values (not used in address calculation)
    CHIP_UNMAPPED     = 10,  // Unmapped regions (not in unified buffer)
    CHIP_IO           = 11,  // I/O region ($D000-$DFFF) - special case, not in unified buffer
} chip_id_t;

// Array of valid CHIP IDs for iteration (due to irregular numbering)
static const uint8_t VALID_CHIP_IDS[] = {
    CHIP_ROML, CHIP_ROMH, CHIP_KERNAL, CHIP_BASIC,
    CHIP_CHARROM, CHIP_RAM, CHIP_UNMAPPED, CHIP_IO
};
static const size_t VALID_CHIP_COUNT = sizeof(VALID_CHIP_IDS) / sizeof(VALID_CHIP_IDS[0]);

// =============================
// Basic CHIP Information Structures
// =============================

// CHIP descriptor struct for tooling
typedef struct {
    uint16_t base;
    size_t size;
    const char* label;
} chip_description_t;

// =============================
// Basic CHIP Function Declarations
// =============================

// Fetch descriptor for a given CHIP from registered chips or synthesize for I/O/special
bool c64_chips_get_description(const c64_bus_t* bus, uint8_t chip, chip_description_t* out);

// Map CHIP to a concise type/title string (not address/size)
const char* c64_chips_to_title(uint8_t chip);

// Utility: Convert a size in bytes to a human-readable string ("256B", "4KB", etc.)
const char* c64_chips_size_to_str(size_t size);

// =============================
// CHIP Encoding/Decoding Functions
// =============================

// Encoding macros for packing read/write CHIPs into single byte
// Order: I/O pages (0-15), then writable chips (16-19), then read-only (20-24)
// Non-I/O CHIPs 16-24 become 1-9 in encoded form, which fits in 4 bits.
// Writable CHIPs 16-19 become 1-4 in encoded form, which fits in 3 bits.
// Output byte format: [7:5] write code (3 bits), [4] unused (1 bit), [3:0] read code (4 bits)
static inline uint8_t encode_chip_rw(uint8_t read_chip, uint8_t write_chip) {
    uint8_t encoded = read_chip | (write_chip << 4);
    return encoded;
}

static inline uint8_t decode_read_chip(uint8_t encoded) {
    return encoded & 0x0F; // Lower 4 bits are the read chip
}

static inline uint8_t decode_write_chip(uint8_t encoded) {
    return (encoded >> 4) & 0x0F; // Upper 4 bits are the write chip
}