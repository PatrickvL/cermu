#include "mos6502_family_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY OPCODE OPERATIONS
// ============================================================================

// Shared arithmetic operations (used by all family members)
void mos6502_family_op_adc(mos6502_family_t* cpu, uint8_t value) {
    uint16_t result = cpu->a + value + (mos6502_family_get_flag(cpu, FLAG_C) ? 1 : 0);
    
    // Set overflow flag (signed overflow)
    bool overflow = ((cpu->a ^ result) & (value ^ result) & 0x80) != 0;
    mos6502_family_set_flag(cpu, FLAG_V, overflow);
    
    // Set carry flag
    mos6502_family_set_flag(cpu, FLAG_C, result > 0xFF);
    
    // Update accumulator and set N/Z flags
    cpu->a = result & 0xFF;
    mos6502_family_set_nz_flags(cpu, cpu->a);
}

void mos6502_family_op_sbc(mos6502_family_t* cpu, uint8_t value) {
    // SBC is equivalent to ADC with inverted value
    uint8_t inverted_value = ~value;
    uint16_t result = cpu->a + inverted_value + (mos6502_family_get_flag(cpu, FLAG_C) ? 1 : 0);
    
    // Set overflow flag (signed overflow)
    bool overflow = ((cpu->a ^ result) & (inverted_value ^ result) & 0x80) != 0;
    mos6502_family_set_flag(cpu, FLAG_V, overflow);
    
    // Set carry flag (inverted for subtraction)
    mos6502_family_set_flag(cpu, FLAG_C, result > 0xFF);
    
    // Update accumulator and set N/Z flags
    cpu->a = result & 0xFF;
    mos6502_family_set_nz_flags(cpu, cpu->a);
}

void mos6502_family_op_and(mos6502_family_t* cpu, uint8_t value) {
    cpu->a &= value;
    mos6502_family_set_nz_flags(cpu, cpu->a);
}

void mos6502_family_op_ora(mos6502_family_t* cpu, uint8_t value) {
    cpu->a |= value;
    mos6502_family_set_nz_flags(cpu, cpu->a);
}

void mos6502_family_op_eor(mos6502_family_t* cpu, uint8_t value) {
    cpu->a ^= value;
    mos6502_family_set_nz_flags(cpu, cpu->a);
}

void mos6502_family_op_cmp(mos6502_family_t* cpu, uint8_t value) {
    uint16_t result = cpu->a - value;
    mos6502_family_set_flag(cpu, FLAG_C, cpu->a >= value);
    mos6502_family_set_nz_flags(cpu, result & 0xFF);
}

void mos6502_family_op_cpx(mos6502_family_t* cpu, uint8_t value) {
    uint16_t result = cpu->x - value;
    mos6502_family_set_flag(cpu, FLAG_C, cpu->x >= value);
    mos6502_family_set_nz_flags(cpu, result & 0xFF);
}

void mos6502_family_op_cpy(mos6502_family_t* cpu, uint8_t value) {
    uint16_t result = cpu->y - value;
    mos6502_family_set_flag(cpu, FLAG_C, cpu->y >= value);
    mos6502_family_set_nz_flags(cpu, result & 0xFF);
}

void mos6502_family_op_bit(mos6502_family_t* cpu, uint8_t value) {
    uint8_t result = cpu->a & value;
    mos6502_family_set_flag(cpu, FLAG_Z, result == 0);
    mos6502_family_set_flag(cpu, FLAG_N, (value & 0x80) != 0);
    mos6502_family_set_flag(cpu, FLAG_V, (value & 0x40) != 0);
}

// ============================================================================
// SHARED OPCODE IMPLEMENTATIONS - ADC
// ============================================================================
// Note: mos6502_family_arithmetic_helper is now inlined in the header for performance

void mos6502_family_adc_immediate(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_imm, mos6502_family_op_adc);
}

void mos6502_family_adc_zero_page(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_zp, mos6502_family_op_adc);
}

void mos6502_family_adc_zero_page_x(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_zpx, mos6502_family_op_adc);
}

void mos6502_family_adc_absolute(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_abs, mos6502_family_op_adc);
}

void mos6502_family_adc_absolute_x(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_absx, mos6502_family_op_adc);
}

void mos6502_family_adc_absolute_y(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_absy, mos6502_family_op_adc);
}

void mos6502_family_adc_indirect_x(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_indx, mos6502_family_op_adc);
}

void mos6502_family_adc_indirect_y(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_indy, mos6502_family_op_adc);
}

