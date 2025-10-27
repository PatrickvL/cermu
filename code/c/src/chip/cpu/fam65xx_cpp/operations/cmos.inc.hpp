/*
 * cmos.inc - 65C02 Enhanced Operations for MOS 65xx Family
 *
 * This file contains 65C02-specific operation implementations that are
 * conditionally compiled based on processor support.
 */

// ============================================================================
// 65C02 ENHANCED INSTRUCTIONS
// ============================================================================

// BRA - Branch Always (65C02)
bus_state_t op_bra(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // Read relative offset
        CPU_AB(this) = CPU_PC(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        CPU_PC(this)++;
        
        // Apply branch offset
        int8_t offset = static_cast<int8_t>(CPU_DL(this));
        CPU_PC(this) += offset;
        
        transition_to_fetch();
        return pins;
    } else {
        // Invalid on NMOS processors - treat as NOP
        transition_to_fetch();
        return pins;
    }
}

// STZ - Store Zero (65C02)
bus_state_t op_stz(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // Store zero to target address
        CPU_DL(this) = 0x00;
        pins = phi2_write(pins, REG_AB, REG_DL);
        
        transition_to_fetch();
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// TRB - Test and Reset Bits (65C02)
bus_state_t op_trb(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // Read-Modify-Write operation
        
        // Read current value
        pins = phi2_read(pins, REG_AB, REG_DL);
        
        uint8_t memory = CPU_DL(this);
        uint8_t accumulator = CPU_A(this);
        
        // Test bits (set Z flag if A & memory == 0)
        uint8_t test_result = memory & accumulator;
        update_flags(FLAG_Z, test_result == 0);
        
        // Reset bits (memory = memory & ~A)
        CPU_DL(this) = memory & ~accumulator;
        
        // Write back
        pins = phi2_write(pins, REG_AB, REG_DL);
        
        transition_to_fetch();
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// TSB - Test and Set Bits (65C02)
bus_state_t op_tsb(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // Read-Modify-Write operation
        
        // Read current value
        pins = phi2_read(pins, REG_AB, REG_DL);
        
        uint8_t memory = CPU_DL(this);
        uint8_t accumulator = CPU_A(this);
        
        // Test bits (set Z flag if A & memory == 0)
        uint8_t test_result = memory & accumulator;
        update_flags(FLAG_Z, test_result == 0);
        
        // Set bits (memory = memory | A)
        CPU_DL(this) = memory | accumulator;
        
        // Write back
        pins = phi2_write(pins, REG_AB, REG_DL);
        
        transition_to_fetch();
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// WAI - Wait for Interrupt (65C02)
bus_state_t op_wai(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
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
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
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

// PHX - Push X Register (65C02)
bus_state_t op_phx(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // Push X to stack
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = CPU_X(this);
        pins = phi2_write_internal(pins, REG_AB, REG_DL);
        CPU_S(this)--;
        
        transition_to_fetch();
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// PHY - Push Y Register (65C02)
bus_state_t op_phy(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // Push Y to stack
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = CPU_Y(this);
        pins = phi2_write_internal(pins, REG_AB, REG_DL);
        CPU_S(this)--;
        
        transition_to_fetch();
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// PLX - Pull X Register (65C02)
bus_state_t op_plx(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // Pull X from stack
        CPU_S(this)++;
        CPU_AB(this) = CPU_SP(this);
        pins = phi2_read(pins, REG_AB, REG_X);
        
        // Update N and Z flags
        update_nz_flags(CPU_X(this));
        
        transition_to_fetch();
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}

// PLY - Pull Y Register (65C02)
bus_state_t op_ply(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // Pull Y from stack
        CPU_S(this)++;
        CPU_AB(this) = CPU_SP(this);
        pins = phi2_read(pins, REG_AB, REG_Y);
        
        // Update N and Z flags
        update_nz_flags(CPU_Y(this));
        
        transition_to_fetch();
        return pins;
    } else {
        transition_to_fetch();
        return pins;
    }
}
