#ifndef MOS6510_CYCLE_INSTRUCTION_TABLE_H
#define MOS6510_CYCLE_INSTRUCTION_TABLE_H

#include "timing_states.h"
#include <stdint.h>

/**
 * MOS6510 Ultra-Compact Instruction Table
 * Based on visual6502.org analysis with 93% storage reduction optimization
 * 
 * This implements the complete 256-entry instruction table using the
 * ultra-compact 32-bit cycle definitions from the specification.
 */

// ===== INSTRUCTION PROPERTIES =====

/**
 * Special instruction properties (4 bits)
 */
#define INSTR_PROP_ILLEGAL        0x01    // Illegal opcode
#define INSTR_PROP_BRANCH         0x02    // Branch instruction
#define INSTR_PROP_RMW            0x04    // Read-modify-write
#define INSTR_PROP_VARIABLE_CYCLE 0x08    // Variable cycle count

// ===== PLA LOOKUP SYSTEM =====

/**
 * Direct O(1) PLA lookup function
 * This is the core of the ultra-compact system - no searching needed
 */
static inline const instruction_definition_ultra_t* pla_lookup(uint8_t opcode) {
    extern const instruction_definition_ultra_t instruction_table[256];
    return &instruction_table[opcode];
}

/**
 * Get current cycle definition for a CPU state
 */
static inline const cycle_definition_ultra_t* get_current_cycle_definition(
    const instruction_definition_ultra_t* instr, uint8_t cycle_position) {
    
    if (cycle_position >= instr->cycle_count || cycle_position >= 8) {
        return NULL; // Invalid cycle position
    }
    
    return &instr->cycles[cycle_position];
}

// ===== BRANCHLESS INSTRUCTION CLASSIFICATION =====

/**
 * Ultra-fast instruction classification using bit patterns
 * These functions compile to 1-5 CPU instructions
 */

/**
 * Get instruction group from CC bits (spec lines 238-243)
 */
static inline uint8_t get_instruction_group(uint8_t opcode) {
    return opcode & 0x03; // CC bits
}

/**
 * Check if instruction is in Group 1 (ALU operations)
 */
static inline bool is_group1_instruction(uint8_t opcode) {
    return (opcode & 0x03) == 0x01;
}

/**
 * Check if instruction is in Group 2 (RMW/Load/Store)
 */
static inline bool is_group2_instruction(uint8_t opcode) {
    return (opcode & 0x03) == 0x02;
}

/**
 * Check if instruction is in Group 0 (Control)
 */
static inline bool is_group0_instruction(uint8_t opcode) {
    return (opcode & 0x03) == 0x00;
}

/**
 * Check if instruction activates both G1 and G2 (illegal opcodes)
 */
static inline bool is_illegal_opcode(uint8_t opcode) {
    return (opcode & 0x03) == 0x03;
}

// ===== ADDRESSING MODE INFERENCE =====

/**
 * Get BBB bits for addressing mode determination
 */
static inline uint8_t get_bbb_bits(uint8_t opcode) {
    return (opcode >> 2) & 0x07;
}

/**
 * Get AAA bits for operation determination  
 */
static inline uint8_t get_aaa_bits(uint8_t opcode) {
    return (opcode >> 5) & 0x07;
}

/**
 * Infer exact addressing mode with indexing details
 */
addressing_mode_t infer_full_addressing_mode(uint8_t opcode);

/**
 * Check if addressing mode uses X indexing
 */
bool infer_uses_x_indexing(uint8_t opcode, addressing_mode_t base_mode);

/**
 * Check if addressing mode uses Y indexing
 */
bool infer_uses_y_indexing(uint8_t opcode, addressing_mode_t base_mode);

// ===== CYCLE COUNT OPTIMIZATION =====

/**
 * Get base cycle count (before conditional adjustments)
 */
static inline uint8_t get_base_cycle_count(uint8_t opcode) {
    const instruction_definition_ultra_t* instr = pla_lookup(opcode);
    return instr->cycle_count;
}

/**
 * Check if instruction has variable cycle timing
 */
static inline bool has_variable_timing(uint8_t opcode) {
    const instruction_definition_ultra_t* instr = pla_lookup(opcode);
    return (instr->special_props & INSTR_PROP_VARIABLE_CYCLE) != 0;
}

/**
 * Determine additional cycles for page crossing
 */
