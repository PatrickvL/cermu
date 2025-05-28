#include "cpu6510.h"
#include "bus.h"
#include "c64.h"

// ============================================================================
// MOS 6510 INCREMENT/DECREMENT INSTRUCTIONS
// ============================================================================

// INX - Increment X Register (0xE8)
void inx_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, inx_wait);  // Dummy read
    cpu.x++;
    cpu_set_zn(cpu.x);
    NEXT_INSTRUCTION(inx_fetch_wait);
}

// INY - Increment Y Register (0xC8)
void iny_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, iny_wait);  // Dummy read
    cpu.y++;
    cpu_set_zn(cpu.y);
    NEXT_INSTRUCTION(iny_fetch_wait);
}

// DEX - Decrement X Register (0xCA)
void dex_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, dex_wait);  // Dummy read
    cpu.x--;
    cpu_set_zn(cpu.x);
    NEXT_INSTRUCTION(dex_fetch_wait);
}

// DEY - Decrement Y Register (0x88)
void dey_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, dey_wait);  // Dummy read
    cpu.y--;
    cpu_set_zn(cpu.y);
    NEXT_INSTRUCTION(dey_fetch_wait);
}

// INC - Increment Memory
void inc_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, inc_zp_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, inc_zp_wait2);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, inc_zp_wait3); // Dummy write
    cpu.temp++;
    cpu_set_zn(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, inc_zp_wait4);
    NEXT_INSTRUCTION(inc_zp_fetch_wait);
}

void inc_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, inc_zp_x_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, inc_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, inc_zp_x_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, inc_zp_x_wait4); // Dummy write
    cpu.temp++;
    cpu_set_zn(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, inc_zp_x_wait5);
    NEXT_INSTRUCTION(inc_zp_x_fetch_wait);
}

void inc_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, inc_abs_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, inc_abs_wait2);
    cpu.addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, inc_abs_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, inc_abs_wait4); // Dummy write
    cpu.temp++;
    cpu_set_zn(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, inc_abs_wait5);
    NEXT_INSTRUCTION(inc_abs_fetch_wait);
}

void inc_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, inc_abs_x_wait1);
    cpu.lo = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, inc_abs_x_wait2);
    cpu.hi = bus.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, inc_abs_x_wait3); // Always extra cycle for RMW
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, inc_abs_x_wait4);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, inc_abs_x_wait5); // Dummy write
    cpu.temp++;
    cpu_set_zn(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, inc_abs_x_wait6);
    NEXT_INSTRUCTION(inc_abs_x_fetch_wait);
}

// DEC - Decrement Memory
void dec_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dec_zp_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, dec_zp_wait2);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dec_zp_wait3); // Dummy write
    cpu.temp--;
    cpu_set_zn(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dec_zp_wait4);
    NEXT_INSTRUCTION(dec_zp_fetch_wait);
}

void dec_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dec_zp_x_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, dec_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, dec_zp_x_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dec_zp_x_wait4); // Dummy write
    cpu.temp--;
    cpu_set_zn(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dec_zp_x_wait5);
    NEXT_INSTRUCTION(dec_zp_x_fetch_wait);
}

void dec_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dec_abs_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, dec_abs_wait2);
    cpu.addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, dec_abs_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dec_abs_wait4); // Dummy write
    cpu.temp--;
    cpu_set_zn(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dec_abs_wait5);
    NEXT_INSTRUCTION(dec_abs_fetch_wait);
}

void dec_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dec_abs_x_wait1);
    cpu.lo = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, dec_abs_x_wait2);
    cpu.hi = bus.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, dec_abs_x_wait3); // Always extra cycle for RMW
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, dec_abs_x_wait4);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, dec_abs_x_wait5); // Dummy write
    cpu.temp--;
    cpu_set_zn(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, dec_abs_x_wait6);
    NEXT_INSTRUCTION(dec_abs_x_fetch_wait);
}
