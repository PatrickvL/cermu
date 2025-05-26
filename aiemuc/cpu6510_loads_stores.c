#include "cpu6510.h"
#include "bus.h"
#include "c64.h"

// ============================================================================
// MOS 6510 LOAD AND STORE INSTRUCTIONS
// ============================================================================

// LDA - Load Accumulator
void lda_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_imm_wait);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_imm_fetch_wait);
}

void lda_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, lda_zp_wait2);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_zp_fetch_wait);
}

void lda_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, lda_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, lda_zp_x_wait3);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_zp_x_fetch_wait);
}

void lda_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, lda_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, lda_abs_wait3);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_abs_fetch_wait);
}

void lda_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, lda_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), lda_abs_x_wait3);
    }
    
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, lda_abs_x_wait4);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_abs_x_fetch_wait);
}

void lda_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, lda_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), lda_abs_y_wait3);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, lda_abs_y_wait4);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_abs_y_fetch_wait);
}

void lda_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_ind_x_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, lda_ind_x_wait2); // Dummy read
    cpu.temp = (cpu.temp + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.temp, lda_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, lda_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, lda_ind_x_wait5);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_ind_x_fetch_wait);
}

void lda_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_ind_y_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, lda_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, lda_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), lda_ind_y_wait4);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, lda_ind_y_wait5);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_ind_y_fetch_wait);
}

// LDX - Load X Register
void ldx_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ldx_imm_wait);
    op_ldx(bus_state.data);
    NEXT_INSTRUCTION(ldx_imm_fetch_wait);
}

void ldx_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ldx_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, ldx_zp_wait2);
    op_ldx(bus_state.data);
    NEXT_INSTRUCTION(ldx_zp_fetch_wait);
}

void ldx_zero_page_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ldx_zp_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, ldx_zp_y_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.y) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, ldx_zp_y_wait3);
    op_ldx(bus_state.data);
    NEXT_INSTRUCTION(ldx_zp_y_fetch_wait);
}

void ldx_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ldx_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, ldx_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, ldx_abs_wait3);
    op_ldx(bus_state.data);
    NEXT_INSTRUCTION(ldx_abs_fetch_wait);
}

void ldx_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ldx_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, ldx_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), ldx_abs_y_wait3);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, ldx_abs_y_wait4);
    op_ldx(bus_state.data);
    NEXT_INSTRUCTION(ldx_abs_y_fetch_wait);
}

// LDY - Load Y Register
void ldy_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ldy_imm_wait);
    op_ldy(bus_state.data);
    NEXT_INSTRUCTION(ldy_imm_fetch_wait);
}

void ldy_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ldy_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, ldy_zp_wait2);
    op_ldy(bus_state.data);
    NEXT_INSTRUCTION(ldy_zp_fetch_wait);
}

void ldy_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ldy_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, ldy_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, ldy_zp_x_wait3);
    op_ldy(bus_state.data);
    NEXT_INSTRUCTION(ldy_zp_x_fetch_wait);
}

void ldy_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ldy_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, ldy_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, ldy_abs_wait3);
    op_ldy(bus_state.data);
    NEXT_INSTRUCTION(ldy_abs_fetch_wait);
}

void ldy_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ldy_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, ldy_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), ldy_abs_x_wait3);
    }
    
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, ldy_abs_x_wait4);
    op_ldy(bus_state.data);
    NEXT_INSTRUCTION(ldy_abs_x_fetch_wait);
}

// STA - Store Accumulator
void sta_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sta_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_zp_wait2);
    NEXT_INSTRUCTION(sta_zp_fetch_wait);
}

void sta_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sta_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, sta_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_zp_x_wait3);
    NEXT_INSTRUCTION(sta_zp_x_fetch_wait);
}

void sta_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sta_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sta_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_abs_wait3);
    NEXT_INSTRUCTION(sta_abs_fetch_wait);
}

void sta_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sta_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sta_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), sta_abs_x_wait3); // Dummy read
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_abs_x_wait4);
    NEXT_INSTRUCTION(sta_abs_x_fetch_wait);
}

void sta_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sta_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sta_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), sta_abs_y_wait3); // Dummy read
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_abs_y_wait4);
    NEXT_INSTRUCTION(sta_abs_y_fetch_wait);
}

void sta_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sta_ind_x_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, sta_ind_x_wait2); // Dummy read
    cpu.temp = (cpu.temp + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.temp, sta_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, sta_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_ind_x_wait5);
    NEXT_INSTRUCTION(sta_ind_x_fetch_wait);
}

void sta_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sta_ind_y_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, sta_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, sta_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), sta_ind_y_wait4); // Dummy read
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_ind_y_wait5);
    NEXT_INSTRUCTION(sta_ind_y_fetch_wait);
}

// STX - Store X Register
void stx_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, stx_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.x, stx_zp_wait2);
    NEXT_INSTRUCTION(stx_zp_fetch_wait);
}

void stx_zero_page_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, stx_zp_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, stx_zp_y_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.y) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.x, stx_zp_y_wait3);
    NEXT_INSTRUCTION(stx_zp_y_fetch_wait);
}

void stx_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, stx_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, stx_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.x, stx_abs_wait3);
    NEXT_INSTRUCTION(stx_abs_fetch_wait);
}

// STY - Store Y Register
void sty_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sty_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.y, sty_zp_wait2);
    NEXT_INSTRUCTION(sty_zp_fetch_wait);
}

void sty_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sty_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, sty_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.y, sty_zp_x_wait3);
    NEXT_INSTRUCTION(sty_zp_x_fetch_wait);
}

void sty_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sty_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sty_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.y, sty_abs_wait3);
    NEXT_INSTRUCTION(sty_abs_fetch_wait);
}