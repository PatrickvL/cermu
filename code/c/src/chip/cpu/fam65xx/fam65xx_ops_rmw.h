#pragma once
/*
 * fam65xx_rmw_operations.h - MOS 65xx Family Read-Modify-Write Operations
 * 
 * This file contains all Read-Modify-Write (RMW) operations that can work
 * in both accumulator mode (1 cycle) and memory mode (3 cycles).
 * 
 * RMW operations include:
 * - ASL (Arithmetic Shift Left)
 * - DEC (Decrement Memory)
 * - INC (Increment Memory) 
 * - LSR (Logical Shift Right)
 * - ROL (Rotate Left)
 * - ROR (Rotate Right)
 * 
 * Plus illegal RMW combination operations:
 * - DCP (DEC + CMP)
 * - ISC (INC + SBC)
 * - RLA (ROL + AND)
 * - RRA (ROR + ADC)
 * - SLO (ASL + ORA)
 * - SRE (LSR + EOR)
 */

#include "fam65xx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

/* ============================================================================
 * RMW MACROS
 * ============================================================================
 * These macros handle the boilerplate for Read-Modify-Write operations.
 * They work with both accumulator mode (1 cycle) and memory mode (3 cycles).
 */

/* RMW handler for operations that support both accumulator and memory modes */
#define RMW_HANDLER_START(cpu, pins, reg_idx_var) \
    uint8_t reg_idx_var; \
    if ((cpu)->opcode_entry.rmw) { \
        reg_idx_var = REG_DL; \
        switch ((cpu)->cycle_index++) { \
            case 0: \
                pins = fam65xx_phi2_read(cpu, pins, REG_AB); \
                if (!FAM65XX_GET_RDY(pins)) return pins; \
                CPU_DL(cpu) = BUS_GET_DATA(pins); \
                return pins; \
            case 1: \
                pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL); \
                if (!FAM65XX_GET_RDY(pins)) return pins; \
                /* Operation logic goes here in cycle 1 - after dummy write, before final write */ \
                break; \
            case 2: \
                pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL); \
                if (!FAM65XX_GET_RDY(pins)) return pins; \
                fam65xx_transition_to_fetch(cpu); \
                return pins; \
        } \
    } else { \
        /* Accumulator mode - single cycle with dummy PHI2 read */ \
        reg_idx_var = REG_A; \
        pins = fam65xx_phi2_read(cpu, pins, REG_PC); \
        if (!FAM65XX_GET_RDY(pins)) return pins; \
    }

#define RMW_HANDLER_END(cpu) \
    if (!(cpu)->opcode_entry.rmw) { \
        fam65xx_transition_to_fetch(cpu); \
    }

/* RMW handler for memory-only operations (no accumulator mode) */
#define RMW_ONLY_START(cpu, pins, reg_idx_var) \
    uint8_t reg_idx_var = REG_DL; \
    switch ((cpu)->cycle_index++) { \
        case 0: \
            pins = fam65xx_phi2_read(cpu, pins, REG_AB); \
            if (!FAM65XX_GET_RDY(pins)) return pins; \
            CPU_DL(cpu) = BUS_GET_DATA(pins); \
            return pins; \
        case 1: \
            pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL); \
            if (!FAM65XX_GET_RDY(pins)) return pins; \
            /* Operation logic goes here in cycle 1 - after dummy write, before final write */ \
            break; \
        case 2: \
            pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL); \
            if (!FAM65XX_GET_RDY(pins)) return pins; \
            fam65xx_transition_to_fetch(cpu); \
            return pins; \
    }

/* ============================================================================
 * OFFICIAL RMW OPERATIONS
 * ============================================================================
 */

/* ASL - Arithmetic Shift Left */
static bus_state_t op_asl(fam65xx_t* cpu, bus_state_t pins) {
    RMW_HANDLER_START(cpu, pins, reg_idx);
    
    /* Perform ASL operation */
    uint8_t value = cpu->reg8[reg_idx];
    if (value & 0x80) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value <<= 1;
    cpu->reg8[reg_idx] = value;
    fam65xx_update_nz_flags(cpu, value);
    
    RMW_HANDLER_END(cpu);
    return pins;
}

/* DEC - Decrement Memory (RMW only, no accumulator mode) */
static bus_state_t op_dec(fam65xx_t* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform DEC operation */
    cpu->reg8[reg_idx]--;
    fam65xx_update_nz_flags(cpu, cpu->reg8[reg_idx]);
    return pins;
}

/* INC - Increment Memory (RMW only, no accumulator mode) */
static bus_state_t op_inc(fam65xx_t* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform INC operation */
    cpu->reg8[reg_idx]++;
    fam65xx_update_nz_flags(cpu, cpu->reg8[reg_idx]);
    return pins;
}

