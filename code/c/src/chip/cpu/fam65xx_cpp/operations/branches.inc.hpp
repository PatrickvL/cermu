/*
 * branches.inc - Branch Operations for MOS 65xx Family
 *
 * This file contains branch operation implementations that are included
 * within the fam65xx_t template class.
 */

// ============================================================================
// BRANCH HELPER FUNCTION
// ============================================================================

/* Helper function for branch operations - hardware-accurate 6502 timing */
bus_state_t branch_helper(bus_state_t pins, uint8_t flag_mask, bool flag_value) {
    switch (this->cycle_index) {
        case 0: {
            /* PHI2: Read branch offset from PC into DL */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                
                /* PHI1: Check branch condition */
                bool branch_taken = ((CPU_P(this) & flag_mask) != 0) == flag_value;
                
                if (!branch_taken) {
                    /* Branch not taken: instruction completes after 2 cycles */
                    transition_to_fetch();
                    return pins;
                }
                
                /* Branch taken: calculate correct target address */
                CPU_AB(this) = CPU_PC(this) + (int8_t)CPU_DL(this);
                this->cycle_index++;
            }
            return pins;
        }
        
        case 1: {
            /* PHI2: Dummy read from incremented PC (hardware behavior) */
            /* Use a different register to avoid overwriting the branch offset in DL */
            pins = phi2_read(pins, REG_PC, REG_IR);  /* Use IR as dummy target */
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Check for page cross */
                bool page_cross = page_crossed(CPU_PC(this), CPU_AB(this));
                
                if (!page_cross) {
                    /* No page cross: set final PC and complete after 3 cycles */
                    CPU_PC(this) = CPU_AB(this);
                    transition_to_fetch();
                    return pins;
                }
                
                /* Page cross detected: need penalty cycle with intermediate address */
                /* Hardware behavior: Add signed offset to PC low byte only, ignore carry */
                /* The intermediate address = (PC & 0xFF00) | ((PCL + signed_offset) & 0xFF) */
                uint8_t pc_low = CPU_PCL(this);
                int8_t signed_offset = (int8_t)CPU_DL(this);
                uint8_t new_low = (uint8_t)(pc_low + signed_offset);  // Let it wrap naturally
                uint16_t intermediate_addr = (CPU_PC(this) & 0xFF00) | new_low;
                
                
                /* Store intermediate address in PC for penalty cycle read */
                CPU_PC(this) = intermediate_addr;
                /* CPU_AB(this) still contains the correct final target from case 0 */
                this->cycle_index++;
            }
            return pins;
        }
        
        case 2: {
            /* PHI2: Page cross penalty - dummy read from intermediate address in PC */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Set final correct target PC and complete instruction */
                /* CPU_AB(this) contains the correct target from case 1 */
                CPU_PC(this) = CPU_AB(this);
                transition_to_fetch();
            }
            return pins;
        }
    }
    return pins;
}

// ============================================================================
// CONDITIONAL BRANCH OPERATIONS
// ============================================================================

/* BCC - Branch if Carry Clear */
bus_state_t op_bcc(bus_state_t pins) {
    return branch_helper(pins, FLAG_C, false);
}

/* BCS - Branch if Carry Set */
bus_state_t op_bcs(bus_state_t pins) {
    return branch_helper(pins, FLAG_C, true);
}

/* BEQ - Branch if Equal (Zero Set) */
bus_state_t op_beq(bus_state_t pins) {
    return branch_helper(pins, FLAG_Z, true);
}

/* BNE - Branch if Not Equal (Zero Clear) */
bus_state_t op_bne(bus_state_t pins) {
    return branch_helper(pins, FLAG_Z, false);
}

/* BMI - Branch if Minus (Negative Set) */
bus_state_t op_bmi(bus_state_t pins) {
    return branch_helper(pins, FLAG_N, true);
}

/* BPL - Branch if Plus (Negative Clear) */
bus_state_t op_bpl(bus_state_t pins) {
    return branch_helper(pins, FLAG_N, false);
}

/* BVC - Branch if Overflow Clear */
bus_state_t op_bvc(bus_state_t pins) {
    return branch_helper(pins, FLAG_V, false);
}

/* BVS - Branch if Overflow Set */
bus_state_t op_bvs(bus_state_t pins) {
    return branch_helper(pins, FLAG_V, true);
}
