#include "fam65xx_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY OPCODE OPERATIONS
// ============================================================================

// Shared arithmetic operations (used by all family members)
void fam65xx_op_adc(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->a + value + (fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0);
    
    // Set overflow flag (signed overflow)
    bool overflow = ((cpu->a ^ result) & (value ^ result) & 0x80) != 0;
    fam65xx_set_flag(cpu, FLAG_V, overflow);
    
    // Set carry flag
    fam65xx_set_flag(cpu, FLAG_C, result > 0xFF);
    
    // Update accumulator and set N/Z flags
    cpu->a = result & 0xFF;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

void fam65xx_op_sbc(fam65xx_t* cpu, uint8_t value) {
    // SBC is equivalent to ADC with inverted value
    uint8_t inverted_value = ~value;
    uint16_t result = cpu->a + inverted_value + (fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0);
    
    // Set overflow flag (signed overflow)
    bool overflow = ((cpu->a ^ result) & (inverted_value ^ result) & 0x80) != 0;
    fam65xx_set_flag(cpu, FLAG_V, overflow);
    
    // Set carry flag (inverted for subtraction)
    fam65xx_set_flag(cpu, FLAG_C, result > 0xFF);
    
    // Update accumulator and set N/Z flags
    cpu->a = result & 0xFF;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

void fam65xx_op_and(fam65xx_t* cpu, uint8_t value) {
    cpu->a &= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

void fam65xx_op_ora(fam65xx_t* cpu, uint8_t value) {
    cpu->a |= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

void fam65xx_op_eor(fam65xx_t* cpu, uint8_t value) {
    cpu->a ^= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

void fam65xx_op_cmp(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->a - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->a >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

void fam65xx_op_cpx(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->x - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->x >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

void fam65xx_op_cpy(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->y - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->y >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

void fam65xx_op_bit(fam65xx_t* cpu, uint8_t value) {
    uint8_t result = cpu->a & value;
    fam65xx_set_flag(cpu, FLAG_Z, result == 0);
    fam65xx_set_flag(cpu, FLAG_N, (value & 0x80) != 0);
    fam65xx_set_flag(cpu, FLAG_V, (value & 0x40) != 0);
}

// ============================================================================
// SHARED OPCODE IMPLEMENTATIONS - ADC
// ============================================================================

void fam65xx_adc_immediate(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_imm, fam65xx_op_adc);
}

void fam65xx_adc_zero_page(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_zp, fam65xx_op_adc);
}

void fam65xx_adc_zero_page_x(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_zpx, fam65xx_op_adc);
}

void fam65xx_adc_absolute(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_abs, fam65xx_op_adc);
}

void fam65xx_adc_absolute_x(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_absx, fam65xx_op_adc);
}

void fam65xx_adc_absolute_y(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_absy, fam65xx_op_adc);
}

void fam65xx_adc_indirect_x(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_indx, fam65xx_op_adc);
}

void fam65xx_adc_indirect_y(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_indy, fam65xx_op_adc);
}

// ============================================================================
// SHARED OPCODE IMPLEMENTATIONS - SBC
// ============================================================================

void fam65xx_sbc_immediate(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_imm, fam65xx_op_sbc);
}

void fam65xx_sbc_zero_page(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_zp, fam65xx_op_sbc);
}

void fam65xx_sbc_zero_page_x(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_zpx, fam65xx_op_sbc);
}

void fam65xx_sbc_absolute(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_abs, fam65xx_op_sbc);
}

void fam65xx_sbc_absolute_x(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_absx, fam65xx_op_sbc);
}

void fam65xx_sbc_absolute_y(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_absy, fam65xx_op_sbc);
}

void fam65xx_sbc_indirect_x(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_indx, fam65xx_op_sbc);
}

void fam65xx_sbc_indirect_y(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_indy, fam65xx_op_sbc);
}

// ============================================================================
// SHARED OPCODE IMPLEMENTATIONS - AND (using inline helpers for performance)
// ============================================================================

void fam65xx_and_immediate(fam65xx_t* cpu) {
    fam65xx_and_helper(cpu, fam65xx_addr_imm);
}

void fam65xx_and_zero_page(fam65xx_t* cpu) {
    fam65xx_and_helper(cpu, fam65xx_addr_zp);
}

