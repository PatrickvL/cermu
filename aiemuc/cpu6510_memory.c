#include "cpu6510.h"

// ============================================================================
// MOS 6510 MEMORY INSTRUCTIONS
// ============================================================================
// Load and Store instructions for A, X, Y registers

// LDA - Load Accumulator
void lda_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_imm(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_zp(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_zpx(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_abs(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_absx(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_absy(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_indirect_x_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_zpx_ind(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void lda_indirect_y_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_zp_ind_y(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

// LDX - Load X Register
void ldx_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->x = addr_imm(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldx_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->x = addr_zp(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldx_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->x = addr_zpy(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldx_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->x = addr_abs(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->x = addr_absy(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

// LDY - Load Y Register
void ldy_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->y = addr_imm(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldy_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->y = addr_zp(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldy_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->y = addr_zpx(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldy_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->y = addr_abs(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}

void ldy_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->y = addr_absx(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}

// STA - Store Accumulator
void sta_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void sta_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
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
    // Always perform dummy read for indexed absolute stores
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x);  // Dummy read
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->x, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void sta_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    // Always perform dummy read for indexed absolute stores
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->y);  // Dummy read
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->y, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void sta_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->address);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

void sta_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->address);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    // Always perform dummy read for indexed indirect stores
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev);
}

// STX - Store X Register
void stx_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

void stx_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
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
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}

void sty_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
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