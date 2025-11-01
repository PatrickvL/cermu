/*
 * transfers.inc.hpp - Register Transfer Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// TRANSFER HELPER FUNCTIONS
// ============================================================================

/* Helper for transfer operations with flags */
bus_state_t transfer_with_flags_helper(bus_state_t pins, uint8_t value, uint8_t& target_reg) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    target_reg = value;
    update_nz_flags(target_reg);
    transition_to_fetch();
    return pins;
}

/* Helper for transfer operations without flags */
bus_state_t transfer_no_flags_helper(bus_state_t pins, uint8_t value, uint8_t& target_reg) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    target_reg = value;
    transition_to_fetch();
    return pins;
}

// ============================================================================
// REGISTER TRANSFER OPERATIONS
// ============================================================================

/* TAX - Transfer A to X */
bus_state_t op_tax(bus_state_t pins) {
    return transfer_with_flags_helper(pins, CPU_A(this), CPU_X(this));
}

/* TAY - Transfer A to Y */
bus_state_t op_tay(bus_state_t pins) {
    return transfer_with_flags_helper(pins, CPU_A(this), CPU_Y(this));
}

/* TSX - Transfer S to X */
bus_state_t op_tsx(bus_state_t pins) {
    return transfer_with_flags_helper(pins, CPU_S(this), CPU_X(this));
}

/* TXA - Transfer X to A */
bus_state_t op_txa(bus_state_t pins) {
    return transfer_with_flags_helper(pins, CPU_X(this), CPU_A(this));
}

/* TXS - Transfer X to S */
bus_state_t op_txs(bus_state_t pins) {
    return transfer_no_flags_helper(pins, CPU_X(this), CPU_S(this));
}

/* TYA - Transfer Y to A */
bus_state_t op_tya(bus_state_t pins) {
    return transfer_with_flags_helper(pins, CPU_Y(this), CPU_A(this));
}

// ============================================================================
// REGISTER INCREMENT/DECREMENT OPERATIONS  
// ============================================================================

/* INX - Increment X */
bus_state_t op_inx(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    CPU_X(this)++;
    update_nz_flags(CPU_X(this));
    transition_to_fetch();
    return pins;
}

/* INY - Increment Y */
bus_state_t op_iny(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    CPU_Y(this)++;
    update_nz_flags(CPU_Y(this));
    transition_to_fetch();
    return pins;
}

/* DEX - Decrement X */
bus_state_t op_dex(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    CPU_X(this)--;
    update_nz_flags(CPU_X(this));
    transition_to_fetch();
    return pins;
}

/* DEY - Decrement Y */
bus_state_t op_dey(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    CPU_Y(this)--;
    update_nz_flags(CPU_Y(this));
    transition_to_fetch();
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"