void fam65xx_and_zero_page_x(fam65xx_t* cpu) {
    fam65xx_and_helper(cpu, fam65xx_addr_zpx);
}

void fam65xx_and_absolute(fam65xx_t* cpu) {
    fam65xx_and_helper(cpu, fam65xx_addr_abs);
}

void fam65xx_and_absolute_x(fam65xx_t* cpu) {
    fam65xx_and_helper(cpu, fam65xx_addr_absx);
}

void fam65xx_and_absolute_y(fam65xx_t* cpu) {
    fam65xx_and_helper(cpu, fam65xx_addr_absy);
}

void fam65xx_and_indirect_x(fam65xx_t* cpu) {
    fam65xx_and_helper(cpu, fam65xx_addr_indx);
}

void fam65xx_and_indirect_y(fam65xx_t* cpu) {
    fam65xx_and_helper(cpu, fam65xx_addr_indy);
}

// ============================================================================
// SHARED OPCODE IMPLEMENTATIONS - ORA (using inline helpers for performance)
// ============================================================================

void fam65xx_ora_immediate(fam65xx_t* cpu) {
    fam65xx_ora_helper(cpu, fam65xx_addr_imm);
}

void fam65xx_ora_zero_page(fam65xx_t* cpu) {
    fam65xx_ora_helper(cpu, fam65xx_addr_zp);
}

void fam65xx_ora_zero_page_x(fam65xx_t* cpu) {
    fam65xx_ora_helper(cpu, fam65xx_addr_zpx);
}

void fam65xx_ora_absolute(fam65xx_t* cpu) {
    fam65xx_ora_helper(cpu, fam65xx_addr_abs);
}

void fam65xx_ora_absolute_x(fam65xx_t* cpu) {
    fam65xx_ora_helper(cpu, fam65xx_addr_absx);
}

void fam65xx_ora_absolute_y(fam65xx_t* cpu) {
    fam65xx_ora_helper(cpu, fam65xx_addr_absy);
}

void fam65xx_ora_indirect_x(fam65xx_t* cpu) {
    fam65xx_ora_helper(cpu, fam65xx_addr_indx);
}

void fam65xx_ora_indirect_y(fam65xx_t* cpu) {
    fam65xx_ora_helper(cpu, fam65xx_addr_indy);
}

// ============================================================================
// SHARED OPCODE IMPLEMENTATIONS - EOR (using inline helpers for performance)
// ============================================================================

void fam65xx_eor_immediate(fam65xx_t* cpu) {
    fam65xx_eor_helper(cpu, fam65xx_addr_imm);
}

void fam65xx_eor_zero_page(fam65xx_t* cpu) {
    fam65xx_eor_helper(cpu, fam65xx_addr_zp);
}

void fam65xx_eor_zero_page_x(fam65xx_t* cpu) {
    fam65xx_eor_helper(cpu, fam65xx_addr_zpx);
}

void fam65xx_eor_absolute(fam65xx_t* cpu) {
    fam65xx_eor_helper(cpu, fam65xx_addr_abs);
}

void fam65xx_eor_absolute_x(fam65xx_t* cpu) {
    fam65xx_eor_helper(cpu, fam65xx_addr_absx);
}

void fam65xx_eor_absolute_y(fam65xx_t* cpu) {
    fam65xx_eor_helper(cpu, fam65xx_addr_absy);
}

void fam65xx_eor_indirect_x(fam65xx_t* cpu) {
    fam65xx_eor_helper(cpu, fam65xx_addr_indx);
}

void fam65xx_eor_indirect_y(fam65xx_t* cpu) {
    fam65xx_eor_helper(cpu, fam65xx_addr_indy);
}

