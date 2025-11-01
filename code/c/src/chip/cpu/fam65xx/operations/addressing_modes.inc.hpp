/*
 * addressing_modes.inc.hpp - Addressing Mode Handlers for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// ADDRESSING MODE HANDLERS
// ============================================================================

// Zero Page addressing: $nn (cycle-accurate)
bus_state_t am_zp(bus_state_t pins) {
    // PHI2: Read zero page address from PC
    pins = phi2_read(pins, REG_PC, REG_ABL);
    if (FAM65XX_GET_RDY(pins)) {
        CPU_PC(this)++;
        
        // Set up zero page address (high byte is always 0)
        CPU_ABH(this) = 0x00;
        
        // Transition to operation
        transition_to_operation();
    }
    return pins;
}

// Zero Page,X addressing: $nn,X (cycle-accurate)
bus_state_t am_zpx(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            // PHI2: Read base address from PC
            pins = phi2_read(pins, REG_PC, REG_ZPL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                this->cycle_index++;
            }
            // PHI1: Base address is already stored in ZP
            return pins;
            
        case 1:
            // PHI2: Dummy read from ZP while adding index
            pins = phi2_read(pins, REG_ZP, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Add index to ZP address (wraps in zero page), then copy to AB
                CPU_ZPL(this) += CPU_X(this);
                CPU_AB(this) = CPU_ZP(this); // Copy final ZP address to AB
                transition_to_operation();
            }
            return pins;
    }
    return pins;
}

// Zero Page,Y addressing: $nn,Y (cycle-accurate)
bus_state_t am_zpy(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            // PHI2: Read base address from PC
            pins = phi2_read(pins, REG_PC, REG_ZPL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                this->cycle_index++;
            }
            // PHI1: Base address is already stored in ZP
            return pins;
            
        case 1:
            // PHI2: Dummy read from ZP while adding index
            pins = phi2_read(pins, REG_ZP, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Add index to ZP address (wraps in zero page), then copy to AB
                CPU_ZPL(this) += CPU_Y(this);
                CPU_AB(this) = CPU_ZP(this); // Copy final ZP address to AB
                transition_to_operation();
            }
            return pins;
    }
    return pins;
}

// Absolute addressing: $nnnn (cycle-accurate)
bus_state_t am_abs(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            // PHI2: Read low byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            // PHI2: Read high byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                transition_to_operation();
            }
            return pins;
    }
    return pins;
}

// Absolute,X addressing: $nnnn,X (cycle-accurate with page crossing)
bus_state_t am_abx(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            // PHI2: Read low byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                this->cycle_index++;
            }
            return pins;
            
        case 1: {
            // PHI2: Read high byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                
                // CRITICAL: Store intermediate high byte in DL for illegal opcodes BEFORE any modifications
                CPU_DL(this) = CPU_ABH(this);
                
                // PHI1: Calculate addresses
                uint16_t base = CPU_AB(this);
                uint16_t effective = base + CPU_X(this);
                
                // Add index to low byte (creates intermediate "wrong" address for page cross)
                // ABH stays unchanged, only ABL gets X added
                CPU_ABL(this) += CPU_X(this);
                
                // Check if penalty cycle is needed
                bool needs_penalty = page_crossed(base, effective) ||          // Page crossing
                                   (this->opcode_entry.flags & OF_RMW) ||      // RMW operations
                                   (this->opcode_entry.flags & OF_ILLEGAL_STORE) || // SHY illegal store
                                   !(this->opcode_entry.flags & OF_SKIP_PAGE); // No skip allowed
                
                if (needs_penalty) {
                    // Page crossing, RMW, illegal store, or always need penalty
                    this->cycle_index++;
                } else {
                    // No penalty needed - complete with correct address
                    CPU_AB(this) = effective;
                    transition_to_operation();
                }
            }
            return pins;
        }
            
        case 2:
            // PHI2: Page cross penalty - read from wrong address (use TMP to avoid overwriting DL)
            pins = phi2_read(pins, REG_AB, REG_TMP);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Correct final address
                // ABL has X added, ABH is unchanged from original. Subtract X from ABL to restore original base
                CPU_ABL(this) -= CPU_X(this);
                // Now AB has original base address, add X to full 16-bit AB for correct effective address with carry
                CPU_AB(this) += CPU_X(this);
                transition_to_operation();
            }
            return pins;
    }
    return pins;
}

// Absolute,Y addressing: $nnnn,Y (cycle-accurate with page crossing)
bus_state_t am_aby(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            // PHI2: Read low byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                this->cycle_index++;
            }
            return pins;
            
        case 1: {
            // PHI2: Read high byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                
                // CRITICAL: Store intermediate high byte in DL for illegal opcodes BEFORE any modifications
                CPU_DL(this) = CPU_ABH(this);
                
                // PHI1: Calculate addresses
                uint16_t base = CPU_AB(this);
                uint16_t effective = base + CPU_Y(this);
                
                // Add index to low byte (creates intermediate "wrong" address for page cross)
                CPU_ABL(this) += CPU_Y(this);
                
                // Check if penalty cycle is needed
                bool needs_penalty = page_crossed(base, effective) ||          // Page crossing
                                   (this->opcode_entry.flags & OF_RMW) ||      // RMW operations
                                   (this->opcode_entry.flags & OF_ILLEGAL_STORE) || // SHA illegal store
                                   !(this->opcode_entry.flags & OF_SKIP_PAGE); // No skip allowed
                
                if (needs_penalty) {
                    // Page crossing, RMW, illegal store, or always need penalty
                    this->cycle_index++;
                } else {
                    // No penalty needed - complete with correct address
                    CPU_AB(this) = effective;
                    transition_to_operation();
                }
            }
            return pins;
        }
            
        case 2: {
            // PHI2: Page cross penalty - read from wrong address (use temporary register to avoid overwriting DL)
            pins = phi2_read(pins, REG_AB, REG_TMP); // Use TMP as temporary since DL has intermediate high byte
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Correct final address
                // ABL has Y added, ABH is unchanged from original. Subtract Y from ABL to restore original base
                CPU_ABL(this) -= CPU_Y(this);
                // Now AB has original base address, add Y to full 16-bit AB for correct effective address with carry
                CPU_AB(this) += CPU_Y(this);
                transition_to_operation();
            }
            return pins;
        }
    }
    return pins;
}

// Indirect addressing: ($nnnn) - Used only by JMP instruction
bus_state_t am_ind(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            // PHI2: Read low byte of pointer address from PC
            pins = phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            // PHI2: Read high byte of pointer address from PC
            pins = phi2_read(pins, REG_PC, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            // PHI2: Read low byte of target address from pointer
            pins = phi2_read(pins, REG_AB, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Set up for high byte read with processor-specific behavior
                if constexpr (has_cmos()) {
                    // 65C02: Fixed page boundary behavior
                    CPU_AB(this)++;
                } else {
                    // 6502: Page boundary bug - increment only low byte
                    CPU_ABL(this)++;
                }
                this->cycle_index++;
            }
            return pins;
            
        case 3:
            // PHI2: Read high byte of target address
            pins = phi2_read(pins, REG_AB, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Assemble final target address
                CPU_ABL(this) = CPU_DL(this);  // Low byte from cycle 2
                // High byte already in ABH from this cycle
                transition_to_operation();
            }
            return pins;
    }
    return pins;
}

// Indexed Indirect addressing: ($nn,X)
bus_state_t am_inx(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* Read pointer from PC */
            pins = phi2_read(pins, REG_PC, REG_ZPL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                this->cycle_index++;
            }
            // PHI1: Base address is already stored in ZP
            return pins;
            
        case 1:
            /* Dummy read from ZP (before adding X) */
            pins = phi2_read(pins, REG_ZP, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                /* Calculate ZP+X during dummy cycle */
                CPU_ZPL(this) += CPU_X(this);
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            /* Read low byte of target from ZP+X */
            pins = phi2_read(pins, REG_ZP, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_ZPL(this)++;
                this->cycle_index++;
            }
            return pins;
            
        case 3:
            /* Read high byte of target from ZP+X+1 */
            pins = phi2_read(pins, REG_ZP, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                transition_to_operation();
            }
            return pins;
    }
    return pins;
}

