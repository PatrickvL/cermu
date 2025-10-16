#pragma once
/*
 * fam65xx_utils.hpp - MOS 65xx Family CPU Utility Functions (C++ Version)
 * 
 * This file contains utility functions and helper implementations used
 * across the 65xx family CPU implementations:
 * - Page cross detection
 * - Flag update functions
 * - PHI2 handlers for read/write operations
 * - Transition helper functions
 */

#include "fam65xx_types.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Utility Functions
// ============================================================================

/* Fast page cross detection using XOR and bit 8 check */
static inline bool fam65xx_page_crossed(uint16_t addr1, uint16_t addr2) {
    return (addr1 ^ addr2) & 0x0100;
}

/* Update N and Z flags based on value */
static inline void fam65xx_update_nz_flags(fam65xx_t* cpu, uint8_t value) {
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z)) |
                 (value & FLAG_N) |
                 (value == 0 ? FLAG_Z : 0);
}

// ============================================================================
// HELPER FUNCTIONS (formerly in fam65xx_core.hpp)
// ============================================================================

#ifdef AIEMUC_IMPL

// Centralized PHI2 read handler - handles memory reads during PHI2 phase
static bus_state_t fam65xx_phi2_read(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg) {
    uint16_t address;

    if (FAM65XX_GET_RDY(pins)) {
        address = cpu->reg16[addr_reg];
        pins = BUS_SET_ADDR(pins, address);
    } else {
        address = BUS_GET_ADDR(pins);
    }

    /* Set R/W̅ bit to indicate READ (1 = Read, 0 = Write) */
    pins |= FAM65XX_RW;

    /* Always perform memory read to service VIC-II even when CPU halted */
    uint8_t current_bus_data = BUS_GET_DATA(pins);
    uint8_t data = cpu->mem_read(cpu->mem_user_data, address, current_bus_data);
    pins = BUS_SET_DATA(pins, data);
    
    return pins;
}

// Centralized PHI2 write handler - handles memory writes during PHI2 phase
static bus_state_t fam65xx_phi2_write(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg, reg8_t data_reg) {
    uint16_t address = cpu->reg16[addr_reg];
    
    /* Write cycle - always proceeds regardless of RDY */
    pins = BUS_SET_ADDR(pins, address);
    uint8_t data_byte = cpu->reg8[data_reg];
    pins = BUS_SET_DATA(pins, data_byte);
    
    /* CRITICAL FIX: Clear R/W̅ bit to indicate WRITE (0 = Write, 1 = Read) */
    pins &= ~FAM65XX_RW;
    
    cpu->mem_write(cpu->mem_user_data, address, data_byte);
    
    return pins;
}

/* ============================================================================
 * TRANSITION FUNCTIONS
 * ============================================================================
 */

/* Transition from addressing mode to operation */
static inline void fam65xx_transition_to_operation(fam65xx_t* cpu) {
    /* Use the processor-specific internal table - forward declaration */
    extern const cycle_fn_t fam65xx_op_handlers[];
    cpu->cycle_index = 0;
    cpu->current_handler = fam65xx_op_handlers[cpu->opcode_entry.op_index];
}

// Transition from operation back to opcode fetch
static inline void fam65xx_transition_to_fetch(fam65xx_t* cpu) {
    cpu->cycle_index = 0;
    // Forward declaration - will be resolved at link time
    extern bus_state_t fam65xx_opcode_fetch(fam65xx_t* cpu, bus_state_t pins);
    cpu->current_handler = fam65xx_opcode_fetch;
}

#endif // AIEMUC_IMPL

#ifdef __cplusplus
}
#endif