#include "cpu6510.h"

// ============================================================================
// MOS 6510 LOAD AND STORE INSTRUCTIONS
// ============================================================================

// LDA - Load Accumulator
void lda_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_imm_wait);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, lda_imm_fetch_wait);
}

void lda_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_zp_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, lda_zp_wait2);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, lda_zp_fetch_wait);
}

void lda_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_zp_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, lda_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, lda_zp_x_wait3);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, lda_zp_x_fetch_wait);
}

void lda_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, lda_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev, lda_abs_wait3);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, lda_abs_fetch_wait);
}

void lda_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_abs_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, lda_abs_x_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, lda_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, lda_abs_x_wait4);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, lda_abs_x_fetch_wait);
}

void lda_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_abs_y_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, lda_abs_y_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, lda_abs_y_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, lda_abs_y_wait4);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, lda_abs_y_fetch_wait);
}

void lda_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_ind_x_wait1);
    uint8_t zp_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, lda_ind_x_wait2);
    (void)cpu_read_cycle(cpu_dev, zp_addr); // Dummy read
    zp_addr = (zp_addr + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, lda_ind_x_wait3);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev, lda_ind_x_wait4);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, lda_ind_x_wait5);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, lda_ind_x_fetch_wait);
}

void lda_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lda_ind_y_wait1);
    uint8_t zp_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, lda_ind_y_wait2);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev, lda_ind_y_wait3);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, lda_ind_y_wait4);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, lda_ind_y_wait5);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, lda_ind_y_fetch_wait);
}

// LDX - Load X Register
void ldx_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldx_imm_wait);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_ldx(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, ldx_imm_fetch_wait);
}

void ldx_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldx_zp_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, ldx_zp_wait2);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, ldx_zp_fetch_wait);
}

void ldx_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldx_zp_y_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, ldx_zp_y_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->y) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, ldx_zp_y_wait3);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, ldx_zp_y_fetch_wait);
}

void ldx_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_wait3);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, ldx_abs_fetch_wait);
}

void ldx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_y_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_y_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, ldx_abs_y_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, ldx_abs_y_wait4);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, ldx_abs_y_fetch_wait);
}

// LDY - Load Y Register
void ldy_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldy_imm_wait);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_ldy(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, ldy_imm_fetch_wait);
}

void ldy_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldy_zp_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, ldy_zp_wait2);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, ldy_zp_fetch_wait);
}

void ldy_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldy_zp_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, ldy_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, ldy_zp_x_wait3);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, ldy_zp_x_fetch_wait);
}

void ldy_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_wait3);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, ldy_abs_fetch_wait);
}

void ldy_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_x_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, ldy_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, ldy_abs_x_wait4);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev, ldy_abs_x_fetch_wait);
}

// STA - Store Accumulator
void sta_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_zp_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, sta_zp_fetch_wait);
}

void sta_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_zp_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, sta_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_zp_x_wait3);
    NEXT_INSTRUCTION(cpu_dev, sta_zp_x_fetch_wait);
}

void sta_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, sta_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_abs_wait3);
    NEXT_INSTRUCTION(cpu_dev, sta_abs_fetch_wait);
}

void sta_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_abs_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sta_abs_x_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, sta_abs_x_wait3);
    (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->x;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_abs_x_wait4);
    NEXT_INSTRUCTION(cpu_dev, sta_abs_x_fetch_wait);
}

void sta_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_abs_y_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sta_abs_y_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, sta_abs_y_wait3);
    (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->y;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_abs_y_wait4);
    NEXT_INSTRUCTION(cpu_dev, sta_abs_y_fetch_wait);
}

void sta_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_ind_x_wait1);
    uint8_t zp_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sta_ind_x_wait2);
    (void)cpu_read_cycle(cpu_dev, zp_addr); // Dummy read
    zp_addr = (zp_addr + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, sta_ind_x_wait3);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev, sta_ind_x_wait4);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_ind_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, sta_ind_x_fetch_wait);
}

void sta_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_ind_y_wait1);
    uint8_t zp_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sta_ind_y_wait2);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev, sta_ind_y_wait3);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, sta_ind_y_wait4);
    (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->y;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_ind_y_wait5);
    NEXT_INSTRUCTION(cpu_dev, sta_ind_y_fetch_wait);
}

// STX - Store X Register
void stx_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, stx_zp_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->x, stx_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, stx_zp_fetch_wait);
}

void stx_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, stx_zp_y_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, stx_zp_y_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->y) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->x, stx_zp_y_wait3);
    NEXT_INSTRUCTION(cpu_dev, stx_zp_y_fetch_wait);
}

void stx_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, stx_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, stx_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->x, stx_abs_wait3);
    NEXT_INSTRUCTION(cpu_dev, stx_abs_fetch_wait);
}

// STY - Store Y Register
void sty_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sty_zp_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->y, sty_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, sty_zp_fetch_wait);
}

void sty_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sty_zp_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, sty_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->y, sty_zp_x_wait3);
    NEXT_INSTRUCTION(cpu_dev, sty_zp_x_fetch_wait);
}

void sty_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sty_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, sty_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->y, sty_abs_wait3);
    NEXT_INSTRUCTION(cpu_dev, sty_abs_fetch_wait);
}
