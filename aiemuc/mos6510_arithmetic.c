#include "mos6510.h"

// ============================================================================
// MOS 6510 ARITHMETIC AND LOGIC INSTRUCTIONS
// ============================================================================

// ADC - Add with Carry
void adc_immediate_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_imm, op_adc);
}

void adc_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp, op_adc);
}

void adc_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx, op_adc);
}

void adc_absolute_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_abs, op_adc);
}

void adc_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absx, op_adc);
}

void adc_absolute_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absy, op_adc);
}

void adc_indirect_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx_ind, op_adc);
}

void adc_indirect_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp_ind_y, op_adc);
}

// SBC - Subtract with Carry
void sbc_immediate_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_imm, op_sbc);
}

void sbc_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp, op_sbc);
}

void sbc_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx, op_sbc);
}

void sbc_absolute_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_abs, op_sbc);
}

void sbc_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absx, op_sbc);
}

void sbc_absolute_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absy, op_sbc);
}

void sbc_indirect_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx_ind, op_sbc);
}

void sbc_indirect_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp_ind_y, op_sbc);
}

// AND - Logical AND
void and_immediate_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_imm, op_and);
}

void and_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp, op_and);
}

void and_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx, op_and);
}

void and_absolute_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_abs, op_and);
}

void and_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absx, op_and);
}

void and_absolute_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absy, op_and);
}

void and_indirect_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx_ind, op_and);
}

void and_indirect_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp_ind_y, op_and);
}

// ORA - Logical OR
void ora_immediate_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_imm, op_ora);
}

void ora_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp, op_ora);
}

void ora_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx, op_ora);
}

void ora_absolute_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_abs, op_ora);
}

void ora_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absx, op_ora);
}

void ora_absolute_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absy, op_ora);
}

void ora_indirect_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx_ind, op_ora);
}

void ora_indirect_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp_ind_y, op_ora);
}

// EOR - Exclusive OR
void eor_immediate_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_imm, op_eor);
}

void eor_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp, op_eor);
}

void eor_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx, op_eor);
}

void eor_absolute_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_abs, op_eor);
}

void eor_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absx, op_eor);
}

void eor_absolute_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absy, op_eor);
}

void eor_indirect_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx_ind, op_eor);
}

void eor_indirect_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp_ind_y, op_eor);
}

// CMP - Compare Accumulator
void cmp_immediate_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_imm, op_cmp);
}

void cmp_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp, op_cmp);
}

void cmp_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx, op_cmp);
}

void cmp_absolute_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_abs, op_cmp);
}

void cmp_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absx, op_cmp);
}

void cmp_absolute_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_absy, op_cmp);
}

void cmp_indirect_x_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zpx_ind, op_cmp);
}

void cmp_indirect_y_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp_ind_y, op_cmp);
}

// CPX - Compare X Register
void cpx_immediate_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_imm, op_cpx);
}

void cpx_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp, op_cpx);
}

void cpx_absolute_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_abs, op_cpx);
}

// CPY - Compare Y Register  
void cpy_immediate_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_imm, op_cpy);
}

void cpy_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp, op_cpy);
}

void cpy_absolute_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_abs, op_cpy);
}

// BIT - Bit Test
void bit_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_zp, op_bit);
}

void bit_absolute_func(mos6510_t* cpu_dev) {
    mos6510_arithmetic_helper(cpu_dev, addr_abs, op_bit);
}