// Indirect Indexed addressing: ($nn),Y
bus_state_t am_iny(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* Read pointer from PC */
            pins = phi2_read(pins, REG_PC, REG_ZPL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* Read low byte of target from ZP */
            pins = phi2_read(pins, REG_ZP, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_ZPL(this)++;
                this->cycle_index++;
            }
            return pins;
            
        case 2: {
            /* Third cycle: Read high byte of the base address */
            pins = phi2_read(pins, REG_ZP, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                /* Store the intermediate high byte in DL for illegal opcodes */
                CPU_DL(this) = CPU_ABH(this);
                
                /* Add Y register with page crossing check */
                uint16_t base_addr = CPU_AB(this);
                uint16_t final_addr = base_addr + CPU_Y(this);
                
                /* Add index to low byte only (creates intermediate "wrong" address for page cross) */
                /* ABH stays unchanged, only ABL gets Y added */
                CPU_ABL(this) += CPU_Y(this);
                
                /* Check if penalty cycle is needed */
                bool needs_penalty = page_crossed(base_addr, final_addr) ||      // Page crossing
                                   (this->opcode_entry.flags & OF_RMW) ||        // RMW operations
                                   (this->opcode_entry.flags & OF_ILLEGAL_STORE) || // SHA illegal store
                                   !(this->opcode_entry.flags & OF_SKIP_PAGE);   // Store operations and others that can't skip
                
                if (needs_penalty) {
                    /* Page crossing, RMW, or illegal store - need penalty cycle with intermediate address */
                    this->cycle_index++;
                } else {
                    /* No page cross, not RMW, and not illegal store - can skip penalty, set correct address */
                    CPU_AB(this) = final_addr;
                    transition_to_operation();
                }
            }
            return pins;
        }
            
        case 3:
            /* Page cross penalty - dummy read from wrong address (don't overwrite DL!) */
            pins = phi2_read(pins, REG_AB, REG_TMP); // Use TMP as temporary, preserve DL
            if (FAM65XX_GET_RDY(pins)) {
                /* DL already contains intermediate high byte from case 2 */
                
                /* Correct final address using reference implementation approach */
                /* Current AB has intermediate address: wrong_high:(base_low + Y) */
                /* We need: (base_high:(base_low)) + Y */
                CPU_ABH(this) = CPU_DL(this);  /* Restore original high byte */
                CPU_ABL(this) -= CPU_Y(this);  /* Recover original base low */
                CPU_AB(this) += CPU_Y(this);   /* Calculate correct final */
                
                transition_to_operation();
            }
            return pins;
    }
    return pins;
}

