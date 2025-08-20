#ifndef MOS6510_CYCLE_TIMING_STATES_H
#define MOS6510_CYCLE_TIMING_STATES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * MOS6510 Ultra-Compact Timing State System
 * Based on visual6502.org analysis with 32-bit cycle definition optimization
 * 
 * This implements the ultra-compact timing system from spec lines 213-223
 * that achieves 93% storage reduction through aggressive bit packing.
 */

// ===== ULTRA-COMPACT TIMING STATES (3 bits) =====

/**
 * Visual6502 timing states compressed to 3 bits
 * Based on hardware analysis, optimized for inference
 */
typedef enum {
    // Core timing states (3 bits can represent 8 states)
    TIMING_T0    = 0,    // Instruction completion
    TIMING_TPLUS = 1,    // T1X special timing  
    TIMING_T2    = 2,    // First execution cycle
    TIMING_T3    = 3,    // Second execution cycle
    TIMING_T4    = 4,    // Third execution cycle
    TIMING_T5    = 5,    // Fourth execution cycle
    TIMING_T1F   = 6,    // Fetch state (SYNC)
    TIMING_VEC   = 7     // Vector/RMW states (inferred from context)
} timing_state_t;

// ===== ADDRESSING MODES (3 bits) =====

/**
 * Addressing modes compressed from 14 → 8 modes
 * Uses 6502 bit pattern inference for X/Y indexing
 */
typedef enum {
    ADDR_IMPLIED     = 0,    // Implied/accumulator
    ADDR_IMMEDIATE   = 1,    // #$nn
    ADDR_ZP          = 2,    // $nn (X/Y inferred from BBB bits)
    ADDR_ABSOLUTE    = 3,    // $nnnn (indexing inferred from opcode)
    ADDR_INDIRECT    = 4,    // ($nnnn), ($nn,X), ($nn),Y
    ADDR_RELATIVE    = 5,    // Branch instructions
    ADDR_STACK       = 6,    // Stack operations
    ADDR_SPECIAL     = 7     // RMW, interrupts, etc.
} addressing_mode_t;

// ===== EXECUTION CONDITIONS (2 bits) =====

/**
 * Execution conditions for conditional cycles
 */
typedef enum {
    COND_ALWAYS      = 0,    // Always execute
    COND_BRANCH      = 1,    // Branch taken/not taken
    COND_PAGE_CROSS  = 2,    // Page boundary crossed
    COND_RMW_PHASE   = 3     // RMW dummy/real write
} condition_t;

// ===== ALU OPERATIONS (4 bits) =====

/**
 * ALU operations compressed from 17 → 8 operations
 * Uses opcode AAA bits for specific inference
 */
typedef enum {
    ALU_NOP          = 0,    // No operation
    ALU_LOGIC        = 1,    // ORA/AND/EOR (specific op from AAA bits)
    ALU_ARITHMETIC   = 2,    // ADC/SBC/CMP
    ALU_SHIFT        = 3,    // ASL/LSR/ROL/ROR (direction inferred)
    ALU_TRANSFER     = 4,    // Load/store/transfer operations
    ALU_INCREMENT    = 5,    // INC/DEC/INX/DEX/INY/DEY
    ALU_BRANCH       = 6,    // Branch condition evaluation
    ALU_STACK        = 7,    // Stack operations
    ALU_INTERRUPT    = 8,    // Interrupt processing
    ALU_BIT_TEST     = 9,    // BIT instruction
    ALU_ILLEGAL      = 10,   // Illegal opcode operations
    ALU_RMW          = 11,   // Read-modify-write
    // Values 12-15 reserved for future use
} alu_operation_t;

// ===== DATA FLOW (2 bits each) =====

/**
 * Data source/destination for ultra-compact representation
 */
typedef enum {
    DATA_NONE        = 0,    // No data operation
    DATA_REGISTER    = 1,    // Register (specific reg inferred from opcode)
    DATA_MEMORY      = 2,    // Memory location
    DATA_IMMEDIATE   = 3     // Immediate value
} data_flow_t;

// ===== CYCLE FLAGS (6 bits) =====

/**
 * Control flags for cycle behavior
 */
#define CYCLE_FLAG_SYNC       0x01    // Generate SYNC pulse (T1F)
#define CYCLE_FLAG_INTERRUPT  0x02    // Interrupt recognition cycle
#define CYCLE_FLAG_RMW_DUMMY  0x04    // RMW dummy write
#define CYCLE_FLAG_PAGE_FIX   0x08    // Page boundary address fix
#define CYCLE_FLAG_BRANCH     0x10    // Branch instruction cycle
#define CYCLE_FLAG_VECTOR     0x20    // Interrupt vector cycle

// ===== ULTRA-COMPACT CYCLE DEFINITION (32 bits) =====

/**
 * Ultra-compact cycle definition - EXACTLY 32 bits
 * This is the core optimization that achieves 93% storage reduction
 */
typedef struct {
    uint32_t timing      : 3;  // Timing state (8 values)
    uint32_t address     : 3;  // Addressing mode (8 values)
    uint32_t condition   : 2;  // Execution condition (4 values)
    uint32_t alu         : 4;  // ALU operation (16 values)
    uint32_t data_src    : 2;  // Data source (4 values)
    uint32_t data_dst    : 2;  // Data destination (4 values)
    uint32_t bus_routing : 8;  // Bus transfer mask
    uint32_t cycle_flags : 6;  // Control flags
    uint32_t reserved    : 2;  // Reserved for alignment
} cycle_definition_t;

// Verify the structure is exactly 32 bits
// Note: static_assert requires C11, using compile-time size check instead
typedef char cycle_definition_size_check[
    (sizeof(cycle_definition_t) == 4) ? 1 : -1];

// ===== INSTRUCTION DEFINITION =====

/**
 * Complete instruction definition with up to 8 cycles
 * Ultra-optimized with precomputed flags using bit masks instead of bitfields
 * This allows compact macro-based initialization of instruction tables
 */
typedef struct {
    uint32_t flags;                         // Packed instruction properties
    cycle_definition_t cycles[8];     // Up to 8 cycles (32 bytes)
} instruction_definition_t;                 // Total: 36 bytes (4 + 32), 32-bit aligned

// ===== INSTRUCTION FLAG BIT MASKS =====

// Bit masks for instruction flags (32-bit packed)
#define INSTR_CYCLE_COUNT_MASK    0x0000000F  // Bits 0-3: Number of cycles (0-15)
#define INSTR_IS_BRANCH_MASK      0x00000010  // Bit 4: Branch instruction
#define INSTR_IS_RMW_MASK         0x00000020  // Bit 5: Read-Modify-Write
#define INSTR_USES_X_INDEX_MASK   0x00000040  // Bit 6: Uses X indexing
#define INSTR_USES_Y_INDEX_MASK   0x00000080  // Bit 7: Uses Y indexing
#define INSTR_TARGET_REG_MASK     0x00000700  // Bits 8-10: Target register (0-7)
#define INSTR_IS_ILLEGAL_MASK     0x00000800  // Bit 11: Illegal opcode
#define INSTR_AFFECTS_FLAGS_MASK  0x0000F000  // Bits 12-15: Flag effects (NZVC)
#define INSTR_ALU_OPERATION_MASK  0x000F0000  // Bits 16-19: ALU operation
#define INSTR_ADDR_MODE_VAR_MASK  0x00300000  // Bits 20-21: Addressing mode variant
#define INSTR_RESERVED_MASK       0xFFC00000  // Bits 22-31: Reserved

// Bit shift positions
#define INSTR_CYCLE_COUNT_SHIFT    0
#define INSTR_IS_BRANCH_SHIFT      4
#define INSTR_IS_RMW_SHIFT         5
#define INSTR_USES_X_INDEX_SHIFT   6
#define INSTR_USES_Y_INDEX_SHIFT   7
#define INSTR_TARGET_REG_SHIFT     8
#define INSTR_IS_ILLEGAL_SHIFT     11
#define INSTR_AFFECTS_FLAGS_SHIFT  12
#define INSTR_ALU_OPERATION_SHIFT  16
#define INSTR_ADDR_MODE_VAR_SHIFT  20

// ===== COMPACT FLAG CONSTRUCTION MACROS =====

/**
 * Compact macro for creating instruction flags
 * Usage: INSTR_FLAGS(cycles, branch, rmw, x_idx, y_idx, target, illegal, flags, alu, addr_var)
 */
#define INSTR_FLAGS(cycles, branch, rmw, x_idx, y_idx, target, illegal, flags, alu, addr_var) \
    (((cycles) << INSTR_CYCLE_COUNT_SHIFT) | \
     ((branch) << INSTR_IS_BRANCH_SHIFT) | \
     ((rmw) << INSTR_IS_RMW_SHIFT) | \
     ((x_idx) << INSTR_USES_X_INDEX_SHIFT) | \
     ((y_idx) << INSTR_USES_Y_INDEX_SHIFT) | \
     ((target) << INSTR_TARGET_REG_SHIFT) | \
     ((illegal) << INSTR_IS_ILLEGAL_SHIFT) | \
     ((flags) << INSTR_AFFECTS_FLAGS_SHIFT) | \
     ((alu) << INSTR_ALU_OPERATION_SHIFT) | \
     ((addr_var) << INSTR_ADDR_MODE_VAR_SHIFT))

// ===== ZERO-COST ACCESSOR MACROS =====

/**
 * Zero-cost accessor macros for precomputed instruction properties
 * These replace expensive runtime opcode bit pattern calculations
 */
#define INSTR_GET_CYCLE_COUNT(instr)     (((instr)->flags & INSTR_CYCLE_COUNT_MASK) >> INSTR_CYCLE_COUNT_SHIFT)
#define INSTR_GET_IS_BRANCH(instr)       (((instr)->flags & INSTR_IS_BRANCH_MASK) >> INSTR_IS_BRANCH_SHIFT)
#define INSTR_GET_IS_RMW(instr)          (((instr)->flags & INSTR_IS_RMW_MASK) >> INSTR_IS_RMW_SHIFT)
#define INSTR_GET_USES_X_INDEX(instr)    (((instr)->flags & INSTR_USES_X_INDEX_MASK) >> INSTR_USES_X_INDEX_SHIFT)
#define INSTR_GET_USES_Y_INDEX(instr)    (((instr)->flags & INSTR_USES_Y_INDEX_MASK) >> INSTR_USES_Y_INDEX_SHIFT)
#define INSTR_GET_TARGET_REG(instr)      (((instr)->flags & INSTR_TARGET_REG_MASK) >> INSTR_TARGET_REG_SHIFT)
#define INSTR_GET_IS_ILLEGAL(instr)      (((instr)->flags & INSTR_IS_ILLEGAL_MASK) >> INSTR_IS_ILLEGAL_SHIFT)
#define INSTR_GET_AFFECTS_FLAGS(instr)   (((instr)->flags & INSTR_AFFECTS_FLAGS_MASK) >> INSTR_AFFECTS_FLAGS_SHIFT)
#define INSTR_GET_ALU_OPERATION(instr)   (((instr)->flags & INSTR_ALU_OPERATION_MASK) >> INSTR_ALU_OPERATION_SHIFT)
#define INSTR_GET_ADDR_MODE_VAR(instr)   (((instr)->flags & INSTR_ADDR_MODE_VAR_MASK) >> INSTR_ADDR_MODE_VAR_SHIFT)

// ===== TIMING STATE MACHINE =====

/**
 * Simplified timing state machine for pipeline support
 */
typedef struct {
    timing_state_t current_state;           // Current timing state (3 bits)
    timing_state_t next_state;              // Next state for pipeline
    uint8_t state_flags;                    // Additional state flags
    bool sync_output;                       // SYNC pin state
} timing_state_machine_t;


// ===== TIMING STATE FUNCTIONS =====

/**
 * Initialize timing state machine
 */
void timing_state_init(timing_state_machine_t *tsm);

/**
 * Advance timing state machine for next cycle
 */
void timing_state_advance(timing_state_machine_t *tsm, 
                         const cycle_definition_t *cycle);

/**
 * Check if SYNC should be asserted
 */
static inline bool timing_state_sync_active(const timing_state_machine_t *tsm) {
    return tsm->sync_output;
}

/**
 * Get current timing state for PLA input
 */
static inline timing_state_t timing_state_current(const timing_state_machine_t *tsm) {
    return tsm->current_state;
}

// ===== DEBUGGING AND INSPECTION =====

/**
 * Get human-readable timing state name
 */
const char* timing_state_name(timing_state_t state);

/**
 * Get addressing mode name
 */
const char* addressing_mode_name(addressing_mode_t mode);

/**
 * Get ALU operation name  
 */
const char* alu_operation_name(alu_operation_t op);

/**
 * Dump cycle definition for debugging
 */
void cycle_definition_dump(const cycle_definition_t *cycle, 
                          char *buffer, size_t buffer_size);

/**
 * Dump instruction definition for debugging
 */
void instruction_definition_dump(const instruction_definition_t *instr,
                               char *buffer, size_t buffer_size);

#endif // MOS6510_CYCLE_TIMING_STATES_H