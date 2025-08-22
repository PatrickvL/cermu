#ifndef MOS6510_CYCLE_REGISTERS_H
#define MOS6510_CYCLE_REGISTERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

// MOS6510 Register Array Layout
// Based on visual6502 internal structure (spec lines 80-97)
// 16-register array includes both architectural and internal registers

// Architectural Registers (0-6)
#define REG_A       0   // Accumulator
#define REG_X       1   // X Index Register  
#define REG_Y       2   // Y Index Register
#define REG_P       3   // Processor Status Register
#define REG_SP      4   // Stack Pointer
#define REG_PCL     5   // Program Counter Low
#define REG_PCH     6   // Program Counter High

// Visual6502 Internal Registers (7-15)
#define REG_DL      7   // Data Latch (visual6502 DL)
#define REG_DOR     8   // Data Output Register (visual6502 DOR)
#define REG_SB      9   // Special Bus internal register
#define REG_ADL    10   // Address Low internal bus
#define REG_ADH    11   // Address High internal bus
#define REG_ABL    12   // Address Bus Low latch
#define REG_ABH    13   // Address Bus High latch
#define REG_AC     14   // ALU input A cache
#define REG_ADD    15   // ALU input B cache

// Register array size
#define MOS6510_REGISTER_COUNT 16

// Processor Status Register (REG_P) bit definitions
#define FLAG_C      0x01    // Carry flag
#define FLAG_Z      0x02    // Zero flag
#define FLAG_I      0x04    // Interrupt disable flag
#define FLAG_D      0x08    // Decimal mode flag
#define FLAG_B      0x10    // Break flag (software interrupt)
#define FLAG_UNUSED 0x20    // Unused flag (always 1)
#define FLAG_V      0x40    // Overflow flag
#define FLAG_N      0x80    // Negative flag

// Register categories for debugging and validation
typedef enum {
    REG_CATEGORY_ARCHITECTURAL = 0,  // A, X, Y, P, SP, PCL, PCH
    REG_CATEGORY_INTERNAL_DATA = 1,  // DL, DOR, SB
    REG_CATEGORY_INTERNAL_ADDR = 2,  // ADL, ADH, ABL, ABH
    REG_CATEGORY_INTERNAL_ALU = 3    // AC, ADD
} register_category_t;

 // Register properties for hardware-accurate behavior (bit-packed flags)
 // Flags layout in 'flags' byte:
 //  - bits 0-1: register_category_t (0..3)
 //  - bit 4   : address related (cross-cutting; e.g., SP/PCL/PCH)
 //  - bit 5   : ALU related (cross-cutting; e.g., A/P/SB)
 #define REG_PROP_CATEGORY_MASK        0x03u  // Mask for register category
 #define REG_PROP_FLAG_ADDRESS         0x10u  // Set for address calculation registers
 #define REG_PROP_FLAG_ALU             0x20u  // Set for ALU operation registers

// Helpers to encode/decode category in flags
#define REG_PROP_CAT(cat) ((uint8_t)((cat) & REG_PROP_CATEGORY_MASK))
#define REG_PROP_GET_CATEGORY(flags)  ((register_category_t)((flags) & REG_PROP_CATEGORY_MASK))
#define REG_PROP_SET_CATEGORY(flags, cat) \
    ( (uint8_t)(((flags) & (uint8_t)~REG_PROP_CATEGORY_MASK) | REG_PROP_CAT(cat)) )

typedef struct {
    const char* name;      // Human-readable register name
    uint8_t     flags;     // Category (bits 0-1) + property flags (bits 2..)
    uint8_t     reset_value; // Value after reset
} register_properties_t;

// Register properties table (defined in corresponding .c file)
extern const register_properties_t register_properties[MOS6510_REGISTER_COUNT];

 // Convenience macros for register classification (deduplicated with category)
 #define REG_CATEGORY_OF(reg)         REG_PROP_GET_CATEGORY(register_properties[(reg)].flags)
 #define IS_ARCHITECTURAL_REG(reg)    (REG_CATEGORY_OF(reg) == REG_CATEGORY_ARCHITECTURAL)
 // "internal bus" covers internal data + internal address categories
 #define IS_INTERNAL_BUS_REG(reg)     (REG_CATEGORY_OF(reg) == REG_CATEGORY_INTERNAL_DATA || \
                                       REG_CATEGORY_OF(reg) == REG_CATEGORY_INTERNAL_ADDR)
 // Address-related if internal address category OR explicitly flagged (e.g., SP/PCL/PCH)
 #define IS_ADDRESS_REG(reg)          (REG_CATEGORY_OF(reg) == REG_CATEGORY_INTERNAL_ADDR || \
                                       (register_properties[(reg)].flags & REG_PROP_FLAG_ADDRESS))
 // ALU-related if internal ALU category OR explicitly flagged (e.g., A/P/SB)
 #define IS_ALU_REG(reg)              (REG_CATEGORY_OF(reg) == REG_CATEGORY_INTERNAL_ALU || \
                                       (register_properties[(reg)].flags & REG_PROP_FLAG_ALU))

// Register validation macros
#define IS_VALID_REG(reg)            ((reg) < MOS6510_REGISTER_COUNT)
#define VALIDATE_REG(reg)            assert(IS_VALID_REG(reg))

// Forward declarations to avoid circular includes
typedef struct mos6510_state_s mos6510_state_t;

// Register management functions (implemented in mos6510_registers.c)
void mos6510_registers_reset(mos6510_state_t *cpu);
bool mos6510_registers_validate(const mos6510_state_t *cpu);
void mos6510_registers_dump(const mos6510_state_t *cpu, char *buffer, size_t buffer_size);

// Inline accessor to decode category from the packed flags
static inline register_category_t mos6510_register_get_category(uint8_t reg) {
    VALIDATE_REG(reg);
    return REG_PROP_GET_CATEGORY(register_properties[reg].flags);
}

#endif // MOS6510_CYCLE_REGISTERS_H