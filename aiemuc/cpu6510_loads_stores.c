#include "cpu6510.h"

// ============================================================================
// MOS 6510 LOAD AND STORE INSTRUCTIONS
// ============================================================================

// LDA - Load Accumulator
void lda_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_lda(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, lda_imm_fetch_wait);
}

void lda_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lda_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_lda(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, lda_zp_fetch_wait);
}

void lda_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lda_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, lda_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_lda(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, lda_zp_x_fetch_wait);
}

void lda_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lda_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, lda_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_lda(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, lda_abs_fetch_wait);
}

void lda_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lda_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, lda_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, lda_abs_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_lda(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, lda_abs_x_fetch_wait);
}

void lda_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_abs_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lda_abs_y_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, lda_abs_y_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, lda_abs_y_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_lda(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, lda_abs_y_fetch_wait);
}

void lda_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_ind_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lda_ind_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp); // Dummy read
    cpu_dev->temp = (cpu_dev->temp + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, lda_ind_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lda_ind_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, lda_ind_x_wait5);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_lda(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, lda_ind_x_fetch_wait);
}

void lda_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_ind_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lda_ind_y_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lda_ind_y_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, lda_ind_y_wait4);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, lda_ind_y_wait5);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_lda(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, lda_ind_y_fetch_wait);
}

// LDX - Load X Register
void ldx_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldx_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_ldx(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ldx_imm_fetch_wait);
}

void ldx_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldx_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ldx_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ldx(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ldx_zp_fetch_wait);
}

void ldx_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldx_zp_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ldx_zp_y_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->y) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, ldx_zp_y_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ldx(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ldx_zp_y_fetch_wait);
}

void ldx_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ldx(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ldx_abs_fetch_wait);
}

void ldx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_y_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, ldx_abs_y_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_y_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ldx(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ldx_abs_y_fetch_wait);
}

// LDY - Load Y Register
void ldy_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldy_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_ldy(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ldy_imm_fetch_wait);
}

void ldy_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldy_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ldy_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ldy(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ldy_zp_fetch_wait);
}

void ldy_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldy_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ldy_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, ldy_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ldy(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ldy_zp_x_fetch_wait);
}

void ldy_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ldy(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ldy_abs_fetch_wait);
}

void ldy_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, ldy_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ldy(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ldy_abs_x_fetch_wait);
}

// STA - Store Accumulator
void sta_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a, sta_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, sta_zp_fetch_wait);
}

void sta_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sta_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a, sta_zp_x_wait3);
    NEXT_INSTRUCTION(cpu_dev, sta_zp_x_fetch_wait);
}

void sta_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sta_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a, sta_abs_wait3);
    NEXT_INSTRUCTION(cpu_dev, sta_abs_fetch_wait);
}

void sta_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sta_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, sta_abs_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->addr_abs += cpu_dev->x;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a, sta_abs_x_wait4);
    NEXT_INSTRUCTION(cpu_dev, sta_abs_x_fetch_wait);
}

void sta_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_abs_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sta_abs_y_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, sta_abs_y_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF)); // Dummy read
    cpu_dev->addr_abs += cpu_dev->y;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a, sta_abs_y_wait4);
    NEXT_INSTRUCTION(cpu_dev, sta_abs_y_fetch_wait);
}

void sta_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_ind_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sta_ind_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp); // Dummy read
    cpu_dev->temp = (cpu_dev->temp + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, sta_ind_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sta_ind_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a, sta_ind_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, sta_ind_x_fetch_wait);
}

void sta_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_ind_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sta_ind_y_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sta_ind_y_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, sta_ind_y_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF)); // Dummy read
    cpu_dev->addr_abs += cpu_dev->y;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a, sta_ind_y_wait5);
    NEXT_INSTRUCTION(cpu_dev, sta_ind_y_fetch_wait);
}

// STX - Store X Register
void stx_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, stx_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->x, stx_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, stx_zp_fetch_wait);
}

void stx_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, stx_zp_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, stx_zp_y_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->y) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->x, stx_zp_y_wait3);
    NEXT_INSTRUCTION(cpu_dev, stx_zp_y_fetch_wait);
}

void stx_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, stx_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, stx_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->x, stx_abs_wait3);
    NEXT_INSTRUCTION(cpu_dev, stx_abs_fetch_wait);
}

// STY - Store Y Register
void sty_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sty_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->y, sty_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, sty_zp_fetch_wait);
}

void sty_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sty_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sty_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->y, sty_zp_x_wait3);
    NEXT_INSTRUCTION(cpu_dev, sty_zp_x_fetch_wait);
}

void sty_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sty_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sty_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->y, sty_abs_wait3);
    NEXT_INSTRUCTION(cpu_dev, sty_abs_fetch_wait);
}
