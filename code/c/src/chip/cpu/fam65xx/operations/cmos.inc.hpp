/*
 * cmos.inc.hpp - 65C02 Enhanced Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// 65C02 ENHANCED INSTRUCTIONS
// ============================================================================

// BRA - Branch Always (65C02)
bus_state_t op_bra(bus_state_t pins) {
    if constexpr (has_cmos()) {
        // Read relative offset
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (FAM65XX_GET_RDY(pins)) {
            CPU_PC(this)++;
            
            // Apply branch offset
            int8_t offset = static_cast<int8_t>(CPU_DL(this));
            CPU_PC(this) += offset;
            
            transition_to_fetch();
        }
        return pins;
    } else {
        // Invalid on NMOS processors - treat as NOP
        transition_to_fetch();
        return pins;
    }
}

// STZ - Store Zero (65C02)
bus_state_t op_stz(bus_state_t pins) {
    if constexpr (has_cmos()) {
        // Store zero to target address with proper RDY handling
        if (this->should_complete_write_cycle(pins)) {
            pins = phi2_write(pins, REG_AB, REG_ZERO);
            transition_to_fetch();
        }
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// TRB - Test and Reset Bits (65C02) - Hardware-accurate 3-cycle RMW
bus_state_t op_trb(bus_state_t pins) {
    if constexpr (has_cmos()) {
        // Hardware-accurate 3-cycle Read-Modify-Write operation
        switch (this->cycle_index) {
            case 0:
                // Cycle 0: Read original value from memory
                pins = phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Cycle 1: Dummy write original value back + modify
                if (this->should_complete_write_cycle(pins)) {
                    pins = phi2_write(pins, REG_AB, REG_DL);
                    
                    uint8_t accumulator = CPU_A(this);
                    
                    // Test bits (set Z flag if A & memory == 0)
                    uint8_t test_result = CPU_DL(this) & accumulator;
                    update_flags(FLAG_Z, test_result == 0);
                    
                    // Reset bits (memory = memory & ~A)
                    CPU_DL(this) &= ~accumulator;
                    
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Cycle 2: Write modified result back
                if (this->should_complete_write_cycle(pins)) {
                    pins = phi2_write(pins, REG_AB, REG_DL);
                    transition_to_fetch();
                }
                return pins;
        }
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// TSB - Test and Set Bits (65C02) - Hardware-accurate 3-cycle RMW
bus_state_t op_tsb(bus_state_t pins) {
    if constexpr (has_cmos()) {
        // Hardware-accurate 3-cycle Read-Modify-Write operation
        switch (this->cycle_index) {
            case 0:
                // Cycle 0: Read original value from memory
                pins = phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Cycle 1: Dummy write original value back + modify
                if (this->should_complete_write_cycle(pins)) {
                    pins = phi2_write(pins, REG_AB, REG_DL);
                    
                    uint8_t accumulator = CPU_A(this);
                    
                    // Test bits (set Z flag if A & memory == 0)
                    uint8_t test_result = CPU_DL(this) & accumulator;
                    update_flags(FLAG_Z, test_result == 0);
                    
                    // Set bits (memory = memory | A)
                    CPU_DL(this) |= accumulator;
                    
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Cycle 2: Write modified result back
                if (this->should_complete_write_cycle(pins)) {
                    pins = phi2_write(pins, REG_AB, REG_DL);
                    transition_to_fetch();
                }
                return pins;
        }
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// WAI - Wait for Interrupt (65C02)
bus_state_t op_wai(bus_state_t pins) {
    if constexpr (has_cmos()) {
        // Set wait state
        this->wait_for_interrupt = true;
        
        // CPU halts until interrupt occurs
        // The tick() function will check this flag
        transition_to_fetch();
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// STP - Stop (65C02)
bus_state_t op_stp(bus_state_t pins) {
    if constexpr (has_cmos()) {
        // Set stopped state
        this->stopped = true;
        
        // CPU halts until reset
        transition_to_fetch();
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// PHX - Push X Register (65C02) - Hardware-accurate 3-cycle operation like PHA
bus_state_t op_phx(bus_state_t pins) {
    if constexpr (has_cmos()) {
        switch (this->cycle_index) {
            case 0:
                /* Dummy cycle for internal operation */
                pins = phi2_read(pins, REG_PC, REG_TMP);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                /* PHI2: Write X to stack with processor-specific RDY handling */
                if (this->should_complete_write_cycle(pins)) {
                    pins = phi2_write(pins, REG_SP, REG_X);
                    CPU_S(this)--;
                    transition_to_fetch();
                }
                return pins;
        }
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// PHY - Push Y Register (65C02) - Hardware-accurate 3-cycle operation like PHA
bus_state_t op_phy(bus_state_t pins) {
    if constexpr (has_cmos()) {
        switch (this->cycle_index) {
            case 0:
                /* Dummy cycle for internal operation */
                pins = phi2_read(pins, REG_PC, REG_TMP);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                /* PHI2: Write Y to stack with processor-specific RDY handling */
                if (this->should_complete_write_cycle(pins)) {
                    pins = phi2_write(pins, REG_SP, REG_Y);
                    CPU_S(this)--;
                    transition_to_fetch();
                }
                return pins;
        }
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// PLX - Pull X Register (65C02) - Hardware-accurate 4-cycle operation like PLA
bus_state_t op_plx(bus_state_t pins) {
    if constexpr (has_cmos()) {
        switch (this->cycle_index) {
            case 0:
                /* PHI2: Dummy read from PC */
                pins = phi2_read(pins, REG_PC, REG_TMP);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                /* PHI2: Dummy read from current stack pointer, then increment SP */
                pins = phi2_read(pins, REG_SP, REG_TMP);
                if (FAM65XX_GET_RDY(pins)) {
                    /* PHI1: Increment stack pointer */
                    CPU_S(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                /* PHI2: Read from incremented stack pointer directly into X (eliminates copy) */
                pins = phi2_read(pins, REG_SP, REG_X);
                if (FAM65XX_GET_RDY(pins)) {
                    /* PHI1: Set flags based on X register value */
                    update_nz_flags(CPU_X(this));
                    transition_to_fetch();
                }
                return pins;
        }
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// PLY - Pull Y Register (65C02) - Hardware-accurate 4-cycle operation like PLA
bus_state_t op_ply(bus_state_t pins) {
    if constexpr (has_cmos()) {
        switch (this->cycle_index) {
            case 0:
                /* PHI2: Dummy read from PC */
                pins = phi2_read(pins, REG_PC, REG_TMP);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                /* PHI2: Dummy read from current stack pointer, then increment SP */
                pins = phi2_read(pins, REG_SP, REG_TMP);
                if (FAM65XX_GET_RDY(pins)) {
                    /* PHI1: Increment stack pointer */
                    CPU_S(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                /* PHI2: Read from incremented stack pointer directly into Y (eliminates copy) */
                pins = phi2_read(pins, REG_SP, REG_Y);
                if (FAM65XX_GET_RDY(pins)) {
                    /* PHI1: Set flags based on Y register value */
                    update_nz_flags(CPU_Y(this));
                    transition_to_fetch();
                }
                return pins;
        }
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
