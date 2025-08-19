#ifndef MOS6510_CYCLE_PLA_LOOKUP_H
#define MOS6510_CYCLE_PLA_LOOKUP_H

#include "instruction_table.h"
#include "timing_states.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * MOS6510 Advanced PLA Lookup and Optimization
 * 
 * This implements the complete PLA (Programmable Logic Array) lookup system
 * with direct O(1) access, all 105 illegal opcodes, and pattern-based
 * instruction classification as specified in the emulator spec.
 */

// ===== ADVANCED PLA LOOKUP SYSTEM =====

/**
 * Extended instruction properties for illegal opcodes
 */
// Special instruction properties (4-bit field: 0x0 to 0xF)
#define INSTR_PROP_NORMAL         0x0     // Normal legal instruction
// Note: INSTR_PROP_BRANCH and INSTR_PROP_RMW are defined in instruction_table.h
// We reuse those values to avoid conflicts:
// #define INSTR_PROP_BRANCH      0x02    // From instruction_table.h
// #define INSTR_PROP_RMW         0x04    // From instruction_table.h
#define INSTR_PROP_PAGE_CROSS     0x2     // Page crossing affects cycle count
#define INSTR_PROP_JAM_OPCODE     0x4     // JAM/KIL opcode (halts CPU)
#define INSTR_PROP_USEFUL_ILLEGAL 0x5     // Useful illegal opcode
#define INSTR_PROP_UNSTABLE       0x6     // Unstable illegal opcode
#define INSTR_PROP_HIGHLY_ILLEGAL 0x7     // Highly illegal (crashes CPU)
#define INSTR_PROP_NOP_VARIANT    0x8     // NOP variant with different timing
#define INSTR_PROP_ALU_ILLEGAL    0x9     // ALU-based illegal opcode
#define INSTR_PROP_RMW_ILLEGAL    0xA     // RMW-based illegal opcode
#define INSTR_PROP_LOAD_ILLEGAL   0xB     // Load-combo illegal opcode
#define INSTR_PROP_STORE_ILLEGAL  0xC     // Store-combo illegal opcode
#define INSTR_PROP_SHIFT_ILLEGAL  0xD     // Shift-combo illegal opcode
#define INSTR_PROP_SPECIAL_COMBO  0xE     // Special combination illegal
#define INSTR_PROP_RESERVED       0xF     // Reserved for future use

/**
 * Illegal opcode categories for pattern-based implementation
 */
typedef enum {
    ILLEGAL_CATEGORY_NONE = 0,      // Legal opcode
    ILLEGAL_CATEGORY_ALU_IMM,       // ALU + immediate (ANC, ASR, etc.)
    ILLEGAL_CATEGORY_RMW_ABS,       // RMW + absolute (SLO, RLA, etc.)
    ILLEGAL_CATEGORY_RMW_ZP,        // RMW + zero page
    ILLEGAL_CATEGORY_LOAD_COMBO,    // Combined load (LAX, SAX, etc.)
    ILLEGAL_CATEGORY_DCP_ISC,       // DCP/ISC variants
    ILLEGAL_CATEGORY_SHX_SHY,       // SHX/SHY (unstable)
    ILLEGAL_CATEGORY_JAM,           // JAM/KIL opcodes
    ILLEGAL_CATEGORY_NOP_VARIANTS   // NOP variants with different timings
} illegal_opcode_category_t;

/**
 * Illegal opcode behavior pattern structure
 */
typedef struct {
    illegal_opcode_category_t category;
    uint8_t base_operation;      // Base legal operation (if any)
    uint8_t modifier_operation;  // Secondary operation
    uint8_t affected_registers;  // Which registers are affected
    bool has_side_effects;       // Whether opcode has side effects
    bool is_unstable;           // Whether behavior is unstable
} illegal_opcode_pattern_t;

// ===== DIRECT O(1) PLA LOOKUP =====

/**
 * Ultra-fast direct PLA lookup with illegal opcode support
 * This is the core lookup function - exactly O(1) with no branching
 */
static inline const instruction_definition_ultra_t* pla_lookup_advanced(uint8_t opcode) {
    extern const instruction_definition_ultra_t instruction_table_complete[256];
    return &instruction_table_complete[opcode];
}

/**
 * Check if opcode is a legal instruction
 */
static inline bool pla_is_legal_opcode(uint8_t opcode) {
    const instruction_definition_ultra_t* instr = pla_lookup_advanced(opcode);
    return instr->special_props <= INSTR_PROP_RMW;
}

/**
 * Check if illegal opcode is "useful" (has predictable behavior)
 */
static inline bool pla_is_useful_illegal(uint8_t opcode) {
    const instruction_definition_ultra_t* instr = pla_lookup_advanced(opcode);
    return instr->special_props == INSTR_PROP_USEFUL_ILLEGAL ||
           instr->special_props == INSTR_PROP_ALU_ILLEGAL ||
           instr->special_props == INSTR_PROP_RMW_ILLEGAL ||
           instr->special_props == INSTR_PROP_LOAD_ILLEGAL ||
           instr->special_props == INSTR_PROP_STORE_ILLEGAL ||
           instr->special_props == INSTR_PROP_SHIFT_ILLEGAL ||
           instr->special_props == INSTR_PROP_SPECIAL_COMBO;
}

