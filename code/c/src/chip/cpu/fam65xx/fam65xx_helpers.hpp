#pragma once
/*
 * fam65xx_helpers.hpp - Unified Helper Functions for MOS 65xx Family CPU Operations
 * 
 * This file contains all the unified helper functions that are shared across 
 * the split operation files to eliminate code duplication.
 */

#include "fam65xx_types.hpp"
#include "fam65xx_utils.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef AIEMUC_IMPL

/* UNIFIED: Read operand from immediate or memory mode */
static inline bus_state_t read_operand_immediate_or_memory(fam65xx_t* cpu, bus_state_t pins) {
    if (cpu->opcode_entry.am_index == AM_IMM) {
        /* Immediate mode - read from PC */
        pins = fam65xx_phi2_read(cpu, pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        CPU_PC(cpu)++;
    } else {
        /* Memory mode - read from target address */
        pins = fam65xx_phi2_read(cpu, pins, REG_AB);
        if (!FAM65XX_GET_RDY(pins)) return pins;
    }
    return pins;
}

/* UNIFIED: Simple register operation with dummy cycle */
static inline bus_state_t simple_register_op_with_dummy_cycle(fam65xx_t* cpu, bus_state_t pins) {
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    return pins;
}

/* UNIFIED: Compare operations helper */
static inline bus_state_t compare_helper(fam65xx_t* cpu, bus_state_t pins, uint8_t reg_value) {
    /* PHI2: Read operand using unified pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;

    /* PHI1: Perform compare operation */
    uint8_t data = BUS_GET_DATA(pins);
    uint16_t result = reg_value - data;

    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                (result & FLAG_N) |
                ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                (reg_value >= data ? FLAG_C : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* UNIFIED: Flag operation with dummy cycle */
static inline bus_state_t flag_operation_with_dummy_cycle(fam65xx_t* cpu, bus_state_t pins, uint8_t flag, bool set_flag) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;

    /* PHI1: Set or clear the flag */
    if (set_flag) {
        CPU_P(cpu) |= flag;
    } else {
        CPU_P(cpu) &= ~flag;
    }
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* UNIFIED: BCD Addition Helper - Hardware-accurate 6502 BCD algorithm
 *
 * Implements the exact BCD addition algorithm used by the MOS 6502 with proper flag handling.
 * This unified function ensures consistent BCD behavior across ADC, RRA, ISC, and ARR operations.
 *
 * Parameters:
 *   a_old: Original A register value
 *   operand: Value to add
 *   carry_in: Carry input (0 or 1)
 *   result_out: Pointer to store BCD result
 *   flags_out: Pointer to store flag bits (N|V|Z|C)
 *
 * Returns: void (results stored in output parameters)
 */
static inline void bcd_addition_helper(uint8_t a_old, uint8_t operand, uint8_t carry_in,
                                       uint8_t* result_out, uint8_t* flags_out) {
    /* Hardware-accurate 6502 BCD addition (credit: MAME/floooh implementation) */
    uint8_t al = (a_old & 0x0F) + (operand & 0x0F) + carry_in;
    if (al > 9) {
        al += 6;
    }
    uint8_t ah = (a_old >> 4) + (operand >> 4) + (al > 0x0F);
    
    /* Clear all flags we're about to set */
    *flags_out = 0;
    
    /* Z flag: binary sum is zero */
    if (0 == (uint8_t)(a_old + operand + carry_in)) {
        *flags_out |= FLAG_Z;
    }
    /* N flag: bit 3 of high nibble (ah & 0x08) - FIXED: separate from Z flag check */
    if (ah & 0x08) {
        *flags_out |= FLAG_N;
    }
    
    /* V flag: signed overflow using intermediate result (ah<<4) */
    if (~(a_old ^ operand) & (a_old ^ (ah << 4)) & 0x80) {
        *flags_out |= FLAG_V;
    }
    
    /* High nibble adjustment and carry */
    if (ah > 9) {
        ah += 6;
    }
    if (ah > 15) {
        *flags_out |= FLAG_C;
    }
    
    /* Final BCD result */
    *result_out = (ah << 4) | (al & 0x0F);
}

/* UNIFIED: BCD Subtraction Helper - Hardware-accurate 6502 BCD algorithm
 *
 * Implements the exact BCD subtraction algorithm used by the MOS 6502 with proper flag handling.
 * This unified function ensures consistent BCD behavior across SBC and related operations.
 *
 * Parameters:
 *   a_old: Original A register value
 *   operand: Value to subtract
 *   borrow_in: Borrow input (0 or 1, where 1 means borrow)
 *   result_out: Pointer to store BCD result
 *   flags_out: Pointer to store flag bits (N|V|Z|C)
 *
 * Returns: void (results stored in output parameters)
 */
static inline void bcd_subtraction_helper(uint8_t a_old, uint8_t operand, uint8_t borrow_in,
                                          uint8_t* result_out, uint8_t* flags_out) {
    /* Hardware-accurate 6502 BCD subtraction (credit: MAME/floooh implementation) */
    uint16_t diff = a_old - operand - borrow_in;
    uint8_t al = (a_old & 0x0F) - (operand & 0x0F) - borrow_in;
    if ((int8_t)al < 0) {
        al -= 6;
    }
    uint8_t ah = (a_old >> 4) - (operand >> 4) - ((int8_t)al < 0);
    
    /* Clear all flags we're about to set */
    *flags_out = 0;
    
    /* Z flag: binary difference is zero */
    if (0 == (uint8_t)diff) {
        *flags_out |= FLAG_Z;
    }
    /* N flag: bit 7 of binary difference */
    else if (diff & 0x80) {
        *flags_out |= FLAG_N;
    }
    
    /* V flag: signed overflow on binary operation */
    if ((a_old ^ operand) & (a_old ^ diff) & 0x80) {
        *flags_out |= FLAG_V;
    }
    
    /* C flag: no borrow (result >= 0) */
    if (!(diff & 0xFF00)) {
        *flags_out |= FLAG_C;
    }
    
    /* High nibble adjustment */
    if (ah & 0x80) {
        ah -= 6;
    }
    
    /* Final BCD result */
    *result_out = (ah << 4) | (al & 0x0F);
}

#endif /* AIEMUC_IMPL */

#ifdef __cplusplus
}
#endif