/*
 * arithmetic.inc - Arithmetic Operations for MOS 65xx Family
 *
 * This file contains arithmetic operation implementations (ADC, SBC, CMP, etc.)
 * that are included within the fam65xx_t template class. These operations
 * use conditional compilation to handle processor-specific behaviors.
 */

// ============================================================================
// BCD ARITHMETIC HELPER FUNCTIONS (moved from main class)
// ============================================================================

// Add with Carry in BCD mode - Hardware-accurate 6502 BCD algorithm with constexpr ProcessorTag differentiation
uint8_t adc_bcd(uint8_t a, uint8_t b, bool carry_in, bool& carry_out, bool& overflow) {
    // Use hardware-accurate algorithm with processor-specific behavior
    uint8_t al = (a & 0x0F) + (b & 0x0F) + (carry_in ? 1 : 0);
    if (al > 9) {
        al += 6;
    }
    uint8_t ah = (a >> 4) + (b >> 4) + (al > 0x0F ? 1 : 0);
    
    // V flag calculation with processor-specific behavior using constexpr
    if constexpr (has_nmos_bugs<ProcessorTag>()) {
        // NMOS 6502: V flag uses intermediate result before final adjustment
        overflow = (~(a ^ b) & (a ^ (ah << 4)) & 0x80) != 0;
    } else {
        // CMOS variants: V flag behavior may be different
        overflow = (~(a ^ b) & (a ^ (ah << 4)) & 0x80) != 0;
    }
    
    // Processor-independent high nibble adjustment and carry
    if (ah > 9) {
        ah += 6;
    }
    carry_out = (ah > 15);
    
    // Final BCD result (processor-independent)
    return (ah << 4) | (al & 0x0F);
}

// BCD addition helper matching old implementation signature - now uses adc_bcd with constexpr differentiation
void bcd_addition_helper(uint8_t a_old, uint8_t operand, uint8_t carry_in, uint8_t* bcd_result, uint8_t* bcd_flags) {
    // Check BCD support per processor type using constexpr differentiation
    if constexpr (has_bcd<ProcessorTag>()) {
        // Hardware-accurate 6502 BCD addition (credit: MAME/floooh implementation)
        uint8_t al = (a_old & 0x0F) + (operand & 0x0F) + carry_in;
        if (al > 9) {
            al += 6;
        }
        uint8_t ah = (a_old >> 4) + (operand >> 4) + (al > 0x0F);
        
        // Clear all flags we're about to set
        *bcd_flags = 0;
        
        // Z flag: binary sum is zero (NMOS 6502 hardware behavior)
        uint8_t binary_result = (uint8_t)(a_old + operand + carry_in);
        if (binary_result == 0) {
            *bcd_flags |= FLAG_Z;
        }
        
        // N flag: bit 3 of high nibble intermediate result (NMOS 6502 BCD quirk)
        if (ah & 0x08) {
            *bcd_flags |= FLAG_N;
        }
        
        // V flag: signed overflow using intermediate result (ah<<4)
        if (~(a_old ^ operand) & (a_old ^ (ah << 4)) & 0x80) {
            *bcd_flags |= FLAG_V;
        }
        
        // High nibble adjustment and carry
        if (ah > 9) {
            ah += 6;
        }
        if (ah > 15) {
            *bcd_flags |= FLAG_C;
        }
        
        // Final BCD result
        *bcd_result = (ah << 4) | (al & 0x0F);
    } else {
        // No BCD support - should not be called, but provide fallback
        *bcd_result = a_old + operand + carry_in;
        *bcd_flags = 0;
    }
}

// Subtract with Borrow in BCD mode with constexpr ProcessorTag differentiation
uint8_t sbc_bcd(uint8_t a, uint8_t b, bool borrow_in, bool& carry_out, bool& overflow) {
    uint16_t al = (a & 0x0F) - (b & 0x0F) - (borrow_in ? 0 : 1);
    if (al & 0x10) al -= 0x06;
    
    uint16_t ah = (a >> 4) - (b >> 4) - ((al & 0x10) ? 1 : 0);
    if (ah & 0x10) ah -= 0x06;
    
    carry_out = !(ah & 0x10);
    
    // V flag behavior differs between NMOS and CMOS using constexpr differentiation
    if constexpr (has_nmos_bugs<ProcessorTag>()) {
        // NMOS: V flag reflects binary operation result
        uint16_t binary_result = a - b - (borrow_in ? 0 : 1);
        overflow = ((a ^ b) & (a ^ binary_result) & 0x80) != 0;
    } else {
        // CMOS: V flag undefined in BCD mode
        overflow = false;
    }
    
    return ((ah & 0x0F) << 4) | (al & 0x0F);
}

