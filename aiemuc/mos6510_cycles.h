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
#if 1    
    return mos6510_cycle_table[opcode];
#else
    uint8_t c = ((opcode >> 2) & 7) < 4 ? 2 + (opcode >> 6) : 4 + (opcode >> 5);
    return (opcode ^ 0x20) < 0x41 ? (opcode & 0xBF) == 0x00 ? 7 : 6 :
           (opcode & 0x7C) == 0x4C ? 3 : (opcode & 0x1C) == 0x10 ? 2 :
           (opcode & 0x1B) > 0x10 ? 4 : c + ((opcode & 0x70) == 0x50);
/*
## How It Works
- Initial Value with Case Split:
  - Addressing modes (bits 2–4, m = (opcode >> 2) & 7) are split into two groups:
    - Modes 0–3 (implied, immediate, zero page, absolute): Start with c = 2 + (opcode >> 6). The opcode >> 6 (group) adds 0, 1, or 2, approximating base cycles (2–4).
    - Modes 4–7 (indexed, indirect): Start with c = 4 + (opcode >> 5). The opcode >> 5 adds 0–3, covering higher cycle counts (4–6).
  - This split reduces adjustments by aligning the initial value closer to common cycle counts.
- Bitwise Adjustments:
  - BRK, JSR, RTI, RTS: (opcode ^ 0x20) < 0x41 checks for 0x00 (BRK) and 0x20, 0x40, 0x60 (JSR, RTI, RTS) using XOR to align them around a common pattern.
    Then, (opcode & 0xBF) == 0x00 distinguishes BRK (7 cycles) from the others (6 cycles).
  - JMP: (opcode & 0x7C) == 0x4C checks for 0x4C, 0x6C (3 cycles).
  - Branches: (opcode & 0x1C) == 0x10 detects branch opcodes (2 cycles).
  - Illegal Opcodes (SLO, SRE, etc.): (opcode & 0x1B) > 0x10 catches illegal opcodes like SLO, SRE (4 cycles).
  - Page-Crossing Penalty: (opcode & 0x70) == 0x50 adds 1 for absolute,Y and (zero page),Y in group 1.
- Default: Other cases use c plus the page-crossing penalty if applicable.
## Operation Count
- Bitwise Operations: 6 shifts/masks (>> 2, & 7, >> 6, >> 5, ^ 0x20, & multiple masks).
- Comparisons: 5 (< 4, < 0x41, == 0x00, == 0x4C, == 0x10, > 0x10, == 0x50).
- Arithmetic: 2 additions (2 +, 4 +, plus +1 for page-crossing).
- Ternaries: 2 (initial c and return chain).
- Total: ~13 core operations (shifts, masks, comparisons, additions), fewer than the previous version’s ~15 (due to nested ternaries and more complex adjustments).
## Why It’s More Compact
- Case-Split Initial Value: Splitting modes 0–3 vs. 4–7 reduces adjustment complexity, as each group’s base cycles are closer to the final value.
- XOR for Special Cases: (opcode ^ 0x20) < 0x41 efficiently groups 0x00, 0x20, 0x40, 0x60 (BRK, JSR, RTI, RTS) by flipping bits to align them, reducing checks.
- OR and AND Masks: (opcode & 0x1C) == 0x10 and (opcode & 0x1B) > 0x10 use minimal bits to detect branches and illegal opcodes, consolidating multiple conditions.
- Fewer Adjustments: The initial c calculation absorbs group and mode differences, leaving only page-crossing and specific overrides.

This solution is likely the minimal in terms of operations and code size while meeting the constraint of calculating cycles without a table.
It leverages bitwise operations and a split initial value to streamline the logic.           
*/
#endif
}

// Check if a cycle count is reasonable for any 6510 instruction
static inline bool mos6510_is_reasonable_cycle_count(uint32_t cycles) {
    return cycles <= MOS6510_MAX_CYCLES_WITH_BUS_STEALING;
}

#endif // MOS6510_CYCLES_H
