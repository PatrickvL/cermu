#ifndef MOS6510_CYCLE_OPCODE_MAPPING_H
#define MOS6510_CYCLE_OPCODE_MAPPING_H

#include <stdint.h>
#include <stdbool.h>
#include "mos6510_registers.h"

// Forward declarations to avoid circular includes
typedef struct mos6510_state_s mos6510_state_t;

// Opcode Bit-Pattern to Register Index Mapping
// Based on spec lines 340-343 for shared operation code through register indexing
// Leverages 6502 opcode bit patterns for direct register selection

// ===== OPCODE BIT PATTERN DEFINITIONS =====

// Standard 6502 opcode bit patterns
#define OPCODE_CC_MASK      0x03    // CC bits (bits 0-1)
#define OPCODE_BBB_MASK     0x1C    // BBB bits (bits 2-4) 
#define OPCODE_AAA_MASK     0xE0    // AAA bits (bits 5-7)

#define OPCODE_CC_SHIFT     0
#define OPCODE_BBB_SHIFT    2
#define OPCODE_AAA_SHIFT    5

// Extract bit patterns from opcode
#define OPCODE_GET_CC(op)   (((op) & OPCODE_CC_MASK) >> OPCODE_CC_SHIFT)
#define OPCODE_GET_BBB(op)  (((op) & OPCODE_BBB_MASK) >> OPCODE_BBB_SHIFT)
#define OPCODE_GET_AAA(op)  (((op) & OPCODE_AAA_MASK) >> OPCODE_AAA_SHIFT)

// ===== DIRECT REGISTER MAPPING FUNCTIONS =====

// Map opcode bit patterns directly to register indexes
// Enables shared operation code through register indexing

// Target register selection based on opcode bit pattern
// Used for operations like LDX/LDY, STX/STY, etc.
static inline uint8_t opcode_to_target_register(uint8_t opcode) {
    // Analyze actual 6502 opcodes:
    // LDY immediate = A0 (AAA=101, CC=00)
    // LDX immediate = A2 (AAA=101, CC=10)
    // Need to check the CC bits to distinguish
    uint8_t aaa = OPCODE_GET_AAA(opcode);
    uint8_t cc = OPCODE_GET_CC(opcode);
    
    if (aaa == 5) {  // AAA = 101 (LDX/LDY family)
        if (cc == 0) return REG_Y;  // LDY family (CC=00)
        if (cc == 2) return REG_X;  // LDX family (CC=10)
    }
    
    // Default fallback - bit 0 determines X vs Y for other cases
    return (opcode & 1) ? REG_X : REG_Y;
}

// Source register selection for store operations
// Maps store instructions to their source registers
static inline uint8_t opcode_to_source_register(uint8_t opcode) {
    // Analyze actual 6502 store opcodes:
    // STA absolute = 8D (AAA=100, CC=01)
    // STX absolute = 8E (AAA=100, CC=10)
    // STY absolute = 8C (AAA=100, CC=00)
    uint8_t aaa = OPCODE_GET_AAA(opcode);
    uint8_t cc = OPCODE_GET_CC(opcode);
    
    if (aaa == 4) {  // AAA = 100 (store family)
        if (cc == 0) return REG_Y;  // STY family (CC=00)
        if (cc == 1) return REG_A;  // STA family (CC=01)
        if (cc == 2) return REG_X;  // STX family (CC=10)
    }
    
    return REG_A;  // Default to accumulator
}

// ALU operation target register (usually accumulator, but can vary)
static inline uint8_t opcode_to_alu_target_register(uint8_t opcode) {
    // Most ALU operations target the accumulator
    // Some illegal opcodes may target other registers
    if ((opcode & 0x1F) == 0x1B) return REG_Y;  // LAX variants
    if ((opcode & 0x1F) == 0x1F) return REG_X;  // Some illegal variants
    return REG_A;  // Standard ALU operations
}

// ===== ADDRESSING MODE REGISTER MAPPING =====

// Get index register for indexed addressing modes
static inline uint8_t opcode_to_index_register(uint8_t opcode) {
    uint8_t bbb = OPCODE_GET_BBB(opcode);
    
    switch (bbb) {
        case 4:  // Absolute,X / Zero Page,X
        case 6:  // Zero Page,X  
            return REG_X;
        case 5:  // Absolute,Y / Zero Page,Y
        case 7:  // Zero Page,Y
            return REG_Y;
        default:
            return REG_A;  // No indexing
    }
}

