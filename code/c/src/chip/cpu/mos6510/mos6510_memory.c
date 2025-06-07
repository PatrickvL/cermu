#include "mos6510.h"

// Macro to define load/store handlers
#define DEFINE_LOAD_OP(fn, addr_func, op_func) \
    void fn(mos6510_t* cpu_dev) { mos6510_load_helper(cpu_dev, addr_func, op_func); }
#define DEFINE_STORE_OP(fn, addr_store_func, reg)  \
    void fn(mos6510_t* cpu_dev) { mos6510_store_helper(cpu_dev, addr_store_func, cpu_dev->reg); }

// ============================================================================
// MOS 6510 MEMORY INSTRUCTIONS
// ============================================================================
// Load and Store instructions for A, X, Y registers

// One-line load helpers
DEFINE_LOAD_OP(lda_immediate_func,      addr_imm,      op_lda)
DEFINE_LOAD_OP(lda_zero_page_func,      addr_zp,       op_lda)
DEFINE_LOAD_OP(lda_zero_page_x_func,    addr_zpx,      op_lda)
DEFINE_LOAD_OP(lda_absolute_func,       addr_abs,      op_lda)
DEFINE_LOAD_OP(lda_absolute_x_func,     addr_absx,     op_lda)
DEFINE_LOAD_OP(lda_absolute_y_func,     addr_absy,     op_lda)
DEFINE_LOAD_OP(lda_indirect_x_func,     addr_zpx_ind,  op_lda)
DEFINE_LOAD_OP(lda_indirect_y_func,     addr_zp_ind_y, op_lda)

DEFINE_LOAD_OP(ldx_immediate_func,      addr_imm,      op_ldx)
DEFINE_LOAD_OP(ldx_zero_page_func,      addr_zp,       op_ldx)
DEFINE_LOAD_OP(ldx_zero_page_y_func,    addr_zpy,      op_ldx)
DEFINE_LOAD_OP(ldx_absolute_func,       addr_abs,      op_ldx)
DEFINE_LOAD_OP(ldx_absolute_y_func,     addr_absy,     op_ldx)

DEFINE_LOAD_OP(ldy_immediate_func,      addr_imm,      op_ldy)
DEFINE_LOAD_OP(ldy_zero_page_func,      addr_zp,       op_ldy)
DEFINE_LOAD_OP(ldy_zero_page_x_func,    addr_zpx,      op_ldy)
DEFINE_LOAD_OP(ldy_absolute_func,       addr_abs,      op_ldy)
DEFINE_LOAD_OP(ldy_absolute_x_func,     addr_absx,     op_ldy)

// One-line store helpers
DEFINE_STORE_OP(sta_zero_page_func,        addr_zp_store, a)
DEFINE_STORE_OP(sta_zero_page_x_func,      addr_zpx_store, a)
DEFINE_STORE_OP(sta_absolute_func,         addr_abs_store, a)
DEFINE_STORE_OP(sta_absolute_x_func,       addr_absx_store, a)
DEFINE_STORE_OP(sta_absolute_y_func,       addr_absy_store, a)
DEFINE_STORE_OP(sta_indirect_x_func,       addr_zpx_ind_store, a)
DEFINE_STORE_OP(sta_indirect_y_func,       addr_zp_ind_y_store, a)

DEFINE_STORE_OP(stx_zero_page_func,        addr_zp_store, x)
DEFINE_STORE_OP(stx_zero_page_y_func,      addr_zpy_store, x)
DEFINE_STORE_OP(stx_absolute_func,         addr_abs_store, x)

DEFINE_STORE_OP(sty_zero_page_func,        addr_zp_store, y)
DEFINE_STORE_OP(sty_zero_page_x_func,      addr_zpx_store, y)
DEFINE_STORE_OP(sty_absolute_func,         addr_abs_store, y)
