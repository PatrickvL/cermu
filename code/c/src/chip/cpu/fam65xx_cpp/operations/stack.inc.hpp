/*
 * stack.inc - Stack Operations for MOS 65xx Family
 */

// ============================================================================
// STACK OPERATIONS  
// ============================================================================

/* PHA - Push Accumulator */
bus_state_t op_pha(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Write A to stack (0x0100 | S) */
            pins = phi2_write(pins, REG_SP, REG_A);
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Decrement stack pointer */
                CPU_S(this)--;
                this->transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

/* PHP - Push Processor Status */
bus_state_t op_php(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Write P|B|U to stack (REG_SP already contains 0x0100 | S) */
            CPU_DL(this) = CPU_P(this) | FLAG_B | FLAG_U;
            pins = phi2_write(pins, REG_SP, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Decrement stack pointer */
                CPU_S(this)--;
                transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

/* PLA - Pull Accumulator */
bus_state_t op_pla(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* PHI2: Dummy read from PC */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Dummy read from current stack pointer, then increment SP */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Increment stack pointer */
                CPU_S(this)++;
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            /* PHI2: Read from incremented stack pointer directly into A (eliminates copy) */
            pins = phi2_read(pins, REG_SP, REG_A);
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Set flags based on accumulator value */
                update_nz_flags(CPU_A(this));
                transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

/* PLP - Pull Processor Status */
bus_state_t op_plp(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* PHI2: Dummy read from PC */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Dummy read from current stack pointer, then increment SP */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Increment stack pointer */
                CPU_S(this)++;
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            /* PHI2: Read from incremented stack pointer */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Store in P (clear B, set U) */
                CPU_P(this) = (BUS_GET_DATA(pins) & ~FLAG_B) | FLAG_U;
                transition_to_fetch();
            }
            return pins;
    }
    return pins;
}