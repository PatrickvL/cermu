#ifndef MOS6510_CYCLE_PLA_LOOKUP_H
#define MOS6510_CYCLE_PLA_LOOKUP_H

#include "instruction_table.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * MOS6510 PLA Lookup System
 * Direct O(1) array access implementation
 * Based on visual6502.org 130×21 PLA logic analysis
 *
 * All 256 opcodes (legal + illegal) are handled through the instruction table
 */

// ===== CORE PLA LOOKUP FUNCTIONS =====

/**
 * Primary PLA decode function - O(1) direct array access
 * This replaces the complex PLA logic with a pre-computed lookup table
 */
static inline const instruction_definition_t* pla_decode(uint8_t opcode) {
    return pla_lookup(opcode);
}

/**
 * Fast opcode validation - check if opcode is implemented
 */
static inline bool pla_is_valid_opcode(uint8_t opcode) {
    const instruction_definition_t* instr = pla_lookup(opcode);
    return INSTR_GET_CYCLE_COUNT(instr) > 0;
}

/**
 * Get instruction type classification from PLA decode
 */
typedef enum {
    PLA_TYPE_LEGAL = 0,
    PLA_TYPE_ILLEGAL = 1,
    PLA_TYPE_INVALID = 2
} pla_instruction_type_t;

/**
 * Classify instruction based on PLA decode result
 */
pla_instruction_type_t pla_classify_instruction(uint8_t opcode);

// ===== BRANCHLESS INSTRUCTION CLASSIFICATION =====

/**
 * Ultra-fast branchless classification using bit manipulation
 * All functions compile to 1-5 CPU instructions
 */

/**
 * Check if opcode uses absolute addressing mode
 */
static inline bool pla_is_absolute_mode(uint8_t opcode) {
    const uint8_t bbb = get_bbb_bits(opcode);
    return (bbb == 3) || (bbb == 7); // 011 or 111
}

/**
 * Check if opcode uses zero-page addressing
 */
static inline bool pla_is_zero_page_mode(uint8_t opcode) {
    const uint8_t bbb = get_bbb_bits(opcode);
    return (bbb == 1) || (bbb == 5); // 001 or 101
}

/**
 * Check if opcode uses immediate addressing
 */
static inline bool pla_is_immediate_mode(uint8_t opcode) {
    const uint8_t bbb = get_bbb_bits(opcode);
    return (bbb == 2) && is_group1_instruction(opcode);
}

/**
 * Check if opcode uses indexed addressing (X or Y)
 */
static inline bool pla_uses_indexing(uint8_t opcode) {
    const uint8_t bbb = get_bbb_bits(opcode);
    return (bbb == 5) || (bbb == 7) || (bbb == 4) || (bbb == 6);
}

/**
 * Check if instruction modifies memory (RMW or Store)
 */
static inline bool pla_modifies_memory(uint8_t opcode) {
    const instruction_definition_t* instr = pla_lookup(opcode);
    return INSTR_GET_IS_RMW(instr); // Store operations are inferred from ALU operation
}

/**
 * Check if instruction is a branch
 */
static inline bool pla_is_branch(uint8_t opcode) {
    return (opcode & 0x1F) == 0x10; // All branches follow xx010000 pattern
}

/**
 * Check if instruction affects processor status flags
 */
static inline bool pla_affects_flags(uint8_t opcode) {
    const instruction_definition_t* instr = pla_lookup(opcode);
    return INSTR_GET_AFFECTS_FLAGS(instr) != 0;
}

// ===== ADVANCED PLA DECODE FUNCTIONS =====

/**
 * Get effective addressing mode after PLA decode
 */
addressing_mode_t pla_get_effective_addressing_mode(uint8_t opcode);

/**
 * Get ALU operation from PLA decode
 */
alu_operation_t pla_get_alu_operation(uint8_t opcode);

/**
 * Get data flow specification from PLA decode
 */
typedef struct {
    uint8_t source_register;      // Source register index
    uint8_t destination_register; // Destination register index
    bool uses_memory;            // Operation involves memory
    bool affects_accumulator;    // Operation affects A register
} pla_data_flow_t;

/**
 * Determine data flow from PLA decode
 */
pla_data_flow_t pla_get_data_flow(uint8_t opcode);

/**
 * Get cycle count including conditional cycles
 */
uint8_t pla_get_total_cycle_count(uint8_t opcode, bool page_crossed, bool branch_taken);

// ===== PLA TIMING ANALYSIS =====

/**
 * Check if instruction has conditional timing
 */
static inline bool pla_has_conditional_timing(uint8_t opcode) {
    return has_variable_timing(opcode);
}

/**
 * Get base timing without conditional adjustments
 */
static inline uint8_t pla_get_base_timing(uint8_t opcode) {
    return get_base_cycle_count(opcode);
}

/**
 * Calculate conditional timing adjustment
 */
typedef struct {
    uint8_t page_cross_penalty;   // Additional cycles if page crossed
    uint8_t branch_taken_penalty; // Additional cycles if branch taken
    uint8_t total_adjustment;     // Total additional cycles
} pla_timing_adjustment_t;

/**
 * Calculate all timing adjustments for an instruction
 */
pla_timing_adjustment_t pla_calculate_timing_adjustment(
    uint8_t opcode, uint16_t base_addr, uint16_t target_addr, bool branch_condition);

// ===== PLA VALIDATION AND DEBUGGING =====

/**
 * Validate PLA lookup table consistency
 */
bool pla_validate_lookup_table(void);

/**
 * Get PLA decode statistics
 */
typedef struct {
    uint32_t total_opcodes;
    uint32_t legal_opcodes;
    uint32_t illegal_opcodes;
    uint32_t invalid_opcodes;
} pla_decode_stats_t;

/**
 * Get instruction table coverage information
 */
typedef struct {
    uint32_t total_opcodes;
    uint32_t implemented_opcodes;
    uint32_t legal_opcodes;
    uint32_t illegal_opcodes;
    float coverage_percentage;
} pla_coverage_stats_t;

/**
 * Analyze PLA decode table coverage
 */
pla_decode_stats_t pla_get_decode_statistics(void);

/**
 * Analyze instruction table coverage
 */
pla_coverage_stats_t pla_get_coverage_statistics(void);

/**
 * Debug function: print opcode decode information
 */
void pla_debug_print_opcode(uint8_t opcode);

/**
 * Performance test: measure PLA lookup speed
 */
typedef struct {
    uint64_t total_lookups;
    uint64_t total_cycles;
    double average_cycles_per_lookup;
} pla_performance_stats_t;

/**
 * Run PLA lookup performance benchmark
 */
pla_performance_stats_t pla_benchmark_lookup_performance(uint32_t iterations);

#endif // MOS6510_CYCLE_PLA_LOOKUP_H