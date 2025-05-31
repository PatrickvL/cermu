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
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, inc_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, inc_zp_wait3); // Dummy write
    cpu_dev->temp++;
    cpu_set_zn(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, inc_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, inc_zp_fetch_wait);
}

void inc_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, inc_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, inc_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, inc_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, inc_zp_x_wait4); // Dummy write
    cpu_dev->temp++;
    cpu_set_zn(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, inc_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, inc_zp_x_fetch_wait);
}

void inc_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, inc_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, inc_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, inc_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, inc_abs_wait4); // Dummy write
    cpu_dev->temp++;
    cpu_set_zn(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, inc_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, inc_abs_fetch_wait);
}

void inc_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, inc_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, inc_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, inc_abs_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs + cpu_dev->x); // Always extra cycle for RMW
    CPU_READY_OR_STALL(cpu_dev, inc_abs_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs + cpu_dev->x);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, inc_abs_x_wait5); // Dummy write
    cpu_dev->temp++;
    cpu_set_zn(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, inc_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, inc_abs_x_fetch_wait);
}

// DEC - Decrement Memory
void dec_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dec_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, dec_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dec_zp_wait3); // Dummy write
    cpu_dev->temp--;
    cpu_set_zn(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dec_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, dec_zp_fetch_wait);
}

void dec_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dec_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, dec_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, dec_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dec_zp_x_wait4); // Dummy write
    cpu_dev->temp--;
    cpu_set_zn(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dec_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, dec_zp_x_fetch_wait);
}

void dec_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dec_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, dec_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, dec_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dec_abs_wait4); // Dummy write
    cpu_dev->temp--;
    cpu_set_zn(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dec_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, dec_abs_fetch_wait);
}

void dec_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dec_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, dec_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, dec_abs_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs + cpu_dev->x); // Always extra cycle for RMW
    CPU_READY_OR_STALL(cpu_dev, dec_abs_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs + cpu_dev->x);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, dec_abs_x_wait5); // Dummy write
    cpu_dev->temp--;
    cpu_set_zn(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, dec_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, dec_abs_x_fetch_wait);
}
