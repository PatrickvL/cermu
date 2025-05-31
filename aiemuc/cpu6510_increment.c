#include "cpu6510.h"

// ============================================================================
// MOS 6510 INCREMENT/DECREMENT INSTRUCTIONS
// ============================================================================

// INX - Increment X Register (0xE8)
void inx_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, inx_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x++;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, inx_fetch_wait);
}

// INY - Increment Y Register (0xC8)
void iny_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, iny_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->y++;
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, iny_fetch_wait);
}

// DEX - Decrement X Register (0xCA)
void dex_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dex_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x--;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, dex_fetch_wait);
}

// DEY - Decrement Y Register (0x88)
void dey_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dey_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->y--;
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, dey_fetch_wait);
}

// INC - Increment Memory
void inc_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, inc_zp_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, inc_zp_wait2);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, inc_zp_wait3); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, inc_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, inc_zp_fetch_wait);
}

void inc_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, inc_zp_x_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, inc_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, inc_zp_x_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, inc_zp_x_wait4); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, inc_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, inc_zp_x_fetch_wait);
}

void inc_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, inc_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, inc_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev, inc_abs_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, inc_abs_wait4); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, inc_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, inc_abs_fetch_wait);
}

void inc_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, inc_abs_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, inc_abs_x_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, inc_abs_x_wait3);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x); // Always extra cycle for RMW
    CPU_READY_OR_STALL(cpu_dev, inc_abs_x_wait4);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->x, value, inc_abs_x_wait5); // Dummy write
    value++;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->x, value, inc_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, inc_abs_x_fetch_wait);
}

// DEC - Decrement Memory
void dec_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dec_zp_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, dec_zp_wait2);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dec_zp_wait3); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dec_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, dec_zp_fetch_wait);
}

void dec_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dec_zp_x_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, dec_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, dec_zp_x_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dec_zp_x_wait4); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dec_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, dec_zp_x_fetch_wait);
}

void dec_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dec_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, dec_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev, dec_abs_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dec_abs_wait4); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dec_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, dec_abs_fetch_wait);
}

void dec_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dec_abs_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, dec_abs_x_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, dec_abs_x_wait3);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x); // Always extra cycle for RMW
    CPU_READY_OR_STALL(cpu_dev, dec_abs_x_wait4);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->x, value, dec_abs_x_wait5); // Dummy write
    value--;
    cpu_set_zn(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address + cpu_dev->x, value, dec_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, dec_abs_x_fetch_wait);
}
