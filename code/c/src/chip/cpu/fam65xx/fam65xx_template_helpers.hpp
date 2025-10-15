#pragma once
#ifndef FAM65XX_TEMPLATE_HELPERS_HPP_INCLUDED
#define FAM65XX_TEMPLATE_HELPERS_HPP_INCLUDED

/*
 * fam65xx_template_helpers.hpp - Helper functions for template-based operations
 *
 * This file provides the function declarations and helper utilities needed
 * by template-based processor-specific operations.
 */

#include "fam65xx_core.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// HELPER FUNCTION DECLARATIONS - HEADER ONLY
// ============================================================================

#ifdef CHIPS_IMPL

// Centralized PHI2 read handler - handles memory reads during PHI2 phase
static inline bus_state_t fam65xx_template_phi2_read(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg) {
    uint16_t address;

    if (FAM65XX_GET_RDY(pins)) {
        address = cpu->reg16[addr_reg];
        BUS_SET_ADDR(pins, address);
    } else {
        address = BUS_GET_ADDR(pins);
    }

    /* Always perform memory read to service VIC-II even when CPU halted */
    uint8_t current_bus_data = BUS_GET_DATA(pins);
    uint8_t data = cpu->mem_read(cpu->mem_user_data, address, current_bus_data);
    BUS_SET_DATA(pins, data);
    
    return pins;
}

// Centralized PHI2 write handler - handles memory writes during PHI2 phase
static inline bus_state_t fam65xx_template_phi2_write(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg, reg8_t data_reg) {
    uint16_t address = cpu->reg16[addr_reg];
    
    /* Write cycle - always proceeds regardless of RDY */
    BUS_SET_ADDR(pins, address);
    uint8_t data_byte = cpu->reg8[data_reg];
    BUS_SET_DATA(pins, data_byte);
    cpu->mem_write(cpu->mem_user_data, address, data_byte);
    
    return pins;
}

// Transition from operation back to opcode fetch
static inline void fam65xx_template_transition_to_fetch(fam65xx_t* cpu) {
    cpu->cycle_index = 0;
    // Forward declaration - will be resolved at link time
    extern bus_state_t fam65xx_opcode_fetch(fam65xx_t* cpu, bus_state_t pins);
    cpu->current_handler = fam65xx_opcode_fetch;
}

// Update N and Z flags based on value
static inline void fam65xx_template_update_nz_flags(fam65xx_t* cpu, uint8_t value) {
    uint8_t flags = CPU_P(cpu) & ~(FLAG_N | FLAG_Z);
    if (value & 0x80) flags |= FLAG_N;
    if (value == 0) flags |= FLAG_Z;
    CPU_P(cpu) = flags;
}

#endif // CHIPS_IMPL

#ifdef __cplusplus
}
#endif

#endif // FAM65XX_TEMPLATE_HELPERS_HPP_INCLUDED