// BCD subtraction helper matching old implementation signature with constexpr differentiation
void bcd_subtraction_helper(uint8_t a_old, uint8_t operand, uint8_t borrow_in,
                           uint8_t* bcd_result, uint8_t* bcd_flags) {
    if constexpr (has_bcd<ProcessorTag>()) {
        // Hardware-accurate 6502 BCD subtraction (credit: MAME/floooh implementation)
        uint16_t diff = a_old - operand - borrow_in;
        uint8_t al = (a_old & 0x0F) - (operand & 0x0F) - borrow_in;
        if ((int8_t)al < 0) {
            al -= 6;
        }
        uint8_t ah = (a_old >> 4) - (operand >> 4) - ((int8_t)al < 0);
        
        // Clear all flags we're about to set
        *bcd_flags = 0;
        
        // Z flag: binary difference is zero
        if (0 == (uint8_t)diff) {
            *bcd_flags |= FLAG_Z;
        }
        // N flag: bit 7 of binary difference (different from addition!)
        if (diff & 0x80) {
            *bcd_flags |= FLAG_N;
        }
        
        // V flag: signed overflow on binary operation
        if ((a_old ^ operand) & (a_old ^ diff) & 0x80) {
            *bcd_flags |= FLAG_V;
        }
        
        // C flag: no borrow (result >= 0)
        if (!(diff & 0xFF00)) {
            *bcd_flags |= FLAG_C;
        }
        
        // High nibble adjustment
        if (ah & 0x80) {
            ah -= 6;
        }
        
        // Final BCD result
        *bcd_result = (ah << 4) | (al & 0x0F);
    } else {
        // No BCD support - should not be called, but provide fallback
        *bcd_result = a_old - operand - borrow_in;
        *bcd_flags = 0;
    }
}

// ============================================================================
// ADD WITH CARRY (ADC)
// ============================================================================

bus_state_t op_adc(bus_state_t pins) {
    // Read operand directly into DL register (optimized version)
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = CPU_DL(this);
        uint8_t a = CPU_A(this);
        uint8_t carry_in = this->get_carry_bit_0();
        
        // Handle decimal mode if supported (matching old implementation)
        if constexpr (has_bcd<ProcessorTag>()) {
            bool decimal_mode = (CPU_P(this) & FLAG_D) != 0;
            if (decimal_mode) {
                // BCD (Decimal) mode - use BCD addition helper with ProcessorTag differentiation
                uint8_t bcd_result;
                uint8_t bcd_flags;
                
                bcd_addition_helper(a, operand, carry_in, &bcd_result, &bcd_flags);
                
                CPU_A(this) = bcd_result;
                update_flags(FLAG_N | FLAG_V | FLAG_Z | FLAG_C, bcd_flags);
                // Complete instruction
                transition_to_fetch();
                return pins;
            }
            // No BCD support - treat as binary
        }
        
        // Binary mode addition (exact flag calculation matching old implementation)
        uint16_t result = a + operand + carry_in;
        CPU_A(this) = (uint8_t)result;
        
        // ADC modifies only N, V, Z, C flags - preserve all others exactly
        update_flags_adc(a, operand, result);
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// NO OPERATION (NOP)
// ============================================================================

bus_state_t op_nop(bus_state_t pins) {
    trace_enter("op_nop");
    trace_instruction(0xEA, "NOP");
    
    switch (this->opcode_entry.am_index) {
        case AM_IMM:
            /* AM_IMM: All immediate NOPs read operand and increment PC */
            trace("Immediate NOP: reading operand");
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
            } else {
                trace("RDY low - returning early");
                trace_exit("op_nop");
                return pins;
            }
            break;
            
        case AM_NON:
            /* AM_NON: All implicit NOPs do dummy read from PC without increment */
            trace("Implicit NOP: dummy read from PC");
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) {
                trace("RDY low - returning early");
                trace_exit("op_nop");
                return pins;
            }
            break;
            
        default:
            /* Memory modes: Read from target address and discard */
            trace("Memory mode NOP: reading from target address");
            pins = phi2_read(pins, REG_AB, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) {
                trace("RDY low - returning early");
                trace_exit("op_nop");
                return pins;
            }
            break;
    }
    
    /* Complete instruction */
    trace("NOP complete - transitioning to fetch");
    this->transition_to_fetch();
    
    trace_exit("op_nop");
    return pins;
}

