#include "mos6510.h"

// Macro to define arithmetic instructions
#define DEFINE_ARITH_OP(fn, addr_func, op_func) \
    void fn(mos6510_t* cpu) { mos6510_arithmetic_helper(cpu, addr_func, op_func); }

// ============================================================================
// MOS 6510 ARITHMETIC AND LOGIC INSTRUCTIONS
// ============================================================================

// ADC - Add with Carry
DEFINE_ARITH_OP(adc_immediate_func,       addr_imm,     op_adc)
DEFINE_ARITH_OP(adc_zero_page_func,       addr_zp,      op_adc)
DEFINE_ARITH_OP(adc_zero_page_x_func,     addr_zpx,     op_adc)
DEFINE_ARITH_OP(adc_absolute_func,        addr_abs,     op_adc)
DEFINE_ARITH_OP(adc_absolute_x_func,      addr_absx,    op_adc)
DEFINE_ARITH_OP(adc_absolute_y_func,      addr_absy,    op_adc)
DEFINE_ARITH_OP(adc_indirect_x_func,      addr_zpx_ind, op_adc)
DEFINE_ARITH_OP(adc_indirect_y_func,      addr_zp_ind_y,op_adc)

// SBC - Subtract with Carry
DEFINE_ARITH_OP(sbc_immediate_func,       addr_imm,     op_sbc)
DEFINE_ARITH_OP(sbc_zero_page_func,       addr_zp,      op_sbc)
DEFINE_ARITH_OP(sbc_zero_page_x_func,     addr_zpx,     op_sbc)
DEFINE_ARITH_OP(sbc_absolute_func,        addr_abs,     op_sbc)
DEFINE_ARITH_OP(sbc_absolute_x_func,      addr_absx,    op_sbc)
DEFINE_ARITH_OP(sbc_absolute_y_func,      addr_absy,    op_sbc)
DEFINE_ARITH_OP(sbc_indirect_x_func,      addr_zpx_ind, op_sbc)
DEFINE_ARITH_OP(sbc_indirect_y_func,      addr_zp_ind_y,op_sbc)

// AND - Logical AND
DEFINE_ARITH_OP(and_immediate_func,       addr_imm,     op_and)
DEFINE_ARITH_OP(and_zero_page_func,       addr_zp,      op_and)
DEFINE_ARITH_OP(and_zero_page_x_func,     addr_zpx,     op_and)
DEFINE_ARITH_OP(and_absolute_func,        addr_abs,     op_and)
DEFINE_ARITH_OP(and_absolute_x_func,      addr_absx,    op_and)
DEFINE_ARITH_OP(and_absolute_y_func,      addr_absy,    op_and)
DEFINE_ARITH_OP(and_indirect_x_func,      addr_zpx_ind, op_and)
DEFINE_ARITH_OP(and_indirect_y_func,      addr_zp_ind_y,op_and)

// ORA - Logical OR
DEFINE_ARITH_OP(ora_immediate_func,       addr_imm,     op_ora)
DEFINE_ARITH_OP(ora_zero_page_func,       addr_zp,      op_ora)
DEFINE_ARITH_OP(ora_zero_page_x_func,     addr_zpx,     op_ora)
DEFINE_ARITH_OP(ora_absolute_func,        addr_abs,     op_ora)
DEFINE_ARITH_OP(ora_absolute_x_func,      addr_absx,    op_ora)
DEFINE_ARITH_OP(ora_absolute_y_func,      addr_absy,    op_ora)
DEFINE_ARITH_OP(ora_indirect_x_func,      addr_zpx_ind, op_ora)
DEFINE_ARITH_OP(ora_indirect_y_func,      addr_zp_ind_y,op_ora)

// EOR - Exclusive OR
DEFINE_ARITH_OP(eor_immediate_func,       addr_imm,     op_eor)
DEFINE_ARITH_OP(eor_zero_page_func,       addr_zp,      op_eor)
DEFINE_ARITH_OP(eor_zero_page_x_func,     addr_zpx,     op_eor)
DEFINE_ARITH_OP(eor_absolute_func,        addr_abs,     op_eor)
DEFINE_ARITH_OP(eor_absolute_x_func,      addr_absx,    op_eor)
DEFINE_ARITH_OP(eor_absolute_y_func,      addr_absy,    op_eor)
DEFINE_ARITH_OP(eor_indirect_x_func,      addr_zpx_ind, op_eor)
DEFINE_ARITH_OP(eor_indirect_y_func,      addr_zp_ind_y,op_eor)

// CMP - Compare Accumulator
DEFINE_ARITH_OP(cmp_immediate_func,       addr_imm,     op_cmp)
DEFINE_ARITH_OP(cmp_zero_page_func,       addr_zp,      op_cmp)
DEFINE_ARITH_OP(cmp_zero_page_x_func,     addr_zpx,     op_cmp)
DEFINE_ARITH_OP(cmp_absolute_func,        addr_abs,     op_cmp)
DEFINE_ARITH_OP(cmp_absolute_x_func,      addr_absx,    op_cmp)
DEFINE_ARITH_OP(cmp_absolute_y_func,      addr_absy,    op_cmp)
DEFINE_ARITH_OP(cmp_indirect_x_func,      addr_zpx_ind, op_cmp)
DEFINE_ARITH_OP(cmp_indirect_y_func,      addr_zp_ind_y,op_cmp)

// CPX - Compare X Register
DEFINE_ARITH_OP(cpx_immediate_func,       addr_imm,     op_cpx)
DEFINE_ARITH_OP(cpx_zero_page_func,       addr_zp,      op_cpx)
DEFINE_ARITH_OP(cpx_absolute_func,        addr_abs,     op_cpx)

// CPY - Compare Y Register  
DEFINE_ARITH_OP(cpy_immediate_func,       addr_imm,     op_cpy)
DEFINE_ARITH_OP(cpy_zero_page_func,       addr_zp,      op_cpy)
DEFINE_ARITH_OP(cpy_absolute_func,        addr_abs,     op_cpy)

// BIT - Bit Test
DEFINE_ARITH_OP(bit_zero_page_func,       addr_zp,      op_bit)
DEFINE_ARITH_OP(bit_absolute_func,        addr_abs,     op_bit)
