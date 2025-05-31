#include "cpu6510.h"

// ============================================================================
// MOS 6510 MEMORY INSTRUCTIONS
// ============================================================================
// Load and Store instructions for A, X, Y registers

// LDA - Load Accumulator
void lda_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_imm(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lda_imm_fetch_wait);
}

void lda_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_zp(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lda_zp_fetch_wait);
}

void lda_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_zpx(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lda_zpx_fetch_wait);
}

void lda_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_abs(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lda_abs_fetch_wait);
}

void lda_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_absx(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lda_absx_fetch_wait);
}

void lda_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_absy(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lda_absy_fetch_wait);
}

void lda_indirect_x_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_zpx_ind(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lda_izx_fetch_wait);
}

void lda_indirect_y_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->a = addr_zp_ind_y(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lda_izy_fetch_wait);
}

// LDX - Load X Register
void ldx_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->x = addr_imm(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, ldx_imm_fetch_wait);
}

void ldx_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->x = addr_zp(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, ldx_zp_fetch_wait);
}

void ldx_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->x = addr_zpy(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, ldx_zpy_fetch_wait);
}

void ldx_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->x = addr_abs(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, ldx_abs_fetch_wait);
}

void ldx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->x = addr_absy(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, ldx_absy_fetch_wait);
}

// LDY - Load Y Register
void ldy_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->y = addr_imm(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, ldy_imm_fetch_wait);
}

void ldy_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->y = addr_zp(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, ldy_zp_fetch_wait);
}

void ldy_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->y = addr_zpx(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, ldy_zpx_fetch_wait);
}

void ldy_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->y = addr_abs(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, ldy_abs_fetch_wait);
}

void ldy_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->y = addr_absx(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, ldy_absx_fetch_wait);
}

// STA - Store Accumulator
void sta_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_zp_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, sta_zp_fetch_wait);
}

void sta_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_zpx_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sta_zpx_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_zpx_wait3);
    NEXT_INSTRUCTION(cpu_dev, sta_zpx_fetch_wait);
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
    CPU_READY_OR_STALL(cpu_dev, sta_absx_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sta_absx_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    // Always perform dummy read for indexed absolute stores
    CPU_READY_OR_STALL(cpu_dev, sta_absx_wait3);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x);  // Dummy read
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->x, cpu_dev->a, sta_absx_wait4);
    NEXT_INSTRUCTION(cpu_dev, sta_absx_fetch_wait);
}

void sta_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_absy_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sta_absy_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    // Always perform dummy read for indexed absolute stores
    CPU_READY_OR_STALL(cpu_dev, sta_absy_wait3);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->y);  // Dummy read
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->y, cpu_dev->a, sta_absy_wait4);
    NEXT_INSTRUCTION(cpu_dev, sta_absy_fetch_wait);
}

void sta_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_izx_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sta_izx_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, sta_izx_wait3);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->address);
    CPU_READY_OR_STALL(cpu_dev, sta_izx_wait4);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_izx_wait5);
    NEXT_INSTRUCTION(cpu_dev, sta_izx_fetch_wait);
}

void sta_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sta_izy_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sta_izy_wait2);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->address);
    CPU_READY_OR_STALL(cpu_dev, sta_izy_wait3);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    // Always perform dummy read for indexed indirect stores
    CPU_READY_OR_STALL(cpu_dev, sta_izy_wait4);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a, sta_izy_wait5);
    NEXT_INSTRUCTION(cpu_dev, sta_izy_fetch_wait);
}

// STX - Store X Register
void stx_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, stx_zp_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->x, stx_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, stx_zp_fetch_wait);
}

void stx_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, stx_zpy_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, stx_zpy_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->y) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->x, stx_zpy_wait3);
    NEXT_INSTRUCTION(cpu_dev, stx_zpy_fetch_wait);
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
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->y, sty_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, sty_zp_fetch_wait);
}

void sty_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sty_zpx_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sty_zpx_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->y, sty_zpx_wait3);
    NEXT_INSTRUCTION(cpu_dev, sty_zpx_fetch_wait);
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