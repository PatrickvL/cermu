/*
 * addressing_modes.inc - Addressing Mode Handlers for MOS 65xx Family
 *
 * This file contains all addressing mode handler implementations that are
 * included within the fam65xx_t template class. These handlers prepare
 * addresses for CPU operations and are called before operation handlers.
 *
 * DESIGN: Each handler follows the pin-accurate bus interface pattern,
 * taking and returning bus_state_t pins parameter. Conditional compilation
 * based on ProcessorTag enables processor-specific optimizations.
 */

// ============================================================================
// ADDRESSING MODE HANDLERS
// ============================================================================

// Zero Page addressing: $nn (cycle-accurate)
bus_state_t addr_zp(bus_state_t pins) {
    // PHI2: Read zero page address from PC
    pins = phi2_read(pins, REG_PC, REG_ABL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    CPU_PC(this)++;
    
    // Set up zero page address (high byte is always 0)
    CPU_ABH(this) = 0x00;
    
    // Transition to operation
    transition_to_operation();
    return pins;
}

// Zero Page,X addressing: $nn,X (cycle-accurate)
bus_state_t addr_zpx(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            // PHI2: Read base address from PC
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            
            // PHI1: Store base address in ZP
            CPU_ZPL(this) = CPU_DL(this);
            CPU_ZPH(this) = 0x00;
            break;
            
        case 1:
            // PHI2: Dummy read from ZP while adding index
            pins = phi2_read(pins, REG_ZP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            // PHI1: Add index to ZP address (wraps in zero page), then copy to AB
            CPU_ZPL(this) = (CPU_ZPL(this) + CPU_X(this)) & 0xFF;
            CPU_AB(this) = CPU_ZP(this); // Copy final ZP address to AB
            transition_to_operation();
            break;
    }
    return pins;
}

// Zero Page,Y addressing: $nn,Y (cycle-accurate)
bus_state_t addr_zpy(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            // PHI2: Read base address from PC
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            
            // PHI1: Store base address in ZP
            CPU_ZPL(this) = CPU_DL(this);
            CPU_ZPH(this) = 0x00;
            break;
            
        case 1:
            // PHI2: Dummy read from ZP while adding index
            pins = phi2_read(pins, REG_ZP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            // PHI1: Add index to ZP address (wraps in zero page), then copy to AB
            CPU_ZPL(this) = (CPU_ZPL(this) + CPU_Y(this)) & 0xFF;
            CPU_AB(this) = CPU_ZP(this); // Copy final ZP address to AB
            transition_to_operation();
            break;
    }
    return pins;
}

// Absolute addressing: $nnnn (cycle-accurate)
bus_state_t addr_abs(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            // PHI2: Read low byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            break;
            
        case 1:
            // PHI2: Read high byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            transition_to_operation();
            break;
    }
    return pins;
}

// Absolute,X addressing: $nnnn,X (cycle-accurate with page crossing)
bus_state_t addr_abx(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            // PHI2: Read low byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            break;
            
        case 1: {
            // PHI2: Read high byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            
            // PHI1: Calculate addresses
            uint16_t base = CPU_AB(this);
            uint16_t effective = base + CPU_X(this);
            
            // Store original high byte in DL for page cross correction
            CPU_DL(this) = CPU_ABH(this);
            
            // Add index to low byte (creates intermediate "wrong" address for page cross)
            CPU_ABL(this) += CPU_X(this);
            
            // Skip penalty cycle if allowed and no page cross occurred
            if (this->opcode_entry.can_skip_page_cross && ((base ^ effective) & 0xFF00) == 0) {
                CPU_AB(this) = effective;  // Fix address
                transition_to_operation();
            }
            // Otherwise continue to cycle 2 with intermediate address
            break;
        }
            
        case 2:
            // PHI2: Page cross penalty - read from intermediate address
            pins = phi2_read(pins, REG_AB, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            // PHI1: Correct final address
            CPU_ABH(this) = CPU_DL(this);  // Restore original high byte
            CPU_ABL(this) -= CPU_X(this);  // Restore original low byte
            CPU_AB(this) += CPU_X(this);   // Correctly calculate final address
            transition_to_operation();
            break;
    }
    return pins;
}