// 65C02 Enhanced Addressing Modes (conditional compilation)

// Absolute Indirect addressing: ($nnnn) - JMP/JSR for 65C02
bus_state_t addr_ind_abs(bus_state_t pins) {
    if constexpr (has_cmos()) {
        // This addressing mode only exists on 65C02+
        // Use the same implementation as regular indirect addressing
        return am_ind(pins);
    } else {
        // Invalid on NMOS processors
        return pins; // Should not be called
    }
}

// Zero Page Indirect addressing: ($nn) - 65C02 only
bus_state_t am_zpi(bus_state_t pins) {
    if constexpr (has_cmos()) {
        // 65C02 zero page indirect addressing: ($nn)
        switch (this->cycle_index) {
            case 0:
                // PHI2: Read zero page address from PC
                pins = phi2_read(pins, REG_PC, REG_ZPL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // PHI2: Read low byte of target address from zero page
                pins = phi2_read(pins, REG_ZP, REG_ABL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_ZPL(this)++; // Move to next zero page location
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // PHI2: Read high byte of target address from zero page + 1
                pins = phi2_read(pins, REG_ZP, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    // PHI1: Address bus now contains final target address
                    transition_to_operation();
                }
                return pins;
        }
    }
    return pins;
}

// Absolute Indexed Indirect addressing: ($nnnn,X) - 65C02 JMP only
bus_state_t am_abi(bus_state_t pins) {
    if constexpr (has_cmos()) {
        // WDC 65C02 absolute indexed indirect: JMP (abs,X)
        switch (this->cycle_index) {
            case 0:
                // PHI2: Read low byte of base address from PC
                pins = phi2_read(pins, REG_PC, REG_ABL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // PHI2: Read high byte of base address from PC
                pins = phi2_read(pins, REG_PC, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    // PHI1: Add X register to base address
                    CPU_AB(this) += CPU_X(this);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // PHI2: Read low byte of target address from (base+X)
                pins = phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    // PHI1: Set up for high byte read
                    CPU_AB(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 3:
                // PHI2: Read high byte of target address from (base+X+1)
                pins = phi2_read(pins, REG_AB, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    // PHI1: Assemble final target address
                    CPU_ABL(this) = CPU_DL(this);  // Low byte from cycle 2
                    // High byte already in ABH from this cycle
                    // AB now contains the final jump target address
                    transition_to_operation();
                }
                return pins;
        }
    }
    return pins;
}

// 65C816 Enhanced Addressing Modes (conditional compilation)

// Direct Page addressing: dp (65C816)
bus_state_t am_dp(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // Use Direct Page register instead of zero page
        // Implementation would use this->wide_state.D
        return pins; // Placeholder
    } else {
        // Fall back to zero page on older processors
        return am_zp(pins);
    }
}

// Direct Page,X addressing: dp,X (65C816)
bus_state_t am_dpx(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // Use Direct Page register with X indexing
        return pins; // Placeholder
    } else {
        return am_zpx(pins);
    }
}

// Long addressing: $nnnnnn (65C816)
bus_state_t addr_long(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // 24-bit addressing using bank registers
        return pins; // Placeholder
    } else {
        return pins; // Should not be called
    }
}

// Long,X addressing: $nnnnnn,X (65C816)
bus_state_t addr_long_x(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // 24-bit addressing with X indexing
        return pins; // Placeholder
    } else {
        return pins; // Should not be called
    }
}

// Stack Relative addressing: sr,S (65C816)
bus_state_t amr_sr(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // Stack relative addressing
        return pins; // Placeholder
    } else {
        return pins; // Should not be called
    }
}

// Stack Relative Indirect Indexed: (sr,S),Y (65C816)
bus_state_t am_sri(bus_state_t pins) {
    if constexpr (has_wide_registers()) {
        // Complex 65C816 addressing mode
        return pins; // Placeholder
    } else {
        return pins; // Should not be called
    }
}

// Zero Page Relative Addressing: For BBR/BBS instructions ($nn,$offset)
bus_state_t am_zpr(bus_state_t pins) {
    if constexpr (Traits.has(CPUCoreFlags::ROCKWELL_BITS)) {
        // BBR/BBS instructions: $nn,$offset
        switch (this->cycle_index) {
            case 0:
                // PHI2: Read zero page address from PC
                pins = phi2_read(pins, REG_PC, REG_ZPL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // PHI2: Read branch offset from PC (stored in DL for operation to use)
                pins = phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    CPU_PC(this)++;
                    // ZP address is in ZP register, branch offset is in DL
                    transition_to_operation();
                }
                return pins;
        }
    }
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