// ============================================================================
// SHARED OPCODE IMPLEMENTATIONS - SBC
// ============================================================================

void mos6502_family_sbc_immediate(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_imm, mos6502_family_op_sbc);
}

void mos6502_family_sbc_zero_page(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_zp, mos6502_family_op_sbc);
}

void mos6502_family_sbc_zero_page_x(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_zpx, mos6502_family_op_sbc);
}

void mos6502_family_sbc_absolute(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_abs, mos6502_family_op_sbc);
}

void mos6502_family_sbc_absolute_x(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_absx, mos6502_family_op_sbc);
}

void mos6502_family_sbc_absolute_y(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_absy, mos6502_family_op_sbc);
}

void mos6502_family_sbc_indirect_x(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_indx, mos6502_family_op_sbc);
}

void mos6502_family_sbc_indirect_y(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_indy, mos6502_family_op_sbc);
}

// ============================================================================
// SHARED OPCODE IMPLEMENTATIONS - AND (using inline helpers for performance)
// ============================================================================

void mos6502_family_and_immediate(mos6502_family_t* cpu) {
    MOS6502_FAMILY_AND_HELPER(cpu, mos6502_family_addr_imm);
}

void mos6502_family_and_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_AND_HELPER(cpu, mos6502_family_addr_zp);
}

void mos6502_family_and_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_AND_HELPER(cpu, mos6502_family_addr_zpx);
}

void mos6502_family_and_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_AND_HELPER(cpu, mos6502_family_addr_abs);
}

void mos6502_family_and_absolute_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_AND_HELPER(cpu, mos6502_family_addr_absx);
}

void mos6502_family_and_absolute_y(mos6502_family_t* cpu) {
    MOS6502_FAMILY_AND_HELPER(cpu, mos6502_family_addr_absy);
}

void mos6502_family_and_indirect_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_AND_HELPER(cpu, mos6502_family_addr_indx);
}

void mos6502_family_and_indirect_y(mos6502_family_t* cpu) {
    MOS6502_FAMILY_AND_HELPER(cpu, mos6502_family_addr_indy);
}

// ============================================================================
// SHARED OPCODE IMPLEMENTATIONS - ORA (using inline helpers for performance)
// ============================================================================

void mos6502_family_ora_immediate(mos6502_family_t* cpu) {
    MOS6502_FAMILY_ORA_HELPER(cpu, mos6502_family_addr_imm);
}

void mos6502_family_ora_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_ORA_HELPER(cpu, mos6502_family_addr_zp);
}

void mos6502_family_ora_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_ORA_HELPER(cpu, mos6502_family_addr_zpx);
}

void mos6502_family_ora_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_ORA_HELPER(cpu, mos6502_family_addr_abs);
}

void mos6502_family_ora_absolute_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_ORA_HELPER(cpu, mos6502_family_addr_absx);
}

void mos6502_family_ora_absolute_y(mos6502_family_t* cpu) {
    MOS6502_FAMILY_ORA_HELPER(cpu, mos6502_family_addr_absy);
}

void mos6502_family_ora_indirect_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_ORA_HELPER(cpu, mos6502_family_addr_indx);
}

void mos6502_family_ora_indirect_y(mos6502_family_t* cpu) {
    MOS6502_FAMILY_ORA_HELPER(cpu, mos6502_family_addr_indy);
}

// ============================================================================
// SHARED OPCODE IMPLEMENTATIONS - EOR (using inline helpers for performance)
// ============================================================================

void mos6502_family_eor_immediate(mos6502_family_t* cpu) {
    MOS6502_FAMILY_EOR_HELPER(cpu, mos6502_family_addr_imm);
}

void mos6502_family_eor_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_EOR_HELPER(cpu, mos6502_family_addr_zp);
}

void mos6502_family_eor_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_EOR_HELPER(cpu, mos6502_family_addr_zpx);
}

void mos6502_family_eor_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_EOR_HELPER(cpu, mos6502_family_addr_abs);
}

void mos6502_family_eor_absolute_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_EOR_HELPER(cpu, mos6502_family_addr_absx);
}

void mos6502_family_eor_absolute_y(mos6502_family_t* cpu) {
    MOS6502_FAMILY_EOR_HELPER(cpu, mos6502_family_addr_absy);
}

void mos6502_family_eor_indirect_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_EOR_HELPER(cpu, mos6502_family_addr_indx);
}

void mos6502_family_eor_indirect_y(mos6502_family_t* cpu) {
    MOS6502_FAMILY_EOR_HELPER(cpu, mos6502_family_addr_indy);
}