// Absolute,Y addressing: $nnnn,Y (cycle-accurate with page crossing)
bus_state_t addr_aby(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            // PHI2: Read low byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            break;
            
        case 1: {
            // PHI2: Read high byte from PC
            pins = phi2_read(pins, REG_PC, REG_ABH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            
            // PHI1: Calculate addresses
            uint16_t base = CPU_AB(this);
            uint16_t effective = base + CPU_Y(this);
            
            // Store original high byte in DL for page cross correction
            CPU_DL(this) = CPU_ABH(this);
            
            // Add index to low byte (creates intermediate "wrong" address for page cross)
            CPU_ABL(this) += CPU_Y(this);
            
            // Skip penalty cycle if allowed and no page cross occurred
            if (this->opcode_entry.can_skip_page_cross && ((base ^ effective) & 0xFF00) == 0) {
                CPU_AB(this) = effective;  // Fix address
                transition_to_operation();
            }
            // Otherwise continue to cycle 2 with intermediate address
            break;
        }
            
        case 2:
            // PHI2: Page cross penalty - read from intermediate address
            pins = phi2_read(pins, REG_AB, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            // PHI1: Correct final address
            CPU_ABH(this) = CPU_DL(this);  // Restore original high byte
            CPU_ABL(this) -= CPU_Y(this);  // Restore original low byte
            CPU_AB(this) += CPU_Y(this);   // Correctly calculate final address
            transition_to_operation();
            break;
    }
    return pins;
}

// Indirect addressing: ($nnnn) - JMP only
bus_state_t addr_ind(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            /* Read low byte of pointer address from PC */
            pins = phi2_read(pins, REG_PC, REG_ABL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            break;
            
        case 1:
            /* Read high byte of pointer address from PC */
            pins = phi2_read(pins, REG_PC, REG_ABH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            break;
            
        case 2:
            /* Read low byte of target address from (pointer) */
            pins = phi2_read(pins, REG_AB, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* Store target low byte and prepare for bug handling */
            CPU_ABL(this)++; // Increment for high byte read
            break;
            
        case 3:
            /* Read high byte of target address */
            pins = phi2_read(pins, REG_AB, REG_ABH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* Assemble final target address */
            CPU_ABL(this) = CPU_DL(this);
            // ABH already contains the high byte from the read
            break;
    }
    return pins;
}

// Indexed Indirect addressing: ($nn,X)
bus_state_t addr_inx(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            /* Read pointer from PC */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            
            /* Store pointer in zero page */
            CPU_ZPL(this) = CPU_DL(this);
            CPU_ZPH(this) = 0x00;
            break;
            
        case 1:
            /* Dummy read from ZP (before adding X) */
            pins = phi2_read(pins, REG_ZP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* Calculate ZP+X during dummy cycle */
            CPU_ZPL(this) += CPU_X(this);
            break;
            
        case 2:
            /* Read low byte of target from ZP+X */
            pins = phi2_read(pins, REG_ZP, REG_ABL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_ZPL(this)++;
            break;
            
        case 3:
            /* Read high byte of target from ZP+X+1 */
            pins = phi2_read(pins, REG_ZP, REG_ABH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            break;
    }
    return pins;
}

// Indirect Indexed addressing: ($nn),Y
bus_state_t addr_iny(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            /* Read pointer from PC */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            
            /* Store pointer in zero page */
            CPU_ZPL(this) = CPU_DL(this);
            CPU_ZPH(this) = 0x00;
            break;
            
        case 1:
            /* Read low byte of target from ZP */
            pins = phi2_read(pins, REG_ZP, REG_ABL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_ZPL(this)++;
            break;
            
        case 2: {
            /* Third cycle: Read high byte of the base address */
            pins = phi2_read(pins, REG_ZP, REG_ABH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* Add Y register with page crossing check */
            uint16_t base_addr = CPU_AB(this);
            uint16_t final_addr = base_addr + CPU_Y(this);
            CPU_AB(this) = final_addr;
            
            /* Check for page crossing penalty */
            if (page_crossed(base_addr, final_addr)) {
                /* Page crossing - need penalty cycle */
                break;
            } else {
                /* No page cross - complete addressing mode */
                return pins;
            }
        }
            
        case 3:
            /* Page cross penalty - dummy read from wrong address, then fix */
            pins = phi2_read(pins, REG_AB, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            /* Address is already correct from case 2 */
            break;
    }
    return pins;
}

// 65C02 Enhanced Addressing Modes (conditional compilation)

// Absolute Indirect addressing: ($nnnn) - JMP/JSR for 65C02
bus_state_t addr_ind_abs(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // This addressing mode only exists on 65C02+
        // Use the same implementation as regular indirect addressing
        return addr_ind(pins);
    } else {
        // Invalid on NMOS processors
        return pins; // Should not be called
    }
}

// Zero Page Indirect addressing: ($nn) - 65C02 only
bus_state_t addr_zp_ind(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // 65C02 zero page indirect addressing
        // Implementation would go here
        return pins; // Placeholder
    } else {
        return pins; // Should not be called
    }
}

// Absolute Indexed Indirect addressing: ($nnnn,X) - 65C02 JMP only
bus_state_t addr_abs_inx(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // 65C02 absolute indexed indirect
        return pins; // Placeholder
    } else {
        return pins; // Should not be called
    }
}

// 65C816 Enhanced Addressing Modes (conditional compilation)

// Direct Page addressing: dp (65C816)
bus_state_t addr_dp(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Use Direct Page register instead of zero page
        // Implementation would use this->wide_state.D
        return pins; // Placeholder
    } else {
        // Fall back to zero page on older processors
        return addr_zp(pins);
    }
}

// Direct Page,X addressing: dp,X (65C816)
bus_state_t addr_dpx(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Use Direct Page register with X indexing
        return pins; // Placeholder
    } else {
        return addr_zpx(pins);
    }
}

// Long addressing: $nnnnnn (65C816)
bus_state_t addr_long(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // 24-bit addressing using bank registers
        return pins; // Placeholder
    } else {
        return pins; // Should not be called
    }
}

// Long,X addressing: $nnnnnn,X (65C816)
bus_state_t addr_long_x(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // 24-bit addressing with X indexing
        return pins; // Placeholder
    } else {
        return pins; // Should not be called
    }
}

// Stack Relative addressing: sr,S (65C816)
bus_state_t addr_stack_rel(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Stack relative addressing
        return pins; // Placeholder
    } else {
        return pins; // Should not be called
    }
}

// Stack Relative Indirect Indexed: (sr,S),Y (65C816)
bus_state_t addr_stack_rel_iny(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Complex 65C816 addressing mode
        return pins; // Placeholder
    } else {
        return pins; // Should not be called
    }
}

// Relative Addressing: For branch instructions
bus_state_t addr_rel(bus_state_t pins) {
    // Relative addressing doesn't pre-calculate address
    // Branch operations handle their own relative address calculation
    // Move directly to the operation
    transition_to_operation();
    return pins;
}


