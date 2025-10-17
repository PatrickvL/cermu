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

#endif /* AIEMUC_IMPL */

#ifdef __cplusplus
}
#endif