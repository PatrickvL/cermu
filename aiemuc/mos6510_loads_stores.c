#include "mos6510.h"

// ============================================================================
// MOS 6510 LOAD AND STORE INSTRUCTIONS
// ============================================================================

// LDA - Load Accumulator
void lda_immediate_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_imm, op_lda);
}

void lda_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_zp, op_lda);
}

void lda_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_zpx, op_lda);
}

void lda_absolute_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_abs, op_lda);
}

void lda_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_absx, op_lda);
}

void lda_absolute_y_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_absy, op_lda);
}

void lda_indirect_x_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_zpx_ind, op_lda);
}

void lda_indirect_y_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_zp_ind_y, op_lda);
}

// LDX - Load X Register
void ldx_immediate_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_imm, op_ldx);
}

void ldx_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_zp, op_ldx);
}

void ldx_zero_page_y_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_zpy, op_ldx);
}

void ldx_absolute_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_abs, op_ldx);
}

void ldx_absolute_y_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_absy, op_ldx);
}

// LDY - Load Y Register
void ldy_immediate_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_imm, op_ldy);
}

void ldy_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_zp, op_ldy);
}

void ldy_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_zpx, op_ldy);
}

void ldy_absolute_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_abs, op_ldy);
}

void ldy_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_load_helper(cpu_dev, addr_absx, op_ldy);
}

// STA - Store Accumulator
void sta_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_zp_store, cpu_dev->a);
}

void sta_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_zpx_store, cpu_dev->a);
}

void sta_absolute_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_abs_store, cpu_dev->a);
}

void sta_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_absx_store, cpu_dev->a);
}

void sta_absolute_y_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_absy_store, cpu_dev->a);
}

void sta_indirect_x_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_zpx_ind_store, cpu_dev->a);
}

void sta_indirect_y_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_zp_ind_y_store, cpu_dev->a);
}

// STX - Store X Register
void stx_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_zp_store, cpu_dev->x);
}

void stx_zero_page_y_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_zpy_store, cpu_dev->x);
}

void stx_absolute_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_abs_store, cpu_dev->x);
}

// STY - Store Y Register
void sty_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_zp_store, cpu_dev->y);
}

void sty_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_zpx_store, cpu_dev->y);
}

void sty_absolute_func(mos6510_t* cpu_dev) {
    mos6510_store_helper(cpu_dev, addr_abs_store, cpu_dev->y);
}
