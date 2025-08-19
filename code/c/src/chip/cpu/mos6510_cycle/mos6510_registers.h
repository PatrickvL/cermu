#ifndef MOS6510_CYCLE_REGISTERS_H
#define MOS6510_CYCLE_REGISTERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

// Register properties for hardware-accurate behavior
typedef struct {
    const char* name;               // Human-readable register name
    register_category_t category;   // Register category
    bool is_architectural;          // True for user-visible registers
    bool is_internal_bus;          // True for internal bus registers
    bool is_address_related;       // True for address calculation registers
    bool is_alu_related;           // True for ALU operation registers
    uint8_t reset_value;           // Value after reset
} register_properties_t;

// Register properties table (defined in corresponding .c file)
extern const register_properties_t register_properties[MOS6510_REGISTER_COUNT];

// Convenience macros for register classification
#define IS_ARCHITECTURAL_REG(reg)    (register_properties[reg].is_architectural)
#define IS_INTERNAL_BUS_REG(reg)     (register_properties[reg].is_internal_bus)
#define IS_ADDRESS_REG(reg)          (register_properties[reg].is_address_related)
#define IS_ALU_REG(reg)              (register_properties[reg].is_alu_related)

// Register validation macros
#define IS_VALID_REG(reg)            ((reg) < MOS6510_REGISTER_COUNT)
#define VALIDATE_REG(reg)            assert(IS_VALID_REG(reg))

// Forward declarations to avoid circular includes
typedef struct mos6510_state_s mos6510_state_t;

// Register management functions (implemented in mos6510_registers.c)
void mos6510_registers_reset(mos6510_state_t *cpu);
bool mos6510_registers_validate(const mos6510_state_t *cpu);
void mos6510_registers_dump(const mos6510_state_t *cpu, char *buffer, size_t buffer_size);
register_category_t mos6510_register_get_category(uint8_t reg);

#endif // MOS6510_CYCLE_REGISTERS_H