// ===== INSTRUCTION CLASSIFICATION =====

// Branchless instruction classification functions
// Based on opcode bit patterns for efficient decode

static inline bool opcode_is_branch(uint8_t opcode) {
    return (opcode & 0x1F) == 0x10;
}

static inline bool opcode_is_rmw(uint8_t opcode) {
    return (opcode & 0x1F) == 0x06 || (opcode & 0x1F) == 0x0E;
}

static inline bool opcode_is_store(uint8_t opcode) {
    // Store instructions have specific patterns:
    // STA zp = 85 (AAA=100, CC=01)
    // STX zp = 86 (AAA=100, CC=10)
    // STY zp = 84 (AAA=100, CC=00)
    // All store instructions have AAA=100 (4)
    uint8_t aaa = OPCODE_GET_AAA(opcode);
    return (aaa == 4);  // All store instructions have AAA=100
}

static inline bool opcode_is_load(uint8_t opcode) {
    // Load instructions:
    // LDA zp = A5 (AAA=101, CC=01)
    // LDX zp = A6 (AAA=101, CC=10)
    // LDY zp = A4 (AAA=101, CC=00)
    // All load instructions have AAA=101 (5)
    uint8_t aaa = OPCODE_GET_AAA(opcode);
    return (aaa == 5);  // All load instructions have AAA=101
}

static inline bool opcode_is_alu(uint8_t opcode) {
    return OPCODE_GET_CC(opcode) == 0x01;
}

static inline bool opcode_is_control(uint8_t opcode) {
    return OPCODE_GET_CC(opcode) == 0x00;
}

static inline bool opcode_is_illegal(uint8_t opcode) {
    return OPCODE_GET_CC(opcode) == 0x03;
}

// ===== SHARED OPERATION FUNCTIONS =====

// Generic register-based operations that can be shared across multiple opcodes
// Enable code reuse through register index parameterization
// (Implementations in opcode_mapping.c to avoid circular includes)

void shared_load_operation(mos6510_state_t *cpu, uint8_t target_reg, uint8_t value);
uint8_t shared_store_operation(mos6510_state_t *cpu, uint8_t source_reg);
void shared_increment_operation(mos6510_state_t *cpu, uint8_t target_reg);
void shared_decrement_operation(mos6510_state_t *cpu, uint8_t target_reg);
void shared_compare_operation(mos6510_state_t *cpu, uint8_t target_reg, uint8_t value);

// ===== OPCODE DISPATCH HELPERS =====

// Get all relevant registers for an opcode in one call
// Reduces decode overhead for complex instructions
typedef struct {
    uint8_t target_reg;     // Primary target register
    uint8_t source_reg;     // Source register (for stores, transfers)
    uint8_t index_reg;      // Index register (for indexed addressing)
    uint8_t alu_target_reg; // ALU target register
} opcode_registers_t;

static inline opcode_registers_t opcode_get_registers(uint8_t opcode) {
    opcode_registers_t regs = {
        .target_reg = opcode_to_target_register(opcode),
        .source_reg = opcode_to_source_register(opcode),
        .index_reg = opcode_to_index_register(opcode),
        .alu_target_reg = opcode_to_alu_target_register(opcode)
    };
    return regs;
}

// ===== PERFORMANCE OPTIMIZATION HELPERS =====

// Pre-computed lookup tables for hot paths
extern const uint8_t opcode_to_target_register_table[256];
extern const uint8_t opcode_to_source_register_table[256];
extern const uint8_t opcode_to_index_register_table[256];

// Use lookup tables for maximum performance (when available)
#ifdef USE_OPCODE_LOOKUP_TABLES
    #define FAST_OPCODE_TO_TARGET_REG(op)  (opcode_to_target_register_table[op])
    #define FAST_OPCODE_TO_SOURCE_REG(op)  (opcode_to_source_register_table[op])
    #define FAST_OPCODE_TO_INDEX_REG(op)   (opcode_to_index_register_table[op])
#else
    #define FAST_OPCODE_TO_TARGET_REG(op)  opcode_to_target_register(op)
    #define FAST_OPCODE_TO_SOURCE_REG(op)  opcode_to_source_register(op)
    #define FAST_OPCODE_TO_INDEX_REG(op)   opcode_to_index_register(op)
#endif

#endif // MOS6510_CYCLE_OPCODE_MAPPING_H