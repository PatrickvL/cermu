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

// Add with Carry in BCD mode
uint8_t adc_bcd(uint8_t a, uint8_t b, bool carry_in, bool& carry_out, bool& overflow) {
    uint16_t al = (a & 0x0F) + (b & 0x0F) + (carry_in ? 1 : 0);
    if (al > 0x09) al += 0x06;
    
    uint16_t ah = (a >> 4) + (b >> 4) + (al > 0x0F ? 1 : 0);
    if (ah > 0x09) ah += 0x06;
    
    carry_out = (ah > 0x0F);
    
    // V flag behavior differs between NMOS and CMOS
    if constexpr (has_nmos_bugs<ProcessorTag>()) {
        // NMOS: V flag reflects binary operation result  
        uint16_t binary_result = a + b + (carry_in ? 1 : 0);
        overflow = ((a ^ binary_result) & (b ^ binary_result) & 0x80) != 0;
    } else {
        // CMOS: V flag undefined in BCD mode
        overflow = false;
    }
    
    return ((ah & 0x0F) << 4) | (al & 0x0F);
}

// BCD addition helper matching old implementation signature
void bcd_addition_helper(uint8_t a_old, uint8_t operand, uint8_t carry_in, uint8_t* bcd_result, uint8_t* bcd_flags) {
    bool carry_out, overflow;
    *bcd_result = adc_bcd(a_old, operand, carry_in != 0, carry_out, overflow);
    
    // Generate flags matching old implementation
    *bcd_flags = (*bcd_result & 0x80) |                    // N flag
                (*bcd_result == 0 ? 0x02 : 0) |           // Z flag (FLAG_Z = 0x02)
                (carry_out ? 0x01 : 0) |                  // C flag (FLAG_C = 0x01)
                (overflow ? 0x40 : 0);                    // V flag (FLAG_V = 0x40)
}

// Subtract with Borrow in BCD mode  
uint8_t sbc_bcd(uint8_t a, uint8_t b, bool borrow_in, bool& carry_out, bool& overflow) {
    uint16_t al = (a & 0x0F) - (b & 0x0F) - (borrow_in ? 0 : 1);
    if (al & 0x10) al -= 0x06;
    
    uint16_t ah = (a >> 4) - (b >> 4) - ((al & 0x10) ? 1 : 0);
    if (ah & 0x10) ah -= 0x06;
    
    carry_out = !(ah & 0x10);
    
    // V flag behavior differs between NMOS and CMOS
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

// BCD subtraction helper matching old implementation signature
void bcd_subtraction_helper(uint8_t a_old, uint8_t operand, uint8_t borrow_in, 
                           uint8_t* bcd_result, uint8_t* bcd_flags) {
    if constexpr (has_bcd<ProcessorTag>()) {
        bool carry_out, overflow;
        *bcd_result = sbc_bcd(a_old, operand, borrow_in != 0, carry_out, overflow);
        
        // Generate flags matching old implementation
        *bcd_flags = (*bcd_result & 0x80) |          // N flag
                    (*bcd_result == 0 ? 0x02 : 0) |  // Z flag (FLAG_Z = 0x02)
                    (carry_out ? 0x01 : 0) |         // C flag (FLAG_C = 0x01)
                    (overflow ? 0x40 : 0);           // V flag (FLAG_V = 0x40)
    } else {
        // No BCD support - should not be called
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
        
        uint16_t result;
        bool carry_out, overflow;
        
        // Handle decimal mode if supported (matching old implementation)
        if constexpr (has_bcd<ProcessorTag>()) {
            bool decimal_mode = (CPU_P(this) & FLAG_D) != 0;
            if (decimal_mode) {
                // BCD (Decimal) mode - use BCD addition helper
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
        result = a + operand + carry_in;
        CPU_A(this) = result & 0xFF;
        
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
            }
            return pins;
            
        case AM_NON:
            /* AM_NON: All implicit NOPs do dummy read from PC without increment */
            trace("Implicit NOP: dummy read from PC");
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) {
                trace("RDY low - returning early");
                trace_exit("op_nop");
            }
            return pins;
            
        default:
            /* Memory modes: Read from target address and discard */
            trace("Memory mode NOP: reading from target address");
            pins = phi2_read(pins, REG_AB, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) {
                trace("RDY low - returning early");
                trace_exit("op_nop");
            }
            return pins;
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
        bool borrow_in = this->get_borrow_input(); // Inverted carry for SBC
        
        uint16_t result;
        bool carry_out, overflow;
        
        // Handle decimal mode if supported (matching old implementation)
        if constexpr (has_bcd<ProcessorTag>()) {
            bool decimal_mode = (CPU_P(this) & FLAG_D) != 0;
            if (decimal_mode) {
                // BCD (Decimal) mode - use BCD subtraction helper
                uint8_t bcd_result;
                uint8_t bcd_flags;
                
                bcd_subtraction_helper(a, operand, borrow_in ? 1 : 0, &bcd_result, &bcd_flags);
                
                CPU_A(this) = bcd_result;
                update_flags(FLAG_N | FLAG_V | FLAG_Z | FLAG_C, bcd_flags);
                // Complete instruction
                transition_to_fetch();
                return pins;
            }
            // No BCD support - treat as binary
        }
        
        // Binary mode subtraction (exact flag calculation matching old implementation)
        result = a - operand - (borrow_in ? 1 : 0);
        CPU_A(this) = result & 0xFF;
        
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