// ============================================================================
// SUBTRACT WITH CARRY (SBC)
// ============================================================================

bus_state_t op_sbc(bus_state_t pins) {
    // Read operand directly into DL register (optimized version)
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = CPU_DL(this);
        uint8_t a = CPU_A(this);
        uint8_t borrow_in = this->get_borrow_input(); // Inverted carry for SBC
        
        // Handle decimal mode if supported (matching old implementation)
        if constexpr (has_bcd<ProcessorTag>()) {
            bool decimal_mode = (CPU_P(this) & FLAG_D) != 0;
            if (decimal_mode) {
                // BCD (Decimal) mode - use BCD subtraction helper with ProcessorTag differentiation
                uint8_t bcd_result;
                uint8_t bcd_flags;
                
                bcd_subtraction_helper(a, operand, borrow_in, &bcd_result, &bcd_flags);
                
                CPU_A(this) = bcd_result;
                update_flags(FLAG_N | FLAG_V | FLAG_Z | FLAG_C, bcd_flags);
                // Complete instruction
                transition_to_fetch();
                return pins;
            }
            // No BCD support - treat as binary
        }
        
        // Binary mode subtraction (exact flag calculation matching old implementation)
        uint16_t result = a - operand - borrow_in;
        CPU_A(this) = (uint8_t)result;
        
        // SBC modifies only N, V, Z, C flags - preserve all others exactly
        update_flags_sbc(a, operand, result);
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// COMPARE ACCUMULATOR (CMP)
// ============================================================================

bus_state_t op_cmp(bus_state_t pins) {
    // Read operand directly into DL register (optimized version)
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = CPU_DL(this);
        uint8_t a = CPU_A(this);
        uint16_t result = a - operand;
        
        // Update flags based on comparison
        update_nzc_flags(a, operand);
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// COMPARE X REGISTER (CPX)
// ============================================================================

bus_state_t op_cpx(bus_state_t pins) {
    // Read operand directly into DL register (optimized version)
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = CPU_DL(this);
        uint8_t x = CPU_X(this);
        uint16_t result = x - operand;
        
        // Update flags based on comparison
        update_nzc_flags(x, operand);
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// COMPARE Y REGISTER (CPY)
// ============================================================================

bus_state_t op_cpy(bus_state_t pins) {
    // Read operand directly into DL register (optimized version)
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = CPU_DL(this);
        uint8_t y = CPU_Y(this);
        uint16_t result = y - operand;
        
        // Update flags based on comparison
        update_nzc_flags(y, operand);
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// INCREMENT MEMORY (INC)
// ============================================================================

bus_state_t op_inc(bus_state_t pins) {
    // INC - Increment memory by 1
    // This is a Read-Modify-Write operation
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Increment the value
        value++;
        // Update N and Z flags
        this->update_nz_flags(value);
    });
}

// ============================================================================
// DECREMENT MEMORY (DEC)
// ============================================================================

bus_state_t op_dec(bus_state_t pins) {
    // DEC - Decrement memory by 1
    // This is a Read-Modify-Write operation
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Decrement the value
        value--;
        // Update N and Z flags
        this->update_nz_flags(value);
    });
}

// ============================================================================
// 16-BIT ARITHMETIC (65C816 only)
// ============================================================================

bus_state_t op_adc_16bit(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // 16-bit ADC implementation for 65C816
        // This would be a more complex implementation
        // For now, delegate to 8-bit version
        return op_adc(pins);
    } else {
        // Should not be called on processors without wide registers
        return pins;
    }
}

bus_state_t op_sbc_16bit(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // 16-bit SBC implementation for 65C816
        return op_sbc(pins);
    } else {
        return pins;
    }
}
