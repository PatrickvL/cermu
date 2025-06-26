#include "fam65xx_arithmetic.h"

// ============================================================================
// SHARED MOS 6502 FAMILY OPCODE OPERATIONS
// ============================================================================

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
static inline void mos6502_op_adc_decimal(fam65xx_t* cpu, uint8_t value) {
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        // NMOS 6502 decimal mode - flags set from binary, result is BCD
        uint8_t carry_in = fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0;
        uint8_t original_a = cpu->a;
        
        // First do binary addition for flag calculation
        uint16_t binary_sum = original_a + value + carry_in;
        
        // Set N and V flags based on binary result (NMOS 6502 behavior)
        fam65xx_set_flag(cpu, FLAG_N, (binary_sum & 0x80) != 0);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_sum & 0xFF) == 0);
        bool v_flag = ((original_a ^ binary_sum) & (value ^ binary_sum) & 0x80) != 0;
        fam65xx_set_flag(cpu, FLAG_V, v_flag);
        
        // Now do actual BCD arithmetic for the result
        uint16_t al = (original_a & 0x0F) + (value & 0x0F) + carry_in;
        if (al >= 0x0A) {
            al = ((al + 0x06) & 0x0F) + 0x10;
        }
        
        uint16_t bcd_result = (original_a & 0xF0) + (value & 0xF0) + al;
        if (bcd_result >= 0xA0) {
            bcd_result += 0x60;
        }
        
        // Set C flag and update accumulator based on BCD result
        fam65xx_set_flag(cpu, FLAG_C, bcd_result >= 0x100);
        cpu->a = bcd_result & 0xFF;
    } else {
        // Binary mode - use standard implementation
        fam65xx_op_adc(cpu, value);
    }
}

// Decimal mode SBC operation (for MOS6502 with functional decimal mode)
static inline void mos6502_op_sbc_decimal(fam65xx_t* cpu, uint8_t value) {
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
