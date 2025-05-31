#include "cpu6510.h"

// ============================================================================
// MOS 6510 INCREMENT/DECREMENT INSTRUCTIONS
// ============================================================================

// INX - Increment X Register (0xE8)
void inx_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x++;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

// INY - Increment Y Register (0xC8)
void iny_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->y++;
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}

// DEX - Decrement X Register (0xCA)
void dex_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x--;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev);
}

// DEY - Decrement Y Register (0x88)
void dey_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->y--;
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev);
}

// INC - Increment Memory
void inc_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value);
    NEXT_INSTRUCTION(cpu_dev);
}

void inc_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value);
    NEXT_INSTRUCTION(cpu_dev);
}

void inc_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value);
    NEXT_INSTRUCTION(cpu_dev);
}

void inc_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x); // Always extra cycle for RMW
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->x, value); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->x, value);
    NEXT_INSTRUCTION(cpu_dev);
}

// DEC - Decrement Memory
void dec_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value);
    NEXT_INSTRUCTION(cpu_dev);
}

void dec_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value);
    NEXT_INSTRUCTION(cpu_dev);
}

void dec_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value);
    NEXT_INSTRUCTION(cpu_dev);
}

void dec_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x); // Always extra cycle for RMW
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->x, value); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->x, value);
    NEXT_INSTRUCTION(cpu_dev);
}
