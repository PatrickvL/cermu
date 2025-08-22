#ifndef MOS6510_CYCLE_REGISTER_ACCESS_H
#define MOS6510_CYCLE_REGISTER_ACCESS_H

#include <stdint.h>
#include <assert.h>
#include "mos6510_registers.h"
#include "mos6510_state.h"

// Hardware-accurate register access macros
// These macros provide the primary interface for register operations
// and enable hardware-accurate behavior modeling

// ===== ARCHITECTURAL REGISTER ACCESS =====

// Accumulator (A)
#define CPU_A(cpu)          ((cpu)->registers[REG_A])
#define SET_CPU_A(cpu, val) ((cpu)->registers[REG_A] = (val))

// X Index Register
#define CPU_X(cpu)          ((cpu)->registers[REG_X])
#define SET_CPU_X(cpu, val) ((cpu)->registers[REG_X] = (val))

// Y Index Register  
#define CPU_Y(cpu)          ((cpu)->registers[REG_Y])
#define SET_CPU_Y(cpu, val) ((cpu)->registers[REG_Y] = (val))

// Processor Status Register (P)
#define CPU_P(cpu)          ((cpu)->registers[REG_P])
#define SET_CPU_P(cpu, val) ((cpu)->registers[REG_P] = (val))

// Stack Pointer (SP)
#define CPU_SP(cpu)         ((cpu)->registers[REG_SP])
#define SET_CPU_SP(cpu, val) ((cpu)->registers[REG_SP] = (val))

// Program Counter (16-bit, split into low/high bytes)
#define CPU_PCL(cpu)        ((cpu)->registers[REG_PCL])
#define CPU_PCH(cpu)        ((cpu)->registers[REG_PCH])
#define SET_CPU_PCL(cpu, val) ((cpu)->registers[REG_PCL] = (val))
#define SET_CPU_PCH(cpu, val) ((cpu)->registers[REG_PCH] = (val))

// Program Counter as 16-bit value
#define CPU_PC(cpu)         ((uint16_t)(CPU_PCL(cpu) | (CPU_PCH(cpu) << 8)))
#define SET_CPU_PC(cpu, val) do { \
    SET_CPU_PCL(cpu, (val) & 0xFF); \
    SET_CPU_PCH(cpu, ((val) >> 8) & 0xFF); \
} while(0)

// ===== VISUAL6502 INTERNAL REGISTER ACCESS =====

// Data Latch (visual6502 DL register)
#define CPU_DL(cpu)         ((cpu)->registers[REG_DL])
#define SET_CPU_DL(cpu, val) ((cpu)->registers[REG_DL] = (val))

// Data Output Register (visual6502 DOR register)
#define CPU_DOR(cpu)        ((cpu)->registers[REG_DOR])
#define SET_CPU_DOR(cpu, val) ((cpu)->registers[REG_DOR] = (val))

// Special Bus (internal data routing)
#define CPU_SB(cpu)         ((cpu)->registers[REG_SB])
#define SET_CPU_SB(cpu, val) ((cpu)->registers[REG_SB] = (val))

// Address Low/High internal buses
#define CPU_ADL(cpu)        ((cpu)->registers[REG_ADL])
#define CPU_ADH(cpu)        ((cpu)->registers[REG_ADH])
#define SET_CPU_ADL(cpu, val) ((cpu)->registers[REG_ADL] = (val))
#define SET_CPU_ADH(cpu, val) ((cpu)->registers[REG_ADH] = (val))

// Address Bus Low/High latches (external output)
#define CPU_ABL(cpu)        ((cpu)->registers[REG_ABL])
#define CPU_ABH(cpu)        ((cpu)->registers[REG_ABH])
#define SET_CPU_ABL(cpu, val) ((cpu)->registers[REG_ABL] = (val))
#define SET_CPU_ABH(cpu, val) ((cpu)->registers[REG_ABH] = (val))

// ALU input caches
#define CPU_AC(cpu)         ((cpu)->registers[REG_AC])
#define CPU_ADD(cpu)        ((cpu)->registers[REG_ADD])
#define SET_CPU_AC(cpu, val) ((cpu)->registers[REG_AC] = (val))
#define SET_CPU_ADD(cpu, val) ((cpu)->registers[REG_ADD] = (val))

// ===== 16-BIT ADDRESS OPERATIONS =====

// Internal address bus as 16-bit value
#define CPU_ADDR_INTERNAL(cpu) ((uint16_t)(CPU_ADL(cpu) | (CPU_ADH(cpu) << 8)))
#define SET_CPU_ADDR_INTERNAL(cpu, val) do { \
    SET_CPU_ADL(cpu, (val) & 0xFF); \
    SET_CPU_ADH(cpu, ((val) >> 8) & 0xFF); \
} while(0)

// External address bus as 16-bit value
#define CPU_ADDR_EXTERNAL(cpu) ((uint16_t)(CPU_ABL(cpu) | (CPU_ABH(cpu) << 8)))
#define SET_CPU_ADDR_EXTERNAL(cpu, val) do { \
    SET_CPU_ABL(cpu, (val) & 0xFF); \
    SET_CPU_ABH(cpu, ((val) >> 8) & 0xFF); \
} while(0)

// ===== PROCESSOR STATUS FLAGS =====

// Individual flag access
#define CPU_FLAG_C(cpu)     (CPU_P(cpu) & FLAG_C)
#define CPU_FLAG_Z(cpu)     (CPU_P(cpu) & FLAG_Z)
#define CPU_FLAG_I(cpu)     (CPU_P(cpu) & FLAG_I)
#define CPU_FLAG_D(cpu)     (CPU_P(cpu) & FLAG_D)
#define CPU_FLAG_B(cpu)     (CPU_P(cpu) & FLAG_B)
#define CPU_FLAG_V(cpu)     (CPU_P(cpu) & FLAG_V)
#define CPU_FLAG_N(cpu)     (CPU_P(cpu) & FLAG_N)

