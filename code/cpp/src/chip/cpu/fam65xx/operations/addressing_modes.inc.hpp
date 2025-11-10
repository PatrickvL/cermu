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
    pins = this->phi2_read(pins, REG_PC, REG_ABL);
    if (FAM65XX_GET_RDY(pins)) {
        this->inc(REG_PC);
        
        // Set up zero page address (high byte is always 0)
        this->set(REG_ABH, 0x00);
        
        // Transition to operation
        this->transition_to_operation();
    }
    return pins;
}

// Zero Page,X addressing: $nn,X (cycle-accurate)
bus_state_t am_zpx(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            // PHI2: Read base address from PC
            pins = this->phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
                this->cycle_index++;
            }
            // PHI1: Base address is already stored in AB
            return pins;
            
        case 1:
            // PHI2: Dummy read from AB while adding index
            pins = this->phi2_read(pins, REG_AB, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Add index to ABL address (wraps in zero page)
                this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_X));
                this->transition_to_operation();
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
            pins = this->phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
                this->cycle_index++;
            }
            // PHI1: Base address is already stored in AB
            return pins;
            
        case 1:
            // PHI2: Dummy read from AB while adding index
            pins = this->phi2_read(pins, REG_AB, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Add index to AB address (wraps in zero page)
                this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_Y));
                this->transition_to_operation();
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
            pins = this->phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            // PHI2: Read high byte from PC
            pins = this->phi2_read(pins, REG_PC, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->transition_to_operation();
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
            pins = this->phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->cycle_index++;
            }
            return pins;
            
        case 1: {
            // PHI2: Read high byte from PC
            pins = this->phi2_read(pins, REG_PC, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                
                // CRITICAL: Store intermediate high byte in DL for illegal opcodes BEFORE any modifications
                this->set(REG_DL, this->get(REG_ABH));
                
                // PHI1: Calculate addresses
                uint16_t base = this->get(REG_AB);
                uint16_t effective = base + this->get(REG_X);
                
                // Add index to low byte (creates intermediate "wrong" address for page cross)
                // ABH stays unchanged, only ABL gets X added
                this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_X));
                
                // Check if penalty cycle is needed
                bool needs_penalty = this->page_crossed(base, effective) ||          // Page crossing
                                   (this->opcode_entry.flags & to_index(OF::RMW)) ||      // RMW operations
                                   (this->opcode_entry.flags & to_index(OF::ILLEGAL_STORE)) || // SHY illegal store
                                   !(this->opcode_entry.flags & to_index(OF::SKIP_PAGE)); // No skip allowed
                
                if (needs_penalty) {
                    // Page crossing, RMW, illegal store, or always need penalty
                    this->cycle_index++;
                } else {
                    // No penalty needed - complete with correct address
                    this->set(REG_AB, effective);
                    this->transition_to_operation();
                }
            }
            return pins;
        }
            
        case 2:
            // PHI2: Page cross penalty - read from wrong address (use TMP to avoid overwriting DL)
            pins = this->phi2_dummy_read(pins, REG_AB);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Correct final address
                // ABL has X added, ABH is unchanged from original. Subtract X from ABL to restore original base
                this->set(REG_ABL, this->get(REG_ABL) - this->get(REG_X));
                // Now AB has original base address, add X to full 16-bit AB for correct effective address with carry
                this->set(REG_AB, this->get(REG_AB) + this->get(REG_X));
                this->transition_to_operation();
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
            pins = this->phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->cycle_index++;
            }
            return pins;
            
        case 1: {
            // PHI2: Read high byte from PC
            pins = this->phi2_read(pins, REG_PC, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                
                // CRITICAL: Store intermediate high byte in DL for illegal opcodes BEFORE any modifications
                this->set(REG_DL, this->get(REG_ABH));
                
                // PHI1: Calculate addresses
                uint16_t base = this->get(REG_AB);
                uint16_t effective = base + this->get(REG_Y);
                
                // Add index to low byte (creates intermediate "wrong" address for page cross)
                this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_Y));
                
                // Check if penalty cycle is needed
                bool needs_penalty = this->page_crossed(base, effective) ||          // Page crossing
                                   (this->opcode_entry.flags & to_index(OF::RMW)) ||      // RMW operations
                                   (this->opcode_entry.flags & to_index(OF::ILLEGAL_STORE)) || // SHA illegal store
                                   !(this->opcode_entry.flags & to_index(OF::SKIP_PAGE)); // No skip allowed
                
                if (needs_penalty) {
                    // Page crossing, RMW, illegal store, or always need penalty
                    this->cycle_index++;
                } else {
                    // No penalty needed - complete with correct address
                    this->set(REG_AB, effective);
                    this->transition_to_operation();
                }
            }
            return pins;
        }
            
        case 2: {
            // PHI2: Page cross penalty - read from wrong address (use temporary register to avoid overwriting DL)
            pins = this->phi2_dummy_read(pins, REG_AB);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Correct final address
                // ABL has Y added, ABH is unchanged from original. Subtract Y from ABL to restore original base
                this->set(REG_ABL, this->get(REG_ABL) - this->get(REG_Y));
                // Now AB has original base address, add Y to full 16-bit AB for correct effective address with carry
                this->set(REG_AB, this->get(REG_AB) + this->get(REG_Y));
                this->transition_to_operation();
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
            pins = this->phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            // PHI2: Read high byte of pointer address from PC
            pins = this->phi2_read(pins, REG_PC, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            // PHI2: Read low byte of target address from pointer
            pins = this->phi2_read(pins, REG_AB, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Set up for high byte read with processor-specific behavior
                if constexpr (has_cmos()) {
                    // 65C02: Fixed page boundary behavior
                    this->inc(REG_AB);
                } else {
                    // 6502: Page boundary bug - increment only low byte
                    this->set(REG_ABL, this->get(REG_ABL) + 1);
                }
                this->cycle_index++;
            }
            return pins;
            
        case 3:
            // PHI2: Read high byte of target address
            pins = this->phi2_read(pins, REG_AB, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                // PHI1: Assemble final target address
                this->set(REG_ABL, this->get(REG_DL));  // Low byte from cycle 2
                // High byte already in ABH from this cycle
                this->transition_to_operation();
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
            pins = this->phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
                this->cycle_index++;
            }
            // PHI1: Base address is already stored in AB
            return pins;
            
        case 1:
            /* Dummy read from AB (before adding X) */
            pins = this->phi2_read(pins, REG_AB, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                /* Calculate AB+X during dummy cycle */
                this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_X));
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            /* Read low byte of target from AB+X */
            pins = this->phi2_read(pins, REG_AB, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                this->set(REG_ABL, this->get(REG_ABL) + 1);
                this->cycle_index++;
            }
            return pins;
            
        case 3:
            /* Read high byte of target from AB+X+1 */
            pins = this->phi2_read(pins, REG_AB, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                this->set(REG_ABL, this->get(REG_DL));  // Low byte from cycle 2
                this->transition_to_operation();
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
            pins = this->phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* Read low byte of target from ZP */
            pins = this->phi2_read(pins, REG_AB, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                this->set(REG_ABL, this->get(REG_ABL) + 1);  // Increment zero page pointer
                this->cycle_index++;
            }
            return pins;
            
        case 2: {
            /* Third cycle: Read high byte of the base address */
            pins = this->phi2_read(pins, REG_AB, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                /* Set up AB with base address */
                this->set(REG_ABL, this->get(REG_DL));  // Low byte from cycle 1
                // ABH already contains high byte from this cycle
                uint16_t base_addr = this->get(REG_AB);
                uint16_t final_addr = base_addr + this->get(REG_Y);
                
                /* Store intermediate high byte in DL for illegal opcodes AFTER setting up AB */
                this->set(REG_DL, this->get(REG_ABH));
                
                /* Add index to low byte only (creates intermediate "wrong" address for page cross) */
                this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_Y));
                
                /* Check if penalty cycle is needed */
                bool needs_penalty = this->page_crossed(base_addr, final_addr) ||      // Page crossing
                                   (this->opcode_entry.flags & to_index(OF::RMW)) ||        // RMW operations
                                   (this->opcode_entry.flags & to_index(OF::ILLEGAL_STORE)) || // SHA illegal store
                                   !(this->opcode_entry.flags & to_index(OF::SKIP_PAGE));   // Store operations and others that can't skip
                
                if (needs_penalty) {
                    /* Page crossing, RMW, or illegal store - need penalty cycle with intermediate address */
                    this->cycle_index++;
                } else {
                    /* No page cross, not RMW, and not illegal store - can skip penalty, set correct address */
                    this->set(REG_AB, final_addr);
                    this->transition_to_operation();
                }
            }
            return pins;
        }
            
        case 3:
            /* Page cross penalty - dummy read from wrong address */
            pins = this->phi2_dummy_read(pins, REG_AB);
            if (FAM65XX_GET_RDY(pins)) {
                /* DL contains intermediate high byte from case 2 */
                
                /* Correct final address calculation */
                /* Current AB has intermediate address: orig_high:(base_low + Y) */
                /* We need: (orig_high:(base_low)) + Y */
                this->set(REG_ABH, this->get(REG_DL));    /* Restore original high byte */
                this->set(REG_ABL, this->get(REG_ABL) - this->get(REG_Y));    /* Recover original base low */
                this->set(REG_AB, this->get(REG_AB) + this->get(REG_Y));     /* Calculate correct final with carry */
                
                this->transition_to_operation();
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
    // CRITICAL FIX: Remove constexpr conditional - all CMOS processors should support ZPI
    // The constexpr condition was preventing proper execution
    switch (this->cycle_index) {
        case 0:
            // PHI2: Read zero page address from PC
            pins = this->phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
                this->inc(REG_PC);
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            // PHI2: Read low byte of target address from zero page
            pins = this->phi2_read(pins, REG_AB, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                this->set(REG_ABL, this->get(REG_ABL) + 1); // Move to next zero page location
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            // PHI2: Read high byte of target address from zero page + 1
            pins = this->phi2_read(pins, REG_AB, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                this->set(REG_ABL, this->get(REG_DL));  // Low byte from cycle 1
                // PHI1: Address bus now contains final target address
                this->transition_to_operation();
            }
            return pins;
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
                pins = this->phi2_read(pins, REG_PC, REG_ABL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // PHI2: Read high byte of base address from PC
                pins = this->phi2_read(pins, REG_PC, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    // PHI1: Add X register to base address
                    this->set(REG_AB, this->get(REG_AB) + this->get(REG_X));
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // PHI2: Read low byte of target address from (base+X)
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    // PHI1: Set up for high byte read
                    this->inc(REG_AB);
                    this->cycle_index++;
                }
                return pins;
                
            case 3:
                // PHI2: Read high byte of target address from (base+X+1)
                pins = this->phi2_read(pins, REG_AB, REG_ABH);
                if (FAM65XX_GET_RDY(pins)) {
                    // PHI1: Assemble final target address
                    this->set(REG_ABL, this->get(REG_DL));  // Low byte from cycle 2
                    // High byte already in ABH from this cycle
                    // AB now contains the final jump target address
                    this->transition_to_operation();
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
        // Implementation would use this->get_d()
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
                pins = this->phi2_read(pins, REG_PC, REG_ABL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // PHI2: Read branch offset from PC (stored in DL for operation to use)
                pins = this->phi2_read(pins, REG_PC, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->inc(REG_PC);
                    // ZP address is in ZP register, branch offset is in DL
                    this->transition_to_operation();
                }
                return pins;
        }
    }
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