uint8_t calculate_page_cross_penalty(uint8_t opcode, uint16_t base_addr, 
                                   uint16_t final_addr);

/**
 * Determine additional cycles for branch taken
 */
uint8_t calculate_branch_penalty(uint8_t opcode, bool branch_taken, 
                               bool page_crossed);

// ===== REGISTER TARGET INFERENCE =====

/**
 * Get target register index from opcode bit patterns
 * Uses the 16-register array indexing system
 */
uint8_t infer_target_register_from_opcode(uint8_t opcode);

/**
 * Get source register index for operations
 */
uint8_t infer_source_register_from_opcode(uint8_t opcode);

/**
 * Check if operation affects accumulator
 */
static inline bool affects_accumulator(uint8_t opcode) {
    // Most Group 1 instructions affect accumulator
    return is_group1_instruction(opcode) || 
           (opcode == 0xA9 || opcode == 0xA5 || opcode == 0xAD); // LDA variants
}

/**
 * Check if operation affects X register
 */
static inline bool affects_x_register(uint8_t opcode) {
    return (opcode & 0x0F) == 0x0A ||  // TAX, TXA, DEX, INX
           (opcode == 0xA2 || opcode == 0xA6 || opcode == 0xAE); // LDX variants
}

/**
 * Check if operation affects Y register
 */
static inline bool affects_y_register(uint8_t opcode) {
    return (opcode & 0x0F) == 0x08 ||  // TAY, TYA, DEY, INY
           (opcode == 0xA0 || opcode == 0xA4 || opcode == 0xAC); // LDY variants
}

// ===== FLAG EFFECTS INFERENCE =====

/**
 * Determine which flags are affected by instruction
 */
uint8_t infer_flag_effects(uint8_t opcode);

/**
 * Check if instruction affects N and Z flags
 */
static inline bool affects_nz_flags(uint8_t opcode) {
    // Most ALU operations and loads affect N,Z
    // Load instructions: LDA, LDX, LDY patterns
    bool is_load = ((opcode & 0x1F) == 0x01 && (opcode & 0xE0) == 0xA0) ||  // LDA variants
                   ((opcode & 0x1F) == 0x02 && (opcode & 0xE0) == 0xA0) ||  // LDX variants
                   ((opcode & 0x1F) == 0x00 && (opcode & 0xE0) == 0xA0);    // LDY variants
    
    return is_group1_instruction(opcode) ||
           is_load ||
           (opcode & 0x0F) == 0x0A; // Shifts, increments
}

/**
 * Check if instruction affects carry flag
 */
static inline bool affects_carry_flag(uint8_t opcode) {
    const uint8_t aaa = get_aaa_bits(opcode);
    return (is_group1_instruction(opcode) && (aaa == 3 || aaa == 7)) || // ADC, SBC
           (opcode & 0x0F) == 0x0A ||  // Shifts
           (opcode == 0x18 || opcode == 0x38); // CLC, SEC
}

// ===== ILLEGAL OPCODE SUPPORT =====

/**
 * Check if illegal opcode does "useful work"
 */
bool is_useful_illegal_opcode(uint8_t opcode);

/**
 * Get illegal opcode behavior pattern
 */
uint8_t get_illegal_opcode_pattern(uint8_t opcode);

/**
 * Execute illegal opcode operation
 */
void execute_illegal_opcode(uint8_t opcode, void* cpu_state);

// ===== TABLE GENERATION AND VALIDATION =====

/**
 * Initialize the complete instruction table
 * This generates all 256 entries using pattern recognition
 */
void instruction_table_init(void);

/**
 * Validate instruction table completeness
 */
bool instruction_table_validate(void);

/**
 * Get instruction table storage statistics
 */
typedef struct {
    uint32_t total_size_bytes;
    uint32_t original_size_estimate;
    float compression_ratio;
    uint32_t legal_opcodes;
    uint32_t illegal_opcodes;
    uint32_t useful_illegal_opcodes;
} instruction_table_stats_t;

/**
 * Get table statistics for optimization validation
 */
instruction_table_stats_t get_instruction_table_stats(void);

// ===== EXTERNAL INSTRUCTION TABLE DECLARATION =====

/**
 * The complete 256-entry instruction table
 * This is the core data structure - ultra-compact and complete
 */
extern const instruction_definition_ultra_t instruction_table[256];

#endif // MOS6510_CYCLE_INSTRUCTION_TABLE_H