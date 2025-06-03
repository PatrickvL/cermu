#include "mos6510.h"

// ============================================================================
// MOS 6510 MEMORY INSTRUCTIONS
// ============================================================================
// Load and Store instructions for A, X, Y registers

// LDA - Load Accumulator
void lda_immediate_func(mos6510_t* cpu_dev) {
    cpu_dev->a = addr_imm(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_zero_page_func(mos6510_t* cpu_dev) {
    cpu_dev->a = addr_zp(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_zero_page_x_func(mos6510_t* cpu_dev) {
    cpu_dev->a = addr_zpx(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_absolute_func(mos6510_t* cpu_dev) {
    cpu_dev->a = addr_abs(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_absolute_x_func(mos6510_t* cpu_dev) {
    cpu_dev->a = addr_absx(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_absolute_y_func(mos6510_t* cpu_dev) {
    cpu_dev->a = addr_absy(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_indirect_x_func(mos6510_t* cpu_dev) {
    cpu_dev->a = addr_zpx_ind(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_indirect_y_func(mos6510_t* cpu_dev) {
    cpu_dev->a = addr_zp_ind_y(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// LDX - Load X Register
void ldx_immediate_func(mos6510_t* cpu_dev) {
    cpu_dev->x = addr_imm(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldx_zero_page_func(mos6510_t* cpu_dev) {
    cpu_dev->x = addr_zp(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldx_zero_page_y_func(mos6510_t* cpu_dev) {
    cpu_dev->x = addr_zpy(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldx_absolute_func(mos6510_t* cpu_dev) {
    cpu_dev->x = addr_abs(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldx_absolute_y_func(mos6510_t* cpu_dev) {
    cpu_dev->x = addr_absy(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// LDY - Load Y Register
void ldy_immediate_func(mos6510_t* cpu_dev) {
    cpu_dev->y = addr_imm(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldy_zero_page_func(mos6510_t* cpu_dev) {
    cpu_dev->y = addr_zp(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldy_zero_page_x_func(mos6510_t* cpu_dev) {
    cpu_dev->y = addr_zpx(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldy_absolute_func(mos6510_t* cpu_dev) {
    cpu_dev->y = addr_abs(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldy_absolute_x_func(mos6510_t* cpu_dev) {
    cpu_dev->y = addr_absx(cpu_dev);
    mos6510_set_zn(cpu_dev, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// STA - Store Accumulator
void sta_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sta_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sta_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sta_absolute_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    // Always perform dummy read for indexed absolute stores
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x);  // Dummy read
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address + cpu_dev->x, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sta_absolute_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    // Always perform dummy read for indexed absolute stores
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->y);  // Dummy read
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address + cpu_dev->y, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sta_indirect_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sta_indirect_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    // Always perform dummy read for indexed indirect stores
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// STX - Store X Register
void stx_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void stx_zero_page_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->y) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void stx_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// STY - Store Y Register
void sty_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sty_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sty_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}