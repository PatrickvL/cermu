/*
 * wide.inc - 65C816 16-bit Operations for MOS 65xx Family
 *
 * This file contains 65C816-specific 16-bit operation implementations that are
 * conditionally compiled based on processor support for wide registers.
 */

// ============================================================================
// 65C816 MODE CONTROL OPERATIONS
// ============================================================================

// REP - Reset Processor Status Bits (65C816)
bus_state_t op_rep(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Read immediate operand
        CPU_AB(this) = CPU_PC(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        CPU_PC(this)++;
        
        // Reset specified status bits (clear bits that are 1 in operand)
        CPU_P(this) &= ~CPU_DL(this);
    }
    
    transition_to_fetch();
    return pins;
}

// SEP - Set Processor Status Bits (65C816)
bus_state_t op_sep(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Read immediate operand
        CPU_AB(this) = CPU_PC(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        CPU_PC(this)++;
        
        // Set specified status bits (set bits that are 1 in operand)
        CPU_P(this) |= CPU_DL(this);
    }

    transition_to_fetch();
    return pins;
}

// XCE - Exchange Carry and Emulation Flags (65C816)
bus_state_t op_xce(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Exchange C flag and E flag
        bool carry = (CPU_P(this) & FLAG_C) != 0;
        bool emulation = this->wide_state.emulation_mode;
        
        this->update_flag(FLAG_C, emulation);
        
        this->wide_state.emulation_mode = carry;
        
        // If switching to emulation mode, force 8-bit modes
        if (this->wide_state.emulation_mode) {
            CPU_P(this) |= (FLAG_M | FLAG_X); // Set M and X flags (8-bit modes)
        }
    }

    transition_to_fetch();
    return pins;
}

// ============================================================================
// 65C816 ENHANCED STACK OPERATIONS
// ============================================================================

// PEA - Push Effective Absolute Address (65C816)
bus_state_t op_pea(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Read 16-bit immediate address
        CPU_AB(this) = CPU_PC(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        CPU_PC(this)++;
        
        uint8_t addr_low = CPU_DL(this);
        
        CPU_AB(this) = CPU_PC(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        CPU_PC(this)++;
        
        uint8_t addr_high = CPU_DL(this);
        
        // Push high byte first
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = addr_high;
        pins = phi2_write(pins, REG_AB, REG_DL);
        CPU_S(this)--;
        
        // Push low byte
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = addr_low;
        pins = phi2_write(pins, REG_AB, REG_DL);
        CPU_S(this)--;
    }

    transition_to_fetch();
    return pins;
}

// PHB - Push Data Bank Register (65C816)
bus_state_t op_phb(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Push DBR to stack
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = this->wide_state.DBR;
        pins = phi2_write(pins, REG_AB, REG_DL);
        CPU_S(this)--;
    }
    
    transition_to_fetch();
    return pins;
}

// PHD - Push Direct Page Register (65C816)
bus_state_t op_phd(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Push D register (16-bit) to stack
        uint16_t d_reg = this->wide_state.D;
        
        // Push high byte first
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = d_reg >> 8;
        pins = phi2_write(pins, REG_AB, REG_DL);
        CPU_S(this)--;
        
        // Push low byte
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = d_reg & 0xFF;
        pins = phi2_write(pins, REG_AB, REG_DL);
        CPU_S(this)--;
    }
    transition_to_fetch();
    return pins;
}

// PHK - Push Program Bank Register (65C816)
bus_state_t op_phk(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Push PBR to stack
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = this->wide_state.PBR;
        pins = phi2_write(pins, REG_AB, REG_DL);
        CPU_S(this)--;
    }
    transition_to_fetch();
    return pins;
}

// PLB - Pull Data Bank Register (65C816)
bus_state_t op_plb(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Pull DBR from stack
        CPU_S(this)++;
        CPU_AB(this) = CPU_SP(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        
        this->wide_state.DBR = CPU_DL(this);
        
        // Update N and Z flags based on DBR
        update_nz_flags(this->wide_state.DBR);
    }

    transition_to_fetch();
    return pins;
}

// PLD - Pull Direct Page Register (65C816)
bus_state_t op_pld(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Pull D register (16-bit) from stack
        
        // Pull low byte first
        CPU_S(this)++;
        CPU_AB(this) = CPU_SP(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        uint8_t low_byte = CPU_DL(this);
        
        // Pull high byte
        CPU_S(this)++;
        CPU_AB(this) = CPU_SP(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        uint8_t high_byte = CPU_DL(this);
        
        this->wide_state.D = (high_byte << 8) | low_byte;
        
        // Update N and Z flags based on D register
        update_nz_flags(this->wide_state.D & 0xFF); // Only check low byte for flags
    }
    transition_to_fetch();
    return pins;
}

// ============================================================================
// 65C816 LONG ADDRESSING OPERATIONS
// ============================================================================

// JSL - Jump to Subroutine Long (65C816)
bus_state_t op_jsl(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Read 24-bit address
        CPU_AB(this) = CPU_PC(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        CPU_PC(this)++;
        uint8_t addr_low = CPU_DL(this);
        
        CPU_AB(this) = CPU_PC(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        CPU_PC(this)++;
        uint8_t addr_mid = CPU_DL(this);
        
        CPU_AB(this) = CPU_PC(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        CPU_PC(this)++;
        uint8_t addr_high = CPU_DL(this);
        
        // Push return address (24-bit: PBR, PC-1)
        uint16_t return_pc = CPU_PC(this) - 1;
        
        // Push PBR
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = this->wide_state.PBR;
        pins = phi2_write(pins, REG_AB, REG_DL);
        CPU_S(this)--;
        
        // Push PC high
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = return_pc >> 8;
        pins = phi2_write(pins, REG_AB, REG_DL);
        CPU_S(this)--;
        
        // Push PC low
        CPU_AB(this) = CPU_SP(this);
        CPU_DL(this) = return_pc & 0xFF;
        pins = phi2_write(pins, REG_AB, REG_DL);
        CPU_S(this)--;
        
        // Set new PC and PBR
        CPU_PC(this) = (addr_mid << 8) | addr_low;
        this->wide_state.PBR = addr_high;
    }

    transition_to_fetch();
    return pins;
}

// RTL - Return from Subroutine Long (65C816)
bus_state_t op_rtl(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Pull 24-bit return address
        
        // Pull PC low
        CPU_S(this)++;
        CPU_AB(this) = CPU_SP(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        uint8_t pc_low = CPU_DL(this);
        
        // Pull PC high
        CPU_S(this)++;
        CPU_AB(this) = CPU_SP(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        uint8_t pc_high = CPU_DL(this);
        
        // Pull PBR
        CPU_S(this)++;
        CPU_AB(this) = CPU_SP(this);
        pins = phi2_read(pins, REG_AB, REG_DL);
        this->wide_state.PBR = CPU_DL(this);
        
        // Set PC (increment by 1 for RTL)
        CPU_PC(this) = ((pc_high << 8) | pc_low) + 1;
    }

    transition_to_fetch();
    return pins;
}