// Flag setting/clearing
#define SET_CPU_FLAG_C(cpu)     (CPU_P(cpu) |= FLAG_C)
#define SET_CPU_FLAG_Z(cpu)     (CPU_P(cpu) |= FLAG_Z)
#define SET_CPU_FLAG_I(cpu)     (CPU_P(cpu) |= FLAG_I)
#define SET_CPU_FLAG_D(cpu)     (CPU_P(cpu) |= FLAG_D)
#define SET_CPU_FLAG_B(cpu)     (CPU_P(cpu) |= FLAG_B)
#define SET_CPU_FLAG_V(cpu)     (CPU_P(cpu) |= FLAG_V)
#define SET_CPU_FLAG_N(cpu)     (CPU_P(cpu) |= FLAG_N)

#define CLR_CPU_FLAG_C(cpu)     (CPU_P(cpu) &= ~FLAG_C)
#define CLR_CPU_FLAG_Z(cpu)     (CPU_P(cpu) &= ~FLAG_Z)
#define CLR_CPU_FLAG_I(cpu)     (CPU_P(cpu) &= ~FLAG_I)
#define CLR_CPU_FLAG_D(cpu)     (CPU_P(cpu) &= ~FLAG_D)
#define CLR_CPU_FLAG_B(cpu)     (CPU_P(cpu) &= ~FLAG_B)
#define CLR_CPU_FLAG_V(cpu)     (CPU_P(cpu) &= ~FLAG_V)
#define CLR_CPU_FLAG_N(cpu)     (CPU_P(cpu) &= ~FLAG_N)

// Conditional flag setting
#define SET_CPU_FLAG_C_COND(cpu, cond) do { \
    if (cond) SET_CPU_FLAG_C(cpu); else CLR_CPU_FLAG_C(cpu); \
} while(0)

#define SET_CPU_FLAG_Z_COND(cpu, cond) do { \
    if (cond) SET_CPU_FLAG_Z(cpu); else CLR_CPU_FLAG_Z(cpu); \
} while(0)

#define SET_CPU_FLAG_V_COND(cpu, cond) do { \
    if (cond) SET_CPU_FLAG_V(cpu); else CLR_CPU_FLAG_V(cpu); \
} while(0)

#define SET_CPU_FLAG_N_COND(cpu, cond) do { \
    if (cond) SET_CPU_FLAG_N(cpu); else CLR_CPU_FLAG_N(cpu); \
} while(0)

// ===== GENERIC REGISTER ACCESS =====

// Direct register array access with validation
// MSVC doesn't support GNU statement expressions; use inline helpers instead
static inline uint8_t CPU_REG_fn(mos6510_state_t *cpu, uint8_t reg) {
    VALIDATE_REG(reg);
    return cpu->registers[reg];
}

static inline void SET_CPU_REG_fn(mos6510_state_t *cpu, uint8_t reg, uint8_t val) {
    VALIDATE_REG(reg);
    cpu->registers[reg] = val;
}

#define CPU_REG(cpu, reg)               CPU_REG_fn((cpu), (uint8_t)(reg))
#define SET_CPU_REG(cpu, reg, val)      SET_CPU_REG_fn((cpu), (uint8_t)(reg), (uint8_t)(val))

// ===== HARDWARE-ACCURATE FLAG OPERATIONS =====

// Set N and Z flags based on value (common operation)
static inline void set_cpu_nz_flags(mos6510_state_t *cpu, uint8_t value) {
    SET_CPU_FLAG_N_COND(cpu, value & 0x80);
    SET_CPU_FLAG_Z_COND(cpu, value == 0);
}

// Set N, Z, C flags for comparison operations
static inline void set_cpu_nzc_flags_cmp(mos6510_state_t *cpu, uint16_t result) {
    SET_CPU_FLAG_N_COND(cpu, result & 0x80);
    SET_CPU_FLAG_Z_COND(cpu, (result & 0xFF) == 0);
    // For compare operations: C=1 if no borrow (reg >= operand), C=0 if borrow (reg < operand)
    SET_CPU_FLAG_C_COND(cpu, (result & 0x8000) == 0);  // No borrow if bit 15 is 0
}

// Set all arithmetic flags (N, V, Z, C) for ADC/SBC operations
static inline void set_cpu_nvzc_flags_add(mos6510_state_t *cpu, uint8_t a, uint8_t b, uint16_t result) {
    SET_CPU_FLAG_N_COND(cpu, result & 0x80);
    SET_CPU_FLAG_Z_COND(cpu, (result & 0xFF) == 0);
    SET_CPU_FLAG_C_COND(cpu, result > 0xFF);
    // Overflow: sign of inputs same, sign of result different
    SET_CPU_FLAG_V_COND(cpu, ((a ^ result) & (b ^ result) & 0x80) != 0);
}

// ===== REGISTER DEBUGGING AND INSPECTION =====

// Get register name for debugging
static inline const char* get_register_name(uint8_t reg) {
    if (!IS_VALID_REG(reg)) return "INVALID";
    return register_properties[reg].name;
}

// Check if register is architectural (user-visible)
static inline bool is_architectural_register(uint8_t reg) {
    return IS_VALID_REG(reg) && IS_ARCHITECTURAL_REG(reg);
}

// Check if register is internal bus related
static inline bool is_internal_bus_register(uint8_t reg) {
    return IS_VALID_REG(reg) && IS_INTERNAL_BUS_REG(reg);
}

#endif // MOS6510_CYCLE_REGISTER_ACCESS_H