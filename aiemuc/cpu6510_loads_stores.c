#include "cpu6510.h"

// ============================================================================
// MOS 6510 LOAD AND STORE INSTRUCTIONS
// ============================================================================

// LDA - Load Accumulator
void lda_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t zp_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, zp_addr); // Dummy read
    zp_addr = (zp_addr + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t zp_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

// LDX - Load X Register
void ldx_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_ldx(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldx_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldx_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->y) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldx_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

// LDY - Load Y Register
void ldy_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_ldy(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldy_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldy_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldy_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldy_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t data = cpu_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    NEXT_INSTRUCTION(cpu_dev);
}

// STA - Store Accumulator
void sta_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void sta_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void sta_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void sta_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->x;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void sta_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->y;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void sta_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t zp_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, zp_addr); // Dummy read
    zp_addr = (zp_addr + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void sta_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t zp_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->y;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

// STX - Store X Register
void stx_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

void stx_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->y) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

void stx_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

// STY - Store Y Register
void sty_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}

void sty_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}

void sty_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}