/* LSR - Logical Shift Right */
static bus_state_t op_lsr(fam65xx_t* cpu, bus_state_t pins) {
    RMW_HANDLER_START(cpu, pins, reg_idx);
    
    /* Perform LSR operation */
    uint8_t value = cpu->reg8[reg_idx];
    if (value & 0x01) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value >>= 1;
    cpu->reg8[reg_idx] = value;
    fam65xx_update_nz_flags(cpu, value);
    
    RMW_HANDLER_END(cpu);
    return pins;
}

/* ROL - Rotate Left */
static bus_state_t op_rol(fam65xx_t* cpu, bus_state_t pins) {
    RMW_HANDLER_START(cpu, pins, reg_idx);
    
    /* Perform ROL operation */
    uint8_t value = cpu->reg8[reg_idx];
    uint8_t old_carry = (CPU_P(cpu) & FLAG_C) ? 1 : 0;
    if (value & 0x80) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value = (value << 1) | old_carry;
    cpu->reg8[reg_idx] = value;
    fam65xx_update_nz_flags(cpu, value);
    
    RMW_HANDLER_END(cpu);
    return pins;
}

/* ROR - Rotate Right */
static bus_state_t op_ror(fam65xx_t* cpu, bus_state_t pins) {
    RMW_HANDLER_START(cpu, pins, reg_idx);
    
    /* Perform ROR operation */
    uint8_t value = cpu->reg8[reg_idx];
    uint8_t old_carry = (CPU_P(cpu) & FLAG_C) ? 0x80 : 0;
    if (value & 0x01) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value = (value >> 1) | old_carry;
    cpu->reg8[reg_idx] = value;
    fam65xx_update_nz_flags(cpu, value);
    
    RMW_HANDLER_END(cpu);
    return pins;
}

/* ============================================================================
 * ILLEGAL RMW OPERATIONS
 * ============================================================================
 * These are combination operations that perform two operations in sequence.
 * All are memory-only RMW operations (no accumulator mode).
 */

/* DCP - DEC + CMP (Decrement and Compare) */
static bus_state_t op_dcp(fam65xx_t* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform DEC */
    cpu->reg8[reg_idx]--;
    
    /* Perform CMP with A */
    uint8_t value = cpu->reg8[reg_idx];
    uint16_t result = CPU_A(cpu) - value;
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_A(cpu) >= value ? FLAG_C : 0);
    return pins;
}

/* ISC - INC + SBC (Increment and Subtract with Carry) */
static bus_state_t op_isc(fam65xx_t* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform INC */
    cpu->reg8[reg_idx]++;
    
    /* Perform SBC with A */
    uint8_t operand = cpu->reg8[reg_idx];
    uint16_t result = CPU_A(cpu) - operand - (CPU_P(cpu) & FLAG_C ? 0 : 1);
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    if (result < 0x100) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    if (((a_old ^ operand) & (a_old ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    return pins;
}

/* RLA - ROL + AND (Rotate Left and AND) */
static bus_state_t op_rla(fam65xx_t* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform ROL */
    uint8_t value = cpu->reg8[reg_idx];
    uint8_t old_carry = (CPU_P(cpu) & FLAG_C) ? 1 : 0;
    if (value & 0x80) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value = (value << 1) | old_carry;
    cpu->reg8[reg_idx] = value;
    
    /* Perform AND with A */
    CPU_A(cpu) &= value;
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    return pins;
}

/* RRA - ROR + ADC (Rotate Right and Add with Carry) */
static bus_state_t op_rra(fam65xx_t* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform ROR */
    uint8_t value = cpu->reg8[reg_idx];
    uint8_t old_carry = (CPU_P(cpu) & FLAG_C) ? 0x80 : 0;
    if (value & 0x01) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value = (value >> 1) | old_carry;
    cpu->reg8[reg_idx] = value;
    
    /* Perform ADC with A */
    uint8_t operand = value;
    uint16_t result = CPU_A(cpu) + operand + (CPU_P(cpu) & FLAG_C ? 1 : 0);
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    if (result > 0xFF) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    if (((a_old ^ result) & (operand ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    return pins;
}

/* SLO - ASL + ORA (Shift Left and OR) */
static bus_state_t op_slo(fam65xx_t* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform ASL */
    uint8_t value = cpu->reg8[reg_idx];
    if (value & 0x80) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value <<= 1;
    cpu->reg8[reg_idx] = value;
    
    /* Perform ORA with A */
    CPU_A(cpu) |= value;
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    return pins;
}

/* SRE - LSR + EOR (Shift Right and EOR) */
static bus_state_t op_sre(fam65xx_t* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform LSR */
    uint8_t value = cpu->reg8[reg_idx];
    if (value & 0x01) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value >>= 1;
    cpu->reg8[reg_idx] = value;
    
    /* Perform EOR with A */
    CPU_A(cpu) ^= value;
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    return pins;
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif