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
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Reset specified status bits (clear bits that are 1 in operand)
                this->set(REG_P, this->get(REG_P) & ~this->get(REG_DL));
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
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Set specified status bits (set bits that are 1 in operand)
                this->set(REG_P, this->get(REG_P) | this->get(REG_DL));
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
        bool carry = (this->get(REG_P) & FLAG_C) != 0;
        bool emulation = this->get_emulation_mode();
        
        this->update_flag(FLAG_C, emulation);
        this->set_emulation_mode(carry);
        
        // If switching to emulation mode, force 8-bit modes
        if (carry) {
            this->set(REG_P, this->get(REG_P) | (FLAG_M | FLAG_X)); // Set M and X flags (8-bit modes)
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
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Read address high byte
                pins = this->phi2_read(pins, REG_PC, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Push high byte first
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_ABH));
                    this->dec(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 3:
                // Push low byte
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_ABL)); // addr_low from case 0
                    this->dec(REG_S);
                    this->transition_to_fetch();
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
        if (this->should_complete_write_cycle(pins)) {
            pins = this->phi2_write(pins, REG_SP, this->get(REG_DBR));
            this->dec(REG_S);
            this->transition_to_fetch();
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
                    uint8_t d_high = this->get(REG_DH);
                    pins = this->phi2_write(pins, REG_SP, d_high);
                    this->dec(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Push D low byte
                if (this->should_complete_write_cycle(pins)) {
                    uint8_t d_low = this->get(REG_DLow);
                    pins = this->phi2_write(pins, REG_SP, d_low);
                    this->dec(REG_S);
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
        if (this->should_complete_write_cycle(pins)) {
            pins = this->phi2_write(pins, REG_SP, this->get(REG_PBR));
            this->dec(REG_S);
            this->transition_to_fetch();
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
                pins = this->phi2_dummy_read(pins, REG_SP);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Pull DBR from stack
                pins = this->phi2_read(pins, REG_SP, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->set(REG_DBR, this->get(REG_DL));
                    // Update N and Z flags based on DBR
                    this->update_nz_flags(this->get(REG_DBR));
                    this->transition_to_fetch();
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
                pins = this->phi2_dummy_read(pins, REG_SP);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Pull D register low byte first
                pins = this->phi2_read(pins, REG_SP, REG_DLow);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_S); // Increment for high byte
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Pull D register high byte
                pins = this->phi2_read(pins, REG_SP, REG_DH);
                if (FAM65XX_GET_RDY(pins)) {
                    // Update N and Z flags based on D register
                    this->update_nz_flags(this->get(REG_DLow)); // Only check low byte for flags
                    this->transition_to_fetch();
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
        switch (this->cycle_index) {
            case 0:
                // Read address low byte
                pins = this->phi2_read(pins, REG_PC, REG_ABL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Read address high byte
                pins = this->phi2_read(pins, REG_PC, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Read bank byte
                pins = this->phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 3:
                // Push program bank register
                if (this->should_complete_write_cycle(pins)) {
                    // Store PBR in TMP for pushing
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_PBR));
                    this->dec(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 4:
                // Push PC high byte (return address - 1)
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_PCH));
                    this->dec(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 5:
                // Push PC low byte
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_PCL));
                    this->dec(REG_S);
                    // Set new program counter and bank
                    this->set(REG_PC, this->get(REG_AB)); // addr_high:addr_low from cases 0-1
                    this->set(REG_PBR, this->get(REG_DL)); // bank from case 2
                    this->transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

// RTL - Return from Subroutine Long (65C816)
bus_state_t op_rtl(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Dummy read from current SP, then increment SP for PCL
                pins = this->phi2_dummy_read(pins, REG_SP);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Pull PC low byte
                pins = this->phi2_read(pins, REG_SP, REG_PCL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_S); // Increment for PCH
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Pull PC high byte
                pins = this->phi2_read(pins, REG_SP, REG_PCH);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_S); // Increment for PBR
                    this->cycle_index++;
                }
                return pins;
                
            case 3:
                // Pull program bank
                pins = this->phi2_read(pins, REG_SP, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->set(REG_PBR, this->get(REG_DL));
                    // Increment PC (RTL increments, RTS doesn't)
                    this->inc(REG_PC);
                    this->transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

// PER - Push Effective Relative Address (65C816)
bus_state_t op_per(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Read relative offset low byte
                pins = this->phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Read relative offset high byte
                pins = this->phi2_read(pins, REG_PC, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    // Calculate effective address: PC + signed offset
                    int16_t offset = (this->get(REG_ABH) << 8) | this->get(REG_DL);
                    uint16_t effective_addr = this->get(REG_PC) + offset;
                    this->set(REG_AB, effective_addr);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Push high byte of effective address
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_ABH));
                    this->dec(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 3:
                // Push low byte of effective address
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_ABL));
                    this->dec(REG_S);
                    this->transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

// PEI - Push Effective Indirect Address (65C816)
bus_state_t op_pei(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Read zero page address
                pins = this->phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    // Calculate direct page relative address
                    uint16_t dp_addr = this->get(REG_D) + this->get(REG_DL);
                    this->set(REG_AB, dp_addr);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Read low byte of indirect address
                pins = this->phi2_read(pins, REG_AB, REG_ABL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_AB);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Read high byte of indirect address
                pins = this->phi2_read(pins, REG_AB, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 3:
                // Push high byte of effective address
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_ABH));
                    this->dec(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 4:
                // Push low byte of effective address
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_ABL));
                    this->dec(REG_S);
                    this->transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

// XBA - Exchange B and A (65C816)
bus_state_t op_xba(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // Exchange the low and high bytes of the 16-bit accumulator
        uint16_t a = this->get(REG_A_FULL);
        // Swap the bytes by setting them directly
        this->set(REG_A_FULL, (a >> 8) | (a << 8));
        
        // Update N and Z flags based on new A register value (now contains old high byte)
        this->update_nz_flags(this->get(REG_A));
        
        this->transition_to_fetch();
    }
    return pins;
}

// MVN - Move Negative (65C816)
bus_state_t op_mvn(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Read destination bank
                pins = this->phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Read source bank
                pins = this->phi2_read(pins, REG_PC, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Read from source address (bank:X)
                {
                    uint32_t src_addr = (this->get(REG_ABH) << 16) | this->get(REG_X);
                    this->set(REG_AB, src_addr & 0xFFFF);
                    pins = this->phi2_read(pins, REG_AB, REG_ABL);
                    if (FAM65XX_GET_RDY(pins)) {
                        this->cycle_index++;
                    }
                }
                return pins;
                
            case 3:
                // Write to destination address (bank:Y)
                if (this->should_complete_write_cycle(pins)) {
                    uint32_t dst_addr = (this->get(REG_DL) << 16) | this->get(REG_Y);
                    this->set(REG_AB, dst_addr & 0xFFFF);
                    pins = this->phi2_write(pins, REG_AB, this->get(REG_ABL));
                    
                    // Increment X and Y
                    this->inc(REG_X);
                    this->inc(REG_Y);
                    
                    // Decrement A (transfer count)
                    this->dec(REG_A);
                    
                    // Check if more bytes to transfer
                    if (this->get(REG_A) != 0xFFFF) {
                        // Continue transfer - go back to cycle 2
                        this->cycle_index = 2;
                    } else {
                        // Transfer complete
                        this->transition_to_fetch();
                    }
                }
                return pins;
        }
    }
    return pins;
}

// MVP - Move Positive (65C816)
bus_state_t op_mvp(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Read destination bank
                pins = this->phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Read source bank
                pins = this->phi2_read(pins, REG_PC, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Read from source address (bank:X)
                {
                    uint32_t src_addr = (this->get(REG_ABH) << 16) | this->get(REG_X);
                    this->set(REG_AB, src_addr & 0xFFFF);
                    pins = this->phi2_read(pins, REG_AB, REG_ABL);
                    if (FAM65XX_GET_RDY(pins)) {
                        this->cycle_index++;
                    }
                }
                return pins;
                
            case 3:
                // Write to destination address (bank:Y)
                if (this->should_complete_write_cycle(pins)) {
                    uint32_t dst_addr = (this->get(REG_DL) << 16) | this->get(REG_Y);
                    this->set(REG_AB, dst_addr & 0xFFFF);
                    pins = this->phi2_write(pins, REG_AB, this->get(REG_ABL));
                    
                    // Decrement X and Y (move in opposite direction from MVN)
                    this->dec(REG_X);
                    this->dec(REG_Y);
                    
                    // Decrement A (transfer count)
                    this->dec(REG_A);
                    
                    // Check if more bytes to transfer
                    if (this->get(REG_A) != 0xFFFF) {
                        // Continue transfer - go back to cycle 2
                        this->cycle_index = 2;
                    } else {
                        // Transfer complete
                        this->transition_to_fetch();
                    }
                }
                return pins;
        }
    }
    return pins;
}

