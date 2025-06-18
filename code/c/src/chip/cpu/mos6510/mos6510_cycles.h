#ifndef MOS6510_CYCLES_H
#define MOS6510_CYCLES_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// MOS 6510 OPCODE CYCLE COUNT TABLE
// ============================================================================
// Based on 6502/6510 documentation and cycle-accurate implementation analysis
// Includes all 256 opcodes (legal and illegal)
// Cycle counts reflect the minimum cycles for each instruction  
// Some instructions may take additional cycles due to page crossing or other factors

static const uint8_t mos6510_cycle_table[256] = {
    // 0x00-0x0F
    7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
    // 0x10-0x1F
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    // 0x20-0x2F
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
    // 0x30-0x3F
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    // 0x40-0x4F
    6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
    // 0x50-0x5F
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    // 0x60-0x6F
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,
    // 0x70-0x7F
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    // 0x80-0x8F
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    // 0x90-0x9F
    2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
    // 0xA0-0xAF
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    // 0xB0-0xBF
    2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
    // 0xC0-0xCF
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    // 0xD0-0xDF
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    // 0xE0-0xEF
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    // 0xF0-0xFF
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7
};

// Maximum reasonable cycle count for any single instruction     
// Used for stuck detection - if PC doesn't change for more than this many cycles
// we can consider the CPU potentially stuck
#define MOS6510_MAX_INSTRUCTION_CYCLES 8

// Extended maximum accounting for VIA bus-stealing and other C64 timing effects
// This provides a reasonable threshold while avoiding false positives
#define MOS6510_MAX_CYCLES_WITH_BUS_STEALING 16

// Get the base cycle count for an opcode
static inline uint8_t mos6510_get_opcode_cycles(uint8_t opcode) {
    return mos6510_cycle_table[opcode];
}

// Check if a cycle count is reasonable for any 6510 instruction 
static inline bool mos6510_is_reasonable_cycle_count(uint32_t cycles) {
    return cycles <= MOS6510_MAX_CYCLES_WITH_BUS_STEALING;
}

#endif // MOS6510_CYCLES_H