/**
 * Check if opcode is a JAM/KIL instruction (halts CPU)
 */
static inline bool pla_is_jam_opcode(uint8_t opcode) {
    const instruction_definition_ultra_t* instr = pla_lookup_advanced(opcode);
    return instr->special_props == INSTR_PROP_JAM_OPCODE;
}

// ===== BRANCHLESS INSTRUCTION CLASSIFICATION =====

/**
 * Ultra-fast branchless instruction classification functions
 * These compile to 1-5 CPU instructions each
 */

/**
 * Get instruction group from opcode (AAA-BBB-CC pattern)
 */
static inline uint8_t pla_get_instruction_group(uint8_t opcode) {
    return opcode & 0x03; // CC bits determine main group
}

/**
 * Get AAA bits (operation type)
 */
static inline uint8_t pla_get_aaa_bits(uint8_t opcode) {
    return (opcode >> 5) & 0x07;
}

/**
 * Get BBB bits (addressing mode)
 */
static inline uint8_t pla_get_bbb_bits(uint8_t opcode) {
    return (opcode >> 2) & 0x07;
}

/**
 * Check if instruction is Group 01 (main ALU operations)
 */
static inline bool pla_is_group_01(uint8_t opcode) {
    return (opcode & 0x03) == 0x01;
}

/**
 * Check if instruction is Group 10 (RMW, load/store)
 */
static inline bool pla_is_group_10(uint8_t opcode) {
    return (opcode & 0x03) == 0x02;
}

/**
 * Check if instruction is Group 00 (control, branches, interrupts)
 */
static inline bool pla_is_group_00(uint8_t opcode) {
    return (opcode & 0x03) == 0x00;
}

/**
 * Check if instruction is Group 11 (mostly illegal opcodes)
 */
static inline bool pla_is_group_11(uint8_t opcode) {
    return (opcode & 0x03) == 0x03;
}

// ===== PATTERN-BASED ILLEGAL OPCODE CLASSIFICATION =====

/**
 * Get illegal opcode category using pattern recognition
 */
illegal_opcode_category_t pla_get_illegal_category(uint8_t opcode);

/**
 * Get illegal opcode behavior pattern
 */
illegal_opcode_pattern_t pla_get_illegal_pattern(uint8_t opcode);

/**
 * Execute illegal opcode operation (pattern-based)
 */
bool pla_execute_illegal_opcode(uint8_t opcode, void* cpu_state, 
                                void (*register_callback)(uint8_t reg, uint8_t val),
                                void (*flag_callback)(uint8_t flags));

// ===== ADDRESSING MODE INFERENCE =====

/**
 * Infer addressing mode from opcode bit patterns
 */
addressing_mode_t pla_infer_addressing_mode(uint8_t opcode);

/**
 * Check if addressing mode uses X indexing
 */
static inline bool pla_uses_x_indexing(uint8_t opcode) {
    const uint8_t bbb = pla_get_bbb_bits(opcode);
    const uint8_t cc = pla_get_instruction_group(opcode);
    const uint8_t aaa = pla_get_aaa_bits(opcode);
    
    // Pattern: X,ind and abs,X and zp,X
    if (cc == 0x01 && bbb == 0x00) return true;  // (zp,X)
    if (cc == 0x01 && bbb == 0x07) return true;  // abs,X (Group 01)
    if (cc == 0x02 && bbb == 0x05) return true;  // zp,X (Group 10)
    if (cc == 0x02 && bbb == 0x07) {
        // abs,X but not LDX abs,Y (special case)
        return !(aaa == 0x05); // LDX uses Y indexing in abs,Y mode
    }
    
    return false;
}

/**
 * Check if addressing mode uses Y indexing
 */
static inline bool pla_uses_y_indexing(uint8_t opcode) {
    const uint8_t bbb = pla_get_bbb_bits(opcode);
    const uint8_t cc = pla_get_instruction_group(opcode);
    const uint8_t aaa = pla_get_aaa_bits(opcode);
    
    // Pattern: ind,Y and abs,Y and special cases
    if (cc == 0x01 && bbb == 0x04) return true;  // (zp),Y
    if (cc == 0x01 && bbb == 0x06) return true;  // abs,Y
    if (cc == 0x02 && aaa == 0x05 && bbb == 0x05) return true; // LDX zp,Y
    if (cc == 0x02 && aaa == 0x05 && bbb == 0x07) return true; // LDX abs,Y
    
    return false;
}

// ===== ALU OPERATION INFERENCE =====

/**
 * Infer ALU operation from opcode AAA bits
 */
