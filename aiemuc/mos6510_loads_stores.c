#include "mos6510.h"

// ============================================================================
// MOS 6510 LOAD AND STORE INSTRUCTIONS
// ============================================================================

// LDA - Load Accumulator
void lda_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_lda(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_absolute_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->x;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_absolute_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->y;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_indirect_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t zp_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, zp_addr); // Dummy read
    zp_addr = (zp_addr + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, zp_addr);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lda_indirect_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t zp_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, zp_addr);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->y;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_lda(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// LDX - Load X Register
void ldx_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_ldx(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldx_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldx_zero_page_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->y) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldx_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldx_absolute_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->y;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ldx(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// LDY - Load Y Register
void ldy_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_ldy(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldy_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldy_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldy_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ldy_absolute_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    if ((cpu_dev->address & 0xFF00) != ((cpu_dev->address + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->address += cpu_dev->x;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ldy(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// STA - Store Accumulator
void sta_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sta_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
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
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->x;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sta_absolute_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->y;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sta_indirect_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t zp_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, zp_addr); // Dummy read
    zp_addr = (zp_addr + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, zp_addr);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sta_indirect_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t zp_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, zp_addr);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->y) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->y;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// STX - Store X Register
void stx_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void stx_zero_page_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
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
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sty_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
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
