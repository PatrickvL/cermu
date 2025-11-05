/*
 * flags.inc.hpp - Flag Manipulation Operations for MOS 65xx Family
 *
 * This file contains flag manipulation operation implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// FLAG MANIPULATION OPERATIONS
// ============================================================================

/* CLC - Clear Carry Flag */
bus_state_t op_clc(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    clear_flag(FLAG_C);
    transition_to_fetch();
    return pins;
}

/* SEC - Set Carry Flag */
bus_state_t op_sec(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    set_flag(FLAG_C);
    transition_to_fetch();
    return pins;
}

/* CLI - Clear Interrupt Disable Flag */
bus_state_t op_cli(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    clear_flag(FLAG_I);
    transition_to_fetch();
    return pins;
}

/* SEI - Set Interrupt Disable Flag */
bus_state_t op_sei(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    set_flag(FLAG_I);
    transition_to_fetch();
    return pins;
}

/* CLD - Clear Decimal Mode Flag */
bus_state_t op_cld(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    clear_flag(FLAG_D);
    transition_to_fetch();
    return pins;
}

/* SED - Set Decimal Mode Flag */
bus_state_t op_sed(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    set_flag(FLAG_D);
    transition_to_fetch();
    return pins;
}

/* CLV - Clear Overflow Flag */
bus_state_t op_clv(bus_state_t pins) {
    if constexpr (!has_optimized_cycles()) {
        /* Dummy cycle for internal operation */
        pins = phi2_dummy_read(pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) {
            return pins;
        }
    }
    
    // Common operation for all processors
    clear_flag(FLAG_V);
    transition_to_fetch();
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
