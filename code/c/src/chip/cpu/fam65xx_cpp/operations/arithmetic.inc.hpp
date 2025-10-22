/*
 * arithmetic.inc - Arithmetic Operations for MOS 65xx Family
 *
 * This file contains arithmetic operation implementations (ADC, SBC, CMP, etc.)
 * that are included within the fam65xx_t template class. These operations
 * use conditional compilation to handle processor-specific behaviors.
 */

// ============================================================================
// ADD WITH CARRY (ADC)
// ============================================================================

bus_state_t op_adc(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    uint8_t operand = CPU_DL(this);
    uint8_t a = CPU_A(this);
    bool carry_in = (CPU_P(this) & FLAG_C) != 0;
    bool decimal_mode = (CPU_P(this) & FLAG_D) != 0;
    
    uint16_t result;
    bool carry_out, overflow;
    
    // Handle decimal mode if supported (matching old implementation)
    if (decimal_mode) {
        if constexpr (has_bcd<ProcessorTag>()) {
            // BCD (Decimal) mode - use unified BCD addition helper
            uint8_t bcd_result;
            uint8_t bcd_flags;
            
            this->bcd_addition_helper(a, operand, carry_in ? 1 : 0, &bcd_result, &bcd_flags);
            
            CPU_A(this) = bcd_result;
            CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) | bcd_flags;
        } else {
            // No BCD support - treat as binary
            goto binary_adc;
        }
    } else {
    binary_adc:
        // Binary mode addition (matching old implementation flag calculation)
        result = a + operand + (carry_in ? 1 : 0);
        CPU_A(this) = result & 0xFF;
        
        // ADC modifies only N, V, Z, C flags - preserve all others exactly (matching old impl)
        CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                      (CPU_A(this) & FLAG_N) |                                    // N = bit 7 of result
                      (CPU_A(this) == 0 ? FLAG_Z : 0) |                          // Z = result is zero
                      (result > 0xFF ? FLAG_C : 0) |                            // C = carry out
                      (((a ^ result) & (operand ^ result) & 0x80) ? FLAG_V : 0); // V = overflow
    }
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// ============================================================================
// NO OPERATION (NOP)
// ============================================================================

bus_state_t op_nop(bus_state_t pins) {
    // NOP does nothing except consume cycles
    transition_to_fetch();
    return pins;
}
}

// ============================================================================
// SUBTRACT WITH CARRY (SBC)
// ============================================================================

bus_state_t op_sbc(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    uint8_t operand = CPU_DL(this);
    uint8_t a = CPU_A(this);
    bool borrow_in = (CPU_P(this) & FLAG_C) == 0; // Inverted carry for SBC
    bool decimal_mode = (CPU_P(this) & FLAG_D) != 0;
    
    uint16_t result;
    bool carry_out, overflow;
    
    // Handle decimal mode if supported
    if (decimal_mode && has_bcd<ProcessorTag>()) {
        if constexpr (has_bcd<ProcessorTag>()) {
            CPU_A(this) = this->sbc_bcd(a, operand, borrow_in, carry_out, overflow);
        } else {
            // Fallback to binary if BCD not supported
            goto binary_sbc;
        }
    } else {
    binary_sbc:
        // Binary mode subtraction
        result = a - operand - (borrow_in ? 1 : 0);
        carry_out = (result < 0x100); // Inverted for SBC
        overflow = ((a ^ operand) & (a ^ result) & 0x80) != 0;
        CPU_A(this) = result & 0xFF;
    }
    
    // Update flags
    CPU_P(this) = (CPU_P(this) & 0x3C) |  // Clear N,V,Z,C
                  (CPU_A(this) & 0x80) |   // N flag
                  (overflow ? FLAG_V : 0) | // V flag  
                  (CPU_A(this) == 0 ? FLAG_Z : 0) | // Z flag
                  (carry_out ? FLAG_C : 0); // C flag
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// ============================================================================
// COMPARE ACCUMULATOR (CMP)
// ============================================================================

bus_state_t op_cmp(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    uint8_t operand = CPU_DL(this);
    uint8_t a = CPU_A(this);
    uint16_t result = a - operand;
    
    // Update flags based on comparison
    CPU_P(this) = (CPU_P(this) & 0x7C) |  // Clear N,Z,C (preserve V)
                  (result & 0x80) |        // N flag
                  ((result & 0xFF) == 0 ? FLAG_Z : 0) | // Z flag
                  (a >= operand ? FLAG_C : 0); // C flag (set if no borrow)
    
    // Complete instruction
    fam65xx_transition_to_fetch(this);
    return pins;
}

// ============================================================================
// COMPARE X REGISTER (CPX)
// ============================================================================

bus_state_t op_cpx(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    uint8_t operand = CPU_DL(this);
    uint8_t x = CPU_X(this);
    uint16_t result = x - operand;
    
    // Update flags based on comparison
    CPU_P(this) = (CPU_P(this) & 0x7C) |  // Clear N,Z,C (preserve V)
                  (result & 0x80) |        // N flag
                  ((result & 0xFF) == 0 ? FLAG_Z : 0) | // Z flag
                  (x >= operand ? FLAG_C : 0); // C flag
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// ============================================================================
// COMPARE Y REGISTER (CPY)
// ============================================================================

bus_state_t op_cpy(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    uint8_t operand = CPU_DL(this);
    uint8_t y = CPU_Y(this);
    uint16_t result = y - operand;
    
    // Update flags based on comparison
    CPU_P(this) = (CPU_P(this) & 0x7C) |  // Clear N,Z,C (preserve V)
                  (result & 0x80) |        // N flag
                  ((result & 0xFF) == 0 ? FLAG_Z : 0) | // Z flag
                  (y >= operand ? FLAG_C : 0); // C flag
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// ============================================================================
// INCREMENT MEMORY (INC)
// ============================================================================

bus_state_t op_inc(bus_state_t pins) {
    // Read-Modify-Write operation
    
    // Read current value
    pins = this->phi2_read(pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Modify (increment)
    uint8_t value = CPU_DL(this) + 1;
    
    // Update flags
    CPU_P(this) = (CPU_P(this) & 0x7D) |  // Clear N,Z (preserve others)
                  (value & 0x80) |         // N flag
                  (value == 0 ? FLAG_Z : 0); // Z flag
    
    // Write back modified value
    pins = this->phi2_write(pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// ============================================================================
// DECREMENT MEMORY (DEC)
// ============================================================================

bus_state_t op_dec(bus_state_t pins) {
    // Read-Modify-Write operation
    
    // Read current value
    pins = this->phi2_read(pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Modify (decrement)
    uint8_t value = CPU_DL(this) - 1;
    
    // Update flags
    CPU_P(this) = (CPU_P(this) & 0x7D) |  // Clear N,Z (preserve others)
                  (value & 0x80) |         // N flag
                  (value == 0 ? FLAG_Z : 0); // Z flag
    
    // Write back modified value
    pins = this->phi2_write(pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Complete instruction
    transition_to_fetch();
    return pins;
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