// Decimal mode ADC operation (for MOS6502 with functional decimal mode)
void mos6502_op_adc_decimal(fam65xx_t* cpu, uint8_t value) {
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        // Decimal (BCD) mode arithmetic
        uint8_t carry_in = fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0;
        
        // Convert to BCD
        uint8_t al = (cpu->a & 0x0F) + (value & 0x0F) + carry_in;
        uint8_t ah = (cpu->a >> 4) + (value >> 4);
        
        // Handle low nibble carry
        if (al > 9) {
            al = (al + 6) & 0x0F;
            ah++;
        }
        
        // Set N and Z flags based on binary result (6502 quirk)
        uint16_t binary_result = cpu->a + value + carry_in;
        fam65xx_set_flag(cpu, FLAG_N, (binary_result & 0x80) != 0);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_result & 0xFF) == 0);
        
        // Set overflow flag based on binary result
        bool overflow = ((cpu->a ^ binary_result) & (value ^ binary_result) & 0x80) != 0;
        fam65xx_set_flag(cpu, FLAG_V, overflow);
        
        // Handle high nibble carry
        if (ah > 9) {
            ah = (ah + 6) & 0x0F;
            fam65xx_set_flag(cpu, FLAG_C, true);
        } else {
            fam65xx_set_flag(cpu, FLAG_C, false);
        }
        
        // Update accumulator with BCD result
        cpu->a = (ah << 4) | al;
    } else {
        // Binary mode - use standard implementation
        fam65xx_op_adc(cpu, value);
    }
}

// Decimal mode SBC operation (for MOS6502 with functional decimal mode)
void mos6502_op_sbc_decimal(fam65xx_t* cpu, uint8_t value) {
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        // Decimal (BCD) mode arithmetic
        uint8_t carry_in = fam65xx_get_flag(cpu, FLAG_C) ? 0 : 1; // Inverted for SBC
        
        // Convert to BCD for subtraction
        int8_t al = (cpu->a & 0x0F) - (value & 0x0F) - carry_in;
        int8_t ah = (cpu->a >> 4) - (value >> 4);
        
        // Handle low nibble borrow
        if (al < 0) {
            al = (al - 6) & 0x0F;
            ah--;
        }
        
        // Set N and Z flags based on binary result (6502 quirk)
        uint16_t binary_result = cpu->a - value - carry_in;
        fam65xx_set_flag(cpu, FLAG_N, (binary_result & 0x80) != 0);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_result & 0xFF) == 0);
        
        // Set overflow flag based on binary result
        bool overflow = ((cpu->a ^ binary_result) & ((~value) ^ binary_result) & 0x80) != 0;
        fam65xx_set_flag(cpu, FLAG_V, overflow);
        
        // Handle high nibble borrow
        if (ah < 0) {
            ah = (ah - 6) & 0x0F;
            fam65xx_set_flag(cpu, FLAG_C, false);
        } else {
            fam65xx_set_flag(cpu, FLAG_C, true);
        }
        
        // Update accumulator with BCD result
        cpu->a = (ah << 4) | (al & 0x0F);
    } else {
        // Binary mode - use standard implementation
        fam65xx_op_sbc(cpu, value);
    }
}

// Decimal-aware ADC handlers for MOS6502 (all addressing modes)
void mos6502_adc_immediate_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_imm, mos6502_op_adc_decimal);
}

void mos6502_adc_zero_page_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_zp, mos6502_op_adc_decimal);
}

void mos6502_adc_zero_page_x_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_zpx, mos6502_op_adc_decimal);
}

void mos6502_adc_absolute_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_abs, mos6502_op_adc_decimal);
}

void mos6502_adc_absolute_x_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_absx, mos6502_op_adc_decimal);
}

void mos6502_adc_absolute_y_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_absy, mos6502_op_adc_decimal);
}

void mos6502_adc_indirect_x_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_indx, mos6502_op_adc_decimal);
}

void mos6502_adc_indirect_y_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_indy, mos6502_op_adc_decimal);
}

// Decimal-aware SBC handlers for MOS6502 (all addressing modes)
void mos6502_sbc_immediate_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_imm, mos6502_op_sbc_decimal);
}

void mos6502_sbc_zero_page_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_zp, mos6502_op_sbc_decimal);
}

void mos6502_sbc_zero_page_x_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_zpx, mos6502_op_sbc_decimal);
}

void mos6502_sbc_absolute_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_abs, mos6502_op_sbc_decimal);
}

void mos6502_sbc_absolute_x_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_absx, mos6502_op_sbc_decimal);
}

void mos6502_sbc_absolute_y_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_absy, mos6502_op_sbc_decimal);
}

void mos6502_sbc_indirect_x_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_indx, mos6502_op_sbc_decimal);
}

void mos6502_sbc_indirect_y_decimal(fam65xx_t* cpu) {
    fam65xx_arithmetic_helper(cpu, fam65xx_addr_indy, mos6502_op_sbc_decimal);
}
