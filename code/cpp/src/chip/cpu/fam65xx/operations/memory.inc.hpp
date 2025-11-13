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
        update_nz_flags(this->get(REG_A));
        
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
        update_nz_flags(this->get(REG_X));
        
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
        update_nz_flags(this->get(REG_Y));
        
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
        pins = this->phi2_write(pins, REG_AB, this->get(REG_A));
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
        pins = this->phi2_write(pins, REG_AB, this->get(REG_X));
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
        pins = this->phi2_write(pins, REG_AB, this->get(REG_Y));
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
    // Check for 65816 native mode with 16-bit accumulator (M=0) - nested native code
    if constexpr (has_wide_registers()) {
        if (!this->get_emulation_mode() && !(this->get(REG_P) & FLAG_M)) {
            // 65816 native mode, 16-bit accumulator - read 2 bytes
            switch (this->cycle_index) {
                case 0:
                    // Read low byte of operand
                    pins = phi2_read_operand(pins, REG_DL);
                    if (FAM65XX_GET_RDY(pins)) {
                        this->cycle_index++;
                        // For 65816 native mode, increment address bus with bank handling
                        this->inc(REG_AB);
                    }
                    return pins;
                    
                case 1:
                    // Read high byte of operand (this provides N and V flags)
                    pins = phi2_read(pins, REG_AB, REG_DL);
                    if (FAM65XX_GET_RDY(pins)) {
                        // Perform 16-bit BIT operation
                        uint16_t operand = (this->get(REG_ABH) << 8) | this->get(REG_DL);
                        uint16_t acc = this->get(REG_A_FULL);
                        uint16_t result = acc & operand;
                        
                        // BIT 16-bit: N and V flags come from HIGH BYTE of operand
                        uint8_t high_byte = this->get(REG_ABH);
                        update_flag(FLAG_Z, result == 0);          // Z = 1 if (A & operand) == 0
                        update_flag(FLAG_V, (high_byte & 0x40) != 0); // V = bit 6 of HIGH byte
                        update_flag(FLAG_N, (high_byte & 0x80) != 0); // N = bit 7 of HIGH byte
                        
                        transition_to_fetch();
                    }
                    return pins;
            }
            return pins;
        }
    }
    
    // Standard 8-bit BIT operation (emulation mode and non-wide CPUs)
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = this->get(REG_DL);
        uint8_t result = this->get(REG_A) & operand;
        
        // BIT immediate (65C02) only affects Z flag - N and V are NOT affected
        // BIT memory affects N, V, and Z flags normally
        if (this->opcode_entry.am_index == to_index(AM::IMM)) {
            // BIT immediate: only update Z flag
            update_flag(FLAG_Z, result == 0);
        } else {
            // BIT memory: update N, V, and Z flags
            // N = bit 7 of operand (copy bit 7 directly)
            // V = bit 6 of operand (copy bit 6 directly)
            // Z = result of A & operand (set if result is zero)
            
            // Extract N and V flags from operand in one operation (more efficient)
            uint8_t flags_from_operand = operand & (FLAG_N | FLAG_V);
            
            update_flags(FLAG_N | FLAG_V | FLAG_Z, flags_from_operand | calc_z_flag(result));
        }

        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
