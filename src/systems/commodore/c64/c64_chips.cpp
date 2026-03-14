#include "systems/commodore/c64/c64_chips.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// C64 CHIP MANAGEMENT IMPLEMENTATION - From Basic to Complex
// ============================================================================

// =============================
// Basic CHIP Description Functions
// =============================

// Chip entry lookup table for description and validation (handles irregular numbering)
struct c64_chip_entry_t {
    uint16_t base_address;
    size_t size;
    const char* label;
};

// Sparse lookup table indexed by CHIP_* values (supports irregular numbering)
static const c64_chip_entry_t c64_chip_to_entry[] = {
    /* [CHIP_ROML] = */     { 0x8000, 8*1024, "Cartridge ROM Low" },
    { 0, 0, nullptr }, // CHIP_1 unused
    /* [CHIP_ROMH] = */     { 0xA000, 8*1024, "Cartridge ROM High" }, // Note: Can also map to 0xE000
    { 0, 0, nullptr }, // CHIP_3 unused
    /* [CHIP_KERNAL] = */   { 0xE000, 8*1024, "KERNAL ROM" },
    { 0, 0, nullptr }, // CHIP_5 unused
    /* [CHIP_BASIC] = */    { 0xA000, 8*1024, "BASIC ROM" },
    { 0, 0, nullptr }, // CHIP_7 unused
    /* [CHIP_CHARROM] = */  { 0xD000, 4*1024, "Character ROM" },
    /* [CHIP_RAM] = */      { 0x0000, 64*1024, "RAM" },
    /* [CHIP_UNMAPPED] = */ { 0x0000, 0, "Unmapped" },
    /* [CHIP_IO] = */       { 0xD000, 4*1024, "I/O" },
};
static const size_t CHIP_ENTRY_COUNT = sizeof(c64_chip_to_entry) / sizeof(c64_chip_to_entry[0]);

// =============================
// Basic CHIP Description Function Implementation
// =============================

bool c64_chips_get_description(uint8_t chip, chip_description_t* out) {
    if (!out) return false;

    memset(out, 0, sizeof(*out));

    // Validate chip ID and get entry (handles irregular numbering via sparse array)
    if (chip >= CHIP_ENTRY_COUNT) return false;

    const c64_chip_entry_t* entry = &c64_chip_to_entry[chip];

    // Entry exists if it has a label (even UNMAPPED has a label)
    if (entry->label) {
        out->base = entry->base_address;
        out->size = entry->size;
        out->label = entry->label;
        return true;
    }

    // Fallback for undefined entries
    out->base = 0;
    out->size = 0;
    out->label = "?";
    return false;
}

// =============================
// CHIP Title Mapping Function Implementation
// =============================

const char* c64_chips_to_title(uint8_t chip) {
    switch (chip) {
        case CHIP_ROML:
            return "ROML";
        case CHIP_ROMH:
            return "ROMH";
        case CHIP_KERNAL:
            return "KERNAL";
        case CHIP_BASIC:
            return "BASIC";
        case CHIP_CHARROM:
            return "CHARROM";
        case CHIP_RAM:
            return "RAM";
        case CHIP_UNMAPPED:
            return "-";
        case CHIP_IO:
            return "I/O";
        default:
            return "?";
    }
}

// =============================
// CHIP Utility Function Implementation
// =============================

const char* c64_chips_size_to_str(size_t size) {
    static char buf[32];  // Increased buffer size to prevent truncation
    if (size >= (1 << 20) && (size % (1 << 20)) == 0) {
        snprintf(buf, sizeof(buf), "%zuMB", size / (1 << 20));
    } else if (size >= 1024 && (size % 1024) == 0) {
        snprintf(buf, sizeof(buf), "%zuKB", size / 1024);
    } else {
        snprintf(buf, sizeof(buf), "%zuB", size);
    }
    return buf;
}