static inline alu_operation_t pla_infer_alu_operation(uint8_t opcode) {
    const uint8_t aaa = pla_get_aaa_bits(opcode);
    const uint8_t cc = pla_get_instruction_group(opcode);
    
    if (cc == 0x01) {
        // Group 01: Main ALU operations
        switch (aaa) {
            case 0x00: return ALU_LOGIC;      // ORA
            case 0x01: return ALU_LOGIC;      // AND  
            case 0x02: return ALU_LOGIC;      // EOR
            case 0x03: return ALU_ARITHMETIC; // ADC
            case 0x04: return ALU_TRANSFER;   // STA
            case 0x05: return ALU_TRANSFER;   // LDA
            case 0x06: return ALU_ARITHMETIC; // CMP
            case 0x07: return ALU_ARITHMETIC; // SBC
        }
    } else if (cc == 0x02) {
        // Group 10: RMW and Load/Store
        switch (aaa) {
            case 0x00: return ALU_SHIFT;      // ASL
            case 0x01: return ALU_SHIFT;      // ROL
            case 0x02: return ALU_SHIFT;      // LSR
            case 0x03: return ALU_SHIFT;      // ROR
            case 0x04: return ALU_TRANSFER;   // STX
            case 0x05: return ALU_TRANSFER;   // LDX
            case 0x06: return ALU_INCREMENT;  // DEC
            case 0x07: return ALU_INCREMENT;  // INC
        }
    }
    
    return ALU_NOP;
}

// ===== REGISTER TARGET INFERENCE =====

/**
 * Infer target register from opcode patterns
 */
static inline uint8_t pla_infer_target_register(uint8_t opcode) {
    const uint8_t aaa = pla_get_aaa_bits(opcode);
    const uint8_t cc = pla_get_instruction_group(opcode);
    
    // Handle specific patterns
    if (opcode == 0xA9 || opcode == 0xA5 || opcode == 0xAD) return 0; // LDA -> A
    if (opcode == 0xA2 || opcode == 0xA6 || opcode == 0xAE) return 1; // LDX -> X  
    if (opcode == 0xA0 || opcode == 0xA4 || opcode == 0xAC) return 2; // LDY -> Y
    
    // Use pattern recognition for other cases
    if (cc == 0x01) {
        return 0; // Most Group 01 instructions affect accumulator
    } else if (cc == 0x02 && aaa == 0x05) {
        // LDX variants
        return 1;
    } else if (cc == 0x00 && (opcode & 0x0F) == 0x00 && aaa == 5) {
        // LDY variants  
        return 2;
    }
    
    return 0; // Default to accumulator
}

// ===== FLAG EFFECTS INFERENCE =====

/**
 * Determine which status flags are affected by instruction
 */
static inline uint8_t pla_infer_flag_effects(uint8_t opcode) {
    const uint8_t cc = pla_get_instruction_group(opcode);
    const uint8_t aaa = pla_get_aaa_bits(opcode);
    
    uint8_t flags = 0;
    
    if (cc == 0x01) {
        // Most Group 01 operations affect N,Z
        flags |= 0x82; // N and Z flags
        
        // ADC and SBC affect all arithmetic flags
        if (aaa == 0x03 || aaa == 0x07) {
            flags |= 0x41; // C and V flags
        }
    } else if (cc == 0x02) {
        // RMW operations affect N,Z
        if (aaa <= 0x03 || aaa >= 0x06) {
            flags |= 0x82; // N and Z flags
        }
        
        // Shifts affect carry
        if (aaa <= 0x03) {
            flags |= 0x01; // C flag
        }
    }
    
    // Special cases
    if (opcode == 0x18 || opcode == 0x38) flags = 0x01; // CLC/SEC (C only)
    if (opcode == 0x58 || opcode == 0x78) flags = 0x04; // CLI/SEI (I only)
    if (opcode == 0xB8 || opcode == 0xF8) flags = 0x40; // CLV/SED (V/D only)
    
    return flags;
}

// ===== PLA STATISTICS AND VALIDATION =====

/**
 * PLA lookup statistics for optimization validation
 */
typedef struct {
    uint16_t legal_opcodes;
    uint16_t illegal_opcodes;
    uint16_t useful_illegal_opcodes;
    uint16_t jam_opcodes;
    uint16_t unstable_opcodes;
    uint32_t total_lookup_table_size;
    float compression_ratio;
} pla_lookup_stats_t;

/**
 * Get PLA lookup system statistics
 */
pla_lookup_stats_t pla_get_lookup_stats(void);

/**
 * Validate PLA lookup completeness (all 256 opcodes handled)
 */
bool pla_validate_completeness(void);

/**
 * Test PLA lookup performance (for benchmarking)
 */
uint64_t pla_benchmark_lookup_speed(uint32_t iterations);

// ===== EXTERNAL COMPLETE INSTRUCTION TABLE =====

/**
 * The complete 256-entry instruction table including all illegal opcodes
 * This extends the basic table from instruction_table.c
 */
extern const instruction_definition_ultra_t instruction_table_complete[256];

#endif // MOS6510_CYCLE_PLA_LOOKUP_H