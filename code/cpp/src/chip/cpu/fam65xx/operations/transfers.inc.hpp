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
bus_state_t transfer_with_flags_helper(bus_state_t pins, uint8_t value, reg8_t target_reg) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = this->phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    this->set(target_reg, value);
    update_nz_flags(value);
    transition_to_fetch();
    return pins;
}

/* Helper for transfer operations without flags */
bus_state_t transfer_no_flags_helper(bus_state_t pins, uint8_t value, reg8_t target_reg) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = this->phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    this->set(target_reg, value);
    transition_to_fetch();
    return pins;
}

// ============================================================================
// REGISTER TRANSFER OPERATIONS
// ============================================================================

/* TAX - Transfer A to X */
bus_state_t op_tax(bus_state_t pins) {
    return transfer_with_flags_helper(pins, this->get(REG_A), REG_X);
}

/* TAY - Transfer A to Y */
bus_state_t op_tay(bus_state_t pins) {
    return transfer_with_flags_helper(pins, this->get(REG_A), REG_Y);
}

/* TSX - Transfer S to X */
bus_state_t op_tsx(bus_state_t pins) {
    return transfer_with_flags_helper(pins, this->get(REG_S), REG_X);
}

/* TXA - Transfer X to A */
bus_state_t op_txa(bus_state_t pins) {
    return transfer_with_flags_helper(pins, this->get(REG_X), REG_A);
}

/* TXS - Transfer X to S */
bus_state_t op_txs(bus_state_t pins) {
    return transfer_no_flags_helper(pins, this->get(REG_X), REG_S);
}

/* TYA - Transfer Y to A */
bus_state_t op_tya(bus_state_t pins) {
    return transfer_with_flags_helper(pins, this->get(REG_Y), REG_A);
}

// ============================================================================
// REGISTER INCREMENT/DECREMENT OPERATIONS  
// ============================================================================

/* INX - Increment X */
bus_state_t op_inx(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = this->phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    this->inc(REG_X);
    update_nz_flags(this->get(REG_X));
    transition_to_fetch();
    return pins;
}

/* INY - Increment Y */
bus_state_t op_iny(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = this->phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    this->inc(REG_Y);
    update_nz_flags(this->get(REG_Y));
    transition_to_fetch();
    return pins;
}

/* DEX - Decrement X */
bus_state_t op_dex(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = this->phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    this->dec(REG_X);
    update_nz_flags(this->get(REG_X));
    transition_to_fetch();
    return pins;
}

/* DEY - Decrement Y */
bus_state_t op_dey(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = this->phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    this->dec(REG_Y);
    update_nz_flags(this->get(REG_Y));
    transition_to_fetch();
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
