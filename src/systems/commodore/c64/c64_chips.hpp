#pragma once

#include <cstddef>
#include <cstdint>

// ============================================================================
// C64 CHIP MANAGEMENT - Basic CHIP Definitions and Management
// ============================================================================

// =============================
// Basic CHIP Type Definitions
// =============================

// CHIP IDs strategically numbered for branchless flat memory address calculation
enum chip_id_t {
    // Strategic numbering with 4KB step size: offset = chip << 12 (chip * 4096)
    // 8KB regions take 2 steps each: ROML(0), ROMH(2), KERNAL(4), BASIC(6), CHARROM(7), RAM(9)
    // Buffer: ROML(0x0000) + ROMH(0x2000) + KERNAL(0x4000) + BASIC(0x6000) + CHARROM(0x7000) + RAM(0x9000)
    //
    // WHY CHIP_CHARROM = 7 (not 8)?
    // CHARROM is only 4KB but uses the same 0x1FFF mask as 8KB ROMs for branchless calculation.
    // The key insight: CHARROM is NEVER accessed with addresses 0x0000-0x0FFF!
    //
    // Hardware only accesses CHARROM at these addresses:
    //   - CPU:    0xD000-0xDFFF (masks to 0x1000-0x1FFF with 0x1FFF)
    //   - VIC-II: 0x1000-0x1FFF (masks to 0x1000-0x1FFF with 0x1FFF)
    //   - VIC-II: 0x9000-0x9FFF (masks to 0x1000-0x1FFF with 0x1FFF)
    //
    // All CHARROM accesses mask to offset range 0x1000-0x1FFF, which when added to
    // base 0x7000 produces 0x8000-0x8FFF. This creates a natural 4KB gap:
    //   - BASIC ROM ends at:   0x6000 + 0x1FFF = 0x7FFF
    //   - CHARROM starts at:   0x7000 + 0x1000 = 0x8000
    //   - Gap: NO OVERLAP despite CHARROM base at 0x7000!
    //
    // If we used CHIP_CHARROM = 8 (base 0x8000):
    //   - Would need special 0x0FFF mask (adds branch to address calculation)
    //   - VIC-II address 0x1000 & 0x1FFF = 0x1000, 0x8000 + 0x1000 = 0x9000 (maps to RAM, wrong!)
    //
    CHIP_ROML         = 0,   // 8KB ROM Low (cartridge) - maps to offset 0x0000 (0 << 12 = 0x0000)
    CHIP_ROMH         = 2,   // 8KB ROM High (cartridge) - maps to offset 0x2000 (2 << 12 = 0x2000)
    CHIP_KERNAL       = 4,   // 8KB KERNAL ROM - maps to offset 0x4000 (4 << 12 = 0x4000)
    CHIP_BASIC        = 6,   // 8KB BASIC ROM - maps to offset 0x6000 (6 << 12 = 0x6000)
    CHIP_CHARROM      = 7,   // 4KB Character ROM - maps to offset 0x7000 (7 << 12 = 0x7000) - See explanation above!
    CHIP_RAM          = 9,   // 64KB RAM - maps to offset 0x9000 (9 << 12 = 0x9000)

    // Non-offset values (not used in address calculation)
    CHIP_UNMAPPED     = 10,  // Unmapped regions (not in flat mem)
    CHIP_IO           = 11,  // I/O region ($D000-$DFFF) - special case, not in flat mem
};

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
struct chip_description_t {
    uint16_t base;
    size_t size;
    const char* label;
};

// =============================
// Basic CHIP Function Declarations
// =============================

// Fetch descriptor for a given CHIP from the static lookup table
bool c64_chips_get_description(uint8_t chip, chip_description_t* out);

// Map CHIP to a concise type/title string (not address/size)
const char* c64_chips_to_title(uint8_t chip);

// Utility: Convert a size in bytes to a human-readable string ("256B", "4KB", etc.)
const char* c64_chips_size_to_str(size_t size);

// (Encoding/decoding functions removed — MemoryBus uses separate read/write chip IDs)