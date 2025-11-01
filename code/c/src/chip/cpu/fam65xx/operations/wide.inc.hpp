/*
 * wide.inc.hpp - 65C816 16-bit Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// 65C816 MODE CONTROL OPERATIONS
// ============================================================================

// REP - Reset Processor Status Bits (65C816)
bus_state_t op_rep(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Fetch immediate operand
                pins = this->phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    cycle_index++;
                }
                return pins;
                
            case 1:
                // Reset specified status bits (clear bits that are 1 in operand)
                CPU_P(this) &= ~CPU_DL(this);
                this->transition_to_fetch();
                return pins;
        }
    }
    return pins;
}

// SEP - Set Processor Status Bits (65C816)
bus_state_t op_sep(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Fetch immediate operand
                pins = this->phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    cycle_index++;
                }
                return pins;
                
            case 1:
                // Set specified status bits (set bits that are 1 in operand)
                CPU_P(this) |= CPU_DL(this);
                this->transition_to_fetch();
                return pins;
        }
    }
    return pins;
}

// XCE - Exchange Carry and Emulation Flags (65C816)
bus_state_t op_xce(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // Exchange C flag and E flag (implied operation - 1 cycle)
        bool carry = (CPU_P(this) & FLAG_C) != 0;
        bool emulation = this->get_emulation_mode();
        
        this->update_flag(FLAG_C, emulation);
        this->wide_state.emulation_mode = carry;
        
        // If switching to emulation mode, force 8-bit modes
        if (this->wide_state.emulation_mode) {
            CPU_P(this) |= (FLAG_M | FLAG_X); // Set M and X flags (8-bit modes)
        }
        
        this->transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// 65C816 ENHANCED STACK OPERATIONS
// ============================================================================

// PEA - Push Effective Absolute Address (65C816)
bus_state_t op_pea(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Read address low byte
                pins = this->phi2_read(pins, REG_PC, REG_ABL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    cycle_index++;
                }
                return pins;
                
            case 1:
                // Read address high byte
                pins = phi2_read(pins, REG_PC, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    cycle_index++;
                }
                return pins;
                
            case 2:
                // Push high byte first
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, CPU_SP(this), CPU_ABH(this));
                    CPU_S(this)--;
                    cycle_index++;
                }
                return pins;
                
            case 3:
                // Push low byte
                if (should_complete_write_cycle(pins)) {
                    pins = phi2_write(pins, CPU_SP(this), CPU_ABL(this)); // addr_low from case 0
                    CPU_S(this)--;
                    transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

// PHB - Push Data Bank Register (65C816)
bus_state_t op_phb(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // Push DBR to stack
        if (should_complete_write_cycle(pins)) {
            pins = phi2_write(pins, CPU_SP(this), this->wide_state.DBR);
            CPU_S(this)--;
            transition_to_fetch();
        }
    }
    return pins;
}

// PHD - Push Direct Page Register (65C816)
bus_state_t op_phd(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Push D high byte first
                if (this->should_complete_write_cycle(pins)) {
                    uint8_t d_high = this->wide_state.D >> 8;
                    pins = this->phi2_write(pins, CPU_SP(this), d_high);
                    CPU_S(this)--;
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Push D low byte
                if (this->should_complete_write_cycle(pins)) {
                    uint8_t d_low = this->wide_state.D & 0xFF;
                    pins = this->phi2_write(pins, CPU_SP(this), d_low);
                    CPU_S(this)--;
                    this->transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

// PHK - Push Program Bank Register (65C816)
bus_state_t op_phk(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // Push PBR to stack
        if (should_complete_write_cycle(pins)) {
            pins = phi2_write(pins, CPU_SP(this), this->wide_state.PBR);
            CPU_S(this)--;
            transition_to_fetch();
        }
    }
    return pins;
}

// PLB - Pull Data Bank Register (65C816)
bus_state_t op_plb(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Dummy read from current SP, then increment SP
                pins = phi2_dummy_read(pins, REG_SP);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_S(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Pull DBR from stack
                pins = phi2_read(pins, REG_SP, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->wide_state.DBR = CPU_DL(this);
                    // Update N and Z flags based on DBR
                    this->update_nz_flags(this->wide_state.DBR);
                    transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

// PLD - Pull Direct Page Register (65C816)
bus_state_t op_pld(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Dummy read from current SP, then increment SP for low byte
                pins = phi2_dummy_read(pins, REG_SP);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_S(this)++;
                    cycle_index++;
                }
                return pins;
                
            case 1:
                // Pull D register low byte first
                pins = phi2_read(pins, REG_SP, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->wide_state.D = (this->wide_state.D & 0xFF00) | CPU_DL(this); // Set low byte
                    CPU_S(this)++; // Increment for high byte
                    cycle_index++;
                }
                return pins;
                
            case 2:
                // Pull D register high byte
                pins = phi2_read(pins, REG_SP, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->wide_state.D = (this->wide_state.D & 0x00FF) | (CPU_DL(this) << 8); // Set high byte
                    // Update N and Z flags based on D register
                    this->update_nz_flags(this->wide_state.D & 0xFF); // Only check low byte for flags
                    transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

// ============================================================================
// 65C816 LONG ADDRESSING OPERATIONS
// ============================================================================

// JSL - Jump to Subroutine Long (65C816)
bus_state_t op_jsl(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (cycle_index) {
            case 0:
                // Read address low byte
                pins = phi2_read(pins, REG_PC, REG_ABL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    cycle_index++;
                }
                return pins;
                
            case 1:
                // Read address high byte
                pins = phi2_read(pins, REG_PC, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    cycle_index++;
                }
                return pins;
                
            case 2:
                // Read bank byte
                pins = phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    cycle_index++;
                }
                return pins;
                
            case 3:
                // Push program bank register
                if (should_complete_write_cycle(pins)) {
                    // Store PBR in TMP for pushing
                    pins = phi2_write(pins, CPU_SP(this), this->wide_state.PBR);
                    CPU_S(this)--;
                    cycle_index++;
                }
                return pins;
                
            case 4:
                // Push PC high byte (return address - 1)
                if (should_complete_write_cycle(pins)) {
                    pins = phi2_write(pins, CPU_SP(this), CPU_PCH(this));
                    CPU_S(this)--;
                    cycle_index++;
                }
                return pins;
                
            case 5:
                // Push PC low byte
                if (should_complete_write_cycle(pins)) {
                    pins = phi2_write(pins, CPU_SP(this), CPU_PCL(this));
                    CPU_S(this)--;
                    // Set new program counter and bank
                    CPU_PC(this) = CPU_AB(this); // addr_high:addr_low from cases 0-1
                    this->wide_state.PBR = CPU_DL(this); // bank from case 2
                    transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

// RTL - Return from Subroutine Long (65C816)
bus_state_t op_rtl(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (cycle_index) {
            case 0:
                // Dummy read from current SP, then increment SP for PCL
                pins = phi2_dummy_read(pins, REG_SP);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_S(this)++;
                    cycle_index++;
                }
                return pins;
                
            case 1:
                // Pull PC low byte
                pins = phi2_read(pins, REG_SP, REG_PCL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_S(this)++; // Increment for PCH
                    cycle_index++;
                }
                return pins;
                
            case 2:
                // Pull PC high byte
                pins = phi2_read(pins, REG_SP, REG_PCH);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_S(this)++; // Increment for PBR
                    cycle_index++;
                }
                return pins;
                
            case 3:
                // Pull program bank
                pins = phi2_read(pins, REG_SP, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->wide_state.PBR = CPU_DL(this);
                    // Increment PC (RTL increments, RTS doesn't)
                    CPU_PC(this)++;
                    transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"