// COP - Co-processor Instruction (65C816)
bus_state_t op_cop(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        switch (this->cycle_index) {
            case 0:
                // Read signature byte (ignored, but must be read for timing)
                pins = this->phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Push program bank register
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_PBR));
                    this->dec(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Push PC high byte
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_PCH));
                    this->dec(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 3:
                // Push PC low byte
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_PCL));
                    this->dec(REG_S);
                    this->cycle_index++;
                }
                return pins;
                
            case 4:
                // Push processor status register
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_SP, this->get(REG_P));
                    this->dec(REG_S);
                    // Set up interrupt type and vector address
                    this->active_interrupt = FAM65XX_INT_COP;
                    this->set(REG_AB, this->get_vector_addr());
                    this->cycle_index++;
                }
                return pins;
                
            case 5:
                // Read interrupt vector low byte
                pins = this->phi2_read(pins, REG_AB, REG_PCL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_AB);
                    this->cycle_index++;
                }
                return pins;
                
            case 6:
                // Read interrupt vector high byte
                pins = this->phi2_read(pins, REG_AB, REG_PCH);
                if (FAM65XX_GET_RDY(pins)) {
                    // Clear interrupt flag and disable interrupts
                    this->clear_flag(FLAG_D);  // Clear decimal mode
                    this->set_flag(FLAG_I);    // Disable interrupts
                    this->active_interrupt = FAM65XX_INT_NONE;
                    this->transition_to_fetch();
                }
                return pins;
        }
    }
    return pins;
}

// WDM - WDM Reserved Instruction (65C816)
bus_state_t op_wdm(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // Read and ignore operand byte for timing compatibility
        pins = this->phi2_read(pins, REG_PC, REG_DL);
        if (FAM65XX_GET_RDY(pins)) {
            this->inc(REG_PC);
            // WDM is essentially a 2-byte NOP - do nothing else
            this->transition_to_fetch();
        }
    }
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
