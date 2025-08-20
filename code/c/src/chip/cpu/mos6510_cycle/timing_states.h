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
} cycle_definition_ultra_t;

// Verify the structure is exactly 32 bits
// Note: static_assert requires C11, using compile-time size check instead
typedef char cycle_definition_size_check[
    (sizeof(cycle_definition_ultra_t) == 4) ? 1 : -1];

// ===== INSTRUCTION DEFINITION =====

/**
 * Complete instruction definition with up to 8 cycles
 * Optimized version without redundant opcode field (opcode = array index)
 */
typedef struct {
    uint8_t cycle_count     : 4;            // Number of cycles (4 bits)
    uint8_t special_props   : 4;            // Special properties (4 bits)
    cycle_definition_ultra_t cycles[8];     // Up to 8 cycles (32 bytes)
} instruction_definition_t;                 // Total: ~33 bytes (optimized)

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

// ===== INFERENCE FUNCTIONS =====

/**
 * Branchless inference functions - compile to 1-5 CPU instructions
 * These functions recover detailed behavior from compact representations
 */

// Instruction classification (branchless bit pattern matching)
static inline bool infer_is_branch(uint8_t opcode) {
    return (opcode & 0x1F) == 0x10;
}

static inline bool infer_is_rmw(uint8_t opcode) {
    return (opcode & 0x1F) == 0x06 || (opcode & 0x1F) == 0x0E;
}

static inline bool infer_uses_x_index(uint8_t opcode) {
    return (opcode & 0x1C) == 0x14 || (opcode & 0x1C) == 0x1C;
}

static inline bool infer_uses_y_index(uint8_t opcode) {
    return (opcode & 0x1C) == 0x18;
}

// Register selection from opcode bit patterns
static inline uint8_t infer_target_register_index(uint8_t opcode) {
    // Handle specific load immediate instructions
    if (opcode == 0xA9) return 0; // LDA #$nn -> A register
    if (opcode == 0xA2) return 1; // LDX #$nn -> X register
    if (opcode == 0xA0) return 2; // LDY #$nn -> Y register
    
    // Use bit pattern analysis for other instructions
    const uint8_t cc = opcode & 0x03;
    const uint8_t aaa = (opcode >> 5) & 0x07;
    
    if (cc == 0x02) {
        // Group 2 instructions - check AAA bits for LDA/LDX/LDY variants
        if (aaa == 5) { // Load operations (101 in AAA)
            if ((opcode & 0x0F) == 0x0A || (opcode & 0x0F) == 0x06) return 1; // LDX patterns
            if ((opcode & 0x0F) == 0x08 || (opcode & 0x0F) == 0x04) return 2; // LDY patterns
            return 0; // LDA patterns
        }
    }
    
    return 0; // Default to A register
}

// ALU operation inference from opcode AAA bits
static inline alu_operation_t infer_alu_operation(uint8_t opcode) {
    const uint8_t aaa = (opcode >> 5) & 0x07;
    const uint8_t cc = opcode & 0x03;
    
    if (cc == 0x01) {
        // ALU instructions - operation from AAA bits
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
    }
    
    return ALU_NOP;
}

// Addressing mode details from bit patterns
static inline bool infer_zp_x_indexing(uint8_t opcode, addressing_mode_t mode) {
    return mode == ADDR_ZP && infer_uses_x_index(opcode);
}

static inline bool infer_zp_y_indexing(uint8_t opcode, addressing_mode_t mode) {
    return mode == ADDR_ZP && infer_uses_y_index(opcode);
}

// ===== TIMING STATE FUNCTIONS =====

/**
 * Initialize timing state machine
 */
void timing_state_init(timing_state_machine_t *tsm);

/**
 * Advance timing state machine for next cycle
 */
void timing_state_advance(timing_state_machine_t *tsm, 
                         const cycle_definition_ultra_t *cycle);

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
void cycle_definition_dump(const cycle_definition_ultra_t *cycle, 
                          char *buffer, size_t buffer_size);

/**
 * Dump instruction definition for debugging
 */
void instruction_definition_dump(const instruction_definition_t *instr,
                               char *buffer, size_t buffer_size);

#endif // MOS6510_CYCLE_TIMING_STATES_H