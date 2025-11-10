/*
 * memory.inc.hpp - Memory Load/Store Operations for MOS 65xx Family
 *
 * This file contains load and store operation implementations that are
 * included within the fam65xx_t template class. These operations handle
 * data movement between registers and memory.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// LOAD ACCUMULATOR (LDA)
// ============================================================================

bus_state_t op_lda(bus_state_t pins) {
    // Read operand directly into accumulator (eliminates DL copy)
    pins = phi2_read_operand(pins, REG_A);
    if (FAM65XX_GET_RDY(pins)) {
        // Update N and Z flags
        update_nz_flags(get(REG_A));
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// LOAD X REGISTER (LDX)
// ============================================================================

bus_state_t op_ldx(bus_state_t pins) {
    // Read operand directly into X register (eliminates DL copy)
    pins = phi2_read_operand(pins, REG_X);
    if (FAM65XX_GET_RDY(pins)) {
        // Update N and Z flags
        update_nz_flags(get(REG_X));
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// LOAD Y REGISTER (LDY)
// ============================================================================

bus_state_t op_ldy(bus_state_t pins) {
    // Read operand directly into Y register (eliminates DL copy)
    pins = phi2_read_operand(pins, REG_Y);
    if (FAM65XX_GET_RDY(pins)) {
        // Update N and Z flags
        update_nz_flags(get(REG_Y));
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// STORE ACCUMULATOR (STA)
// ============================================================================

bus_state_t op_sta(bus_state_t pins) {
    // Store accumulator with processor-specific RDY handling - optimized direct value write
    if (this->should_complete_write_cycle(pins)) {
        pins = this->phi2_write(pins, REG_AB, get(REG_A));
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// STORE X REGISTER (STX)
// ============================================================================

bus_state_t op_stx(bus_state_t pins) {
    // Store X register with processor-specific RDY handling - optimized direct value write
    if (this->should_complete_write_cycle(pins)) {
        pins = this->phi2_write(pins, REG_AB, get(REG_X));
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// STORE Y REGISTER (STY)
// ============================================================================

bus_state_t op_sty(bus_state_t pins) {
    // Store Y register with processor-specific RDY handling - optimized direct value write
    if (this->should_complete_write_cycle(pins)) {
        pins = this->phi2_write(pins, REG_AB, get(REG_Y));
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// LOGIC OPERATIONS
// ============================================================================

// AND with Accumulator
bus_state_t op_and(bus_state_t pins) {
    // Read operand directly into DL register (optimized version)
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        // Perform AND operation
        this->set(REG_A, this->get(REG_A) & this->get(REG_DL));
        
        // Update N and Z flags
        update_nz_flags(this->get(REG_A));
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// OR with Accumulator
bus_state_t op_ora(bus_state_t pins) {
    // Read operand directly into DL register (optimized version)
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        // Perform OR operation
        this->set(REG_A, this->get(REG_A) | this->get(REG_DL));
        
        // Update N and Z flags
        update_nz_flags(this->get(REG_A));
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// Exclusive OR with Accumulator
bus_state_t op_eor(bus_state_t pins) {
    // Read operand directly into DL register (optimized version)
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        // Perform EOR operation
        this->set(REG_A, this->get(REG_A) ^ this->get(REG_DL));
        
        // Update N and Z flags
        update_nz_flags(this->get(REG_A));
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// BIT TEST OPERATION
// ============================================================================

bus_state_t op_bit(bus_state_t pins) {
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = this->get(REG_DL);
        uint8_t result = get(REG_A) & operand;
        
        // BIT immediate (65C02) only affects Z flag - N and V are NOT affected
        // BIT memory affects N, V, and Z flags normally
        if (this->opcode_entry.am_index == to_index(AM::IMM)) {
            // BIT immediate: only update Z flag
            update_flag(FLAG_Z, result == 0);
        } else {
            // BIT memory: update N, V, and Z flags
            // N = bit 7 of operand
            // V = bit 6 of operand
            // Z = result of A & operand
            update_flags(FLAG_N | FLAG_V | FLAG_Z,
                        (operand & (FLAG_N | FLAG_V)) |
                        calc_z_flag(result));
        }

        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"