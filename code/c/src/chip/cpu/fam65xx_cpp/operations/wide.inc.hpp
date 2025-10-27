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
        switch (this->cycle_index) {
            case 0:
                // Fetch immediate operand
                CPU_AB(this) = CPU_PC(this);
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Reset specified status bits (clear bits that are 1 in operand)
                CPU_P(this) &= ~CPU_DL(this);
                this->transition_to_fetch();
                return pins;
        }
    } else {
        // Not supported on this processor - treat as NOP
        this->transition_to_fetch();
    }
    return pins;
}

// SEP - Set Processor Status Bits (65C816)
bus_state_t op_sep(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        switch (this->cycle_index) {
            case 0:
                // Fetch immediate operand
                CPU_AB(this) = CPU_PC(this);
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Set specified status bits (set bits that are 1 in operand)
                CPU_P(this) |= CPU_DL(this);
                this->transition_to_fetch();
                return pins;
        }
    } else {
        // Not supported on this processor - treat as NOP
        this->transition_to_fetch();
    }
    return pins;
}

// XCE - Exchange Carry and Emulation Flags (65C816)
bus_state_t op_xce(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        switch (this->cycle_index) {
            case 0:
                // Exchange C flag and E flag (implied operation - 1 cycle)
                bool carry = (CPU_P(this) & FLAG_C) != 0;
                bool emulation = this->wide_state.emulation_mode;
                
                this->update_flag(FLAG_C, emulation);
                this->wide_state.emulation_mode = carry;
                
                // If switching to emulation mode, force 8-bit modes
                if (this->wide_state.emulation_mode) {
                    CPU_P(this) |= (FLAG_M | FLAG_X); // Set M and X flags (8-bit modes)
                }
                
                this->transition_to_fetch();
                return pins;
        }
    } else {
        // Not supported on this processor - treat as NOP
        this->transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// 65C816 ENHANCED STACK OPERATIONS
// ============================================================================

// PEA - Push Effective Absolute Address (65C816)
bus_state_t op_pea(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        switch (this->cycle_index) {
            case 0:
                // Read address low byte
                CPU_AB(this) = CPU_PC(this);
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Store low byte in IR, read address high byte
                CPU_IR(this) = CPU_DL(this); // Store addr_low in IR temporarily
                CPU_AB(this) = CPU_PC(this);
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Push high byte first
                CPU_AB(this) = CPU_SP(this);
                // Don't modify DL here - it contains addr_high from previous read
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_AB, REG_DL);
                    CPU_S(this)--;
                    this->cycle_index++;
                }
                return pins;
                
            case 3:
                // Push low byte
                CPU_AB(this) = CPU_SP(this);
                CPU_DL(this) = CPU_IR(this); // addr_low from case 1
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_AB, REG_DL);
                    CPU_S(this)--;
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Not supported on this processor - treat as NOP
        this->transition_to_fetch();
    }
    return pins;
}

// PHB - Push Data Bank Register (65C816)
bus_state_t op_phb(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        switch (this->cycle_index) {
            case 0:
                // Push DBR to stack
                CPU_AB(this) = CPU_SP(this);
                CPU_DL(this) = this->wide_state.DBR;
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_AB, REG_DL);
                    CPU_S(this)--;
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Not supported on this processor - treat as NOP
        this->transition_to_fetch();
    }
    return pins;
}

// PHD - Push Direct Page Register (65C816)
bus_state_t op_phd(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        switch (this->cycle_index) {
            case 0:
                // Push D high byte first
                CPU_AB(this) = CPU_SP(this);
                CPU_DL(this) = this->wide_state.D >> 8;
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_AB, REG_DL);
                    CPU_S(this)--;
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Push D low byte
                CPU_AB(this) = CPU_SP(this);
                CPU_DL(this) = this->wide_state.D & 0xFF;
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_AB, REG_DL);
                    CPU_S(this)--;
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Not supported on this processor - treat as NOP
        this->transition_to_fetch();
    }
    return pins;
}

// PHK - Push Program Bank Register (65C816)
bus_state_t op_phk(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        switch (this->cycle_index) {
            case 0:
                // Push PBR to stack
                CPU_AB(this) = CPU_SP(this);
                CPU_DL(this) = this->wide_state.PBR;
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_AB, REG_DL);
                    CPU_S(this)--;
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Not supported on this processor - treat as NOP
        this->transition_to_fetch();
    }
    return pins;
}

// PLB - Pull Data Bank Register (65C816)
bus_state_t op_plb(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        switch (this->cycle_index) {
            case 0:
                // Pull DBR from stack
                CPU_S(this)++;
                CPU_AB(this) = CPU_SP(this);
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->wide_state.DBR = CPU_DL(this);
                    // Update N and Z flags based on DBR
                    this->update_nz_flags(this->wide_state.DBR);
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        this->transition_to_fetch();
    }
    return pins;
}

// PLD - Pull Direct Page Register (65C816)
bus_state_t op_pld(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        switch (this->cycle_index) {
            case 0:
                // Pull D register low byte first
                CPU_S(this)++;
                CPU_AB(this) = CPU_SP(this);
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->wide_state.D = (this->wide_state.D & 0xFF00) | CPU_DL(this); // Set low byte
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Pull D register high byte
                CPU_S(this)++;
                CPU_AB(this) = CPU_SP(this);
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->wide_state.D = (this->wide_state.D & 0x00FF) | (CPU_DL(this) << 8); // Set high byte
                    // Update N and Z flags based on D register
                    this->update_nz_flags(this->wide_state.D & 0xFF); // Only check low byte for flags
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        this->transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// 65C816 LONG ADDRESSING OPERATIONS
// ============================================================================

// JSL - Jump to Subroutine Long (65C816)
bus_state_t op_jsl(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        switch (this->cycle_index) {
            case 0:
                // Read address low byte
                CPU_AB(this) = CPU_PC(this);
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    CPU_IR(this) = CPU_DL(this); // Store addr_low in IR
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Read address high byte
                CPU_AB(this) = CPU_PC(this);
                pins = this->phi2_read(pins, REG_AB, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    CPU_ABL(this) = CPU_IR(this); // Restore addr_low to ABL
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Read bank byte
                CPU_AB(this) = CPU_PC(this);
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 3:
                // Push program bank register
                CPU_AB(this) = CPU_SP(this);
                CPU_IR(this) = this->wide_state.PBR; // Store PBR in IR
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, REG_IR);
                    CPU_S(this)--;
                    this->cycle_index++;
                }
                return pins;
                
            case 4:
                // Push PC high byte (return address - 1)
                CPU_AB(this) = CPU_SP(this);
                CPU_IR(this) = CPU_PCH(this);
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, REG_IR);
                    CPU_S(this)--;
                    this->cycle_index++;
                }
                return pins;
                
            case 5:
                // Push PC low byte
                CPU_AB(this) = CPU_SP(this);
                CPU_IR(this) = CPU_PCL(this);
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, REG_IR);
                    CPU_S(this)--;
                    // Set new program counter and bank
                    CPU_PC(this) = CPU_AB(this); // addr_high:addr_low from cases 0-1
                    this->wide_state.PBR = CPU_DL(this); // bank from case 2
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        this->transition_to_fetch();
    }
    return pins;
}

// RTL - Return from Subroutine Long (65C816)
bus_state_t op_rtl(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        switch (this->cycle_index) {
            case 0:
                // Pull PC low byte
                CPU_S(this)++;
                CPU_AB(this) = CPU_SP(this);
                pins = this->phi2_read(pins, REG_AB, REG_PCL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Pull PC high byte
                CPU_S(this)++;
                CPU_AB(this) = CPU_SP(this);
                pins = this->phi2_read(pins, REG_AB, REG_PCH);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Pull program bank
                CPU_S(this)++;
                CPU_AB(this) = CPU_SP(this);
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->wide_state.PBR = CPU_DL(this);
                    // Increment PC (RTL increments, RTS doesn't)
                    CPU_PC(this)++;
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        this->transition_to_fetch();
    }
    return pins;
}