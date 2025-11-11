/*
 * stack.inc.hpp - Stack Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// STACK OPERATIONS  
// ============================================================================

/* PHA - Push Accumulator */
bus_state_t op_pha(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_PC));
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Write A to stack with processor-specific RDY handling */
            if (this->should_complete_write_cycle(pins)) {
                pins = this->phi2_write(pins, static_cast<reg16_t>(REG_SP), this->get(REG_A));
                this->dec(REG_S);
                transition_to_fetch();
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
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_PC));
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            this->set(REG_DL, this->get(REG_P) | FLAG_B | FLAG_U);
            return pins;
            
        case 1:
            /* PHI2: Write P|B|U to stack with processor-specific RDY handling */
            if (this->should_complete_write_cycle(pins)) {
                pins = this->phi2_write(pins, static_cast<reg16_t>(REG_SP), REG_DL);
                this->dec(REG_S);
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
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_PC));
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Dummy read from current stack pointer, then increment SP */
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_SP));
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Increment stack pointer */
                this->inc(REG_S);
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            /* PHI2: Read from incremented stack pointer directly into A (eliminates copy) */
            pins = this->phi2_read(pins, static_cast<reg16_t>(REG_SP), static_cast<reg8_t>(REG_A));
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Set flags based on accumulator value */
                update_nz_flags(this->get(REG_A));
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
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_PC));
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Dummy read from current stack pointer, then increment SP */
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_SP));
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Increment stack pointer */
                this->inc(REG_S);
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            /* PHI2: Read from incremented stack pointer */
            pins = this->phi2_read(pins, static_cast<reg16_t>(REG_SP), static_cast<reg8_t>(REG_DL));
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Store in P (clear B, set U) */
                this->set(REG_P, (this->get(REG_DL) & ~FLAG_B) | FLAG_U);
                transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
