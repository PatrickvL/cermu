#include "mos6510.h"

// ============================================================================
// MOS 6510 ARITHMETIC AND LOGIC INSTRUCTIONS
// ============================================================================

// ADC - Add with Carry
void adc_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_adc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void adc_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_adc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void adc_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_adc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void adc_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_adc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void adc_absolute_x_func(mos6510_t* cpu_dev) {
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
    op_adc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void adc_absolute_y_func(mos6510_t* cpu_dev) {
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
    op_adc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void adc_indirect_x_func(mos6510_t* cpu_dev) {
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
    op_adc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void adc_indirect_y_func(mos6510_t* cpu_dev) {
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
    op_adc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// SBC - Subtract with Carry
void sbc_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_sbc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sbc_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_sbc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sbc_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_sbc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sbc_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_sbc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sbc_absolute_x_func(mos6510_t* cpu_dev) {
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
    op_sbc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sbc_absolute_y_func(mos6510_t* cpu_dev) {
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
    op_sbc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sbc_indirect_x_func(mos6510_t* cpu_dev) {
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
    op_sbc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sbc_indirect_y_func(mos6510_t* cpu_dev) {
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
    op_sbc(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// AND - Logical AND
void and_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_and(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void and_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_and(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void and_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_and(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void and_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_and(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void and_absolute_x_func(mos6510_t* cpu_dev) {
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
    op_and(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void and_absolute_y_func(mos6510_t* cpu_dev) {
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
    op_and(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void and_indirect_x_func(mos6510_t* cpu_dev) {
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
    op_and(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void and_indirect_y_func(mos6510_t* cpu_dev) {
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
    op_and(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// ORA - Logical OR
void ora_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_ora(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ora_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ora(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ora_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ora(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ora_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_ora(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ora_absolute_x_func(mos6510_t* cpu_dev) {
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
    op_ora(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ora_absolute_y_func(mos6510_t* cpu_dev) {
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
    op_ora(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ora_indirect_x_func(mos6510_t* cpu_dev) {
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
    op_ora(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ora_indirect_y_func(mos6510_t* cpu_dev) {
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
    op_ora(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// EOR - Exclusive OR
void eor_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_eor(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void eor_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_eor(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void eor_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_eor(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void eor_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_eor(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void eor_absolute_x_func(mos6510_t* cpu_dev) {
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
    op_eor(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void eor_absolute_y_func(mos6510_t* cpu_dev) {
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
    op_eor(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void eor_indirect_x_func(mos6510_t* cpu_dev) {
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
    op_eor(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void eor_indirect_y_func(mos6510_t* cpu_dev) {
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
    op_eor(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// CMP - Compare Accumulator
void cmp_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_cmp(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cmp_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_cmp(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cmp_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_cmp(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cmp_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_cmp(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cmp_absolute_x_func(mos6510_t* cpu_dev) {
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
    op_cmp(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cmp_absolute_y_func(mos6510_t* cpu_dev) {
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
    op_cmp(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cmp_indirect_x_func(mos6510_t* cpu_dev) {
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
    op_cmp(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cmp_indirect_y_func(mos6510_t* cpu_dev) {
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
    op_cmp(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// CPX - Compare X Register
void cpx_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_cpx(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cpx_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_cpx(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cpx_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_cpx(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// CPY - Compare Y Register  
void cpy_immediate_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    op_cpy(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cpy_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_cpy(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cpy_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_cpy(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// BIT - Bit Test
void bit_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_bit(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void bit_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t data = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    op_bit(cpu_dev, data);
    CPU_OPCODE_FOOTER(cpu_dev);
}
