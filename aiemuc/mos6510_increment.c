#include "mos6510.h"

// ============================================================================
// MOS 6510 INCREMENT/DECREMENT INSTRUCTIONS
// ============================================================================

// INX - Increment X Register (0xE8)
void inx_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x++;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// INY - Increment Y Register (0xC8)
void iny_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->y++;
    cpu_set_zn(cpu_dev, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// DEX - Decrement X Register (0xCA)
void dex_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x--;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// DEY - Decrement Y Register (0x88)
void dey_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->y--;
    cpu_set_zn(cpu_dev, cpu_dev->y);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// INC - Increment Memory
void inc_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void inc_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void inc_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void inc_absolute_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x); // Always extra cycle for RMW
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address + cpu_dev->x, value); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address + cpu_dev->x, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// DEC - Decrement Memory
void dec_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void dec_zero_page_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void dec_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void dec_absolute_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x); // Always extra cycle for RMW
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address + cpu_dev->x, value); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address + cpu_dev->x, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}
