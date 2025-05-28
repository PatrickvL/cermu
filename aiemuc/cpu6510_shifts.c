#include "cpu6510.h"
#include "bus.h"

// ============================================================================
// MOS 6510 SHIFT AND ROTATE INSTRUCTIONS
// ============================================================================

// ASL - Arithmetic Shift Left
void asl_accumulator_func(void) {
    // ASL A is a 2-cycle instruction
    WAIT_READY_THEN_READ(cpu.pc, asl_a_wait);  // Dummy read
    cpu.a = op_asl(cpu.a);
    NEXT_INSTRUCTION(asl_a_fetch_wait);
}

void asl_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, asl_zp_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, asl_zp_wait2);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, asl_zp_wait3); // Write original value
    cpu.temp = op_asl(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, asl_zp_wait4);
    NEXT_INSTRUCTION(asl_zp_fetch_wait);
}

void asl_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, asl_zp_x_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, asl_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, asl_zp_x_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, asl_zp_x_wait4); // Write original value
    cpu.temp = op_asl(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, asl_zp_x_wait5);
    NEXT_INSTRUCTION(asl_zp_x_fetch_wait);
}

void asl_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, asl_abs_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, asl_abs_wait2);
    cpu.addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, asl_abs_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, asl_abs_wait4); // Write original value
    cpu.temp = op_asl(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, asl_abs_wait5);
    NEXT_INSTRUCTION(asl_abs_fetch_wait);
}

void asl_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, asl_abs_x_wait1);
    cpu.lo = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, asl_abs_x_wait2);
    cpu.hi = bus.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), asl_abs_x_wait3); // Dummy read
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, asl_abs_x_wait4);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, asl_abs_x_wait5); // Write original value
    cpu.temp = op_asl(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, asl_abs_x_wait6);
    NEXT_INSTRUCTION(asl_abs_x_fetch_wait);
}

// LSR - Logical Shift Right
void lsr_accumulator_func(void) {
    // LSR A is a 2-cycle instruction
    WAIT_READY_THEN_READ(cpu.pc, lsr_a_wait);  // Dummy read
    cpu.a = op_lsr(cpu.a);
    NEXT_INSTRUCTION(lsr_a_fetch_wait);
}

void lsr_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lsr_zp_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, lsr_zp_wait2);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, lsr_zp_wait3); // Write original value
    cpu.temp = op_lsr(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, lsr_zp_wait4);
    NEXT_INSTRUCTION(lsr_zp_fetch_wait);
}

void lsr_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lsr_zp_x_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, lsr_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, lsr_zp_x_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, lsr_zp_x_wait4); // Write original value
    cpu.temp = op_lsr(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, lsr_zp_x_wait5);
    NEXT_INSTRUCTION(lsr_zp_x_fetch_wait);
}

void lsr_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lsr_abs_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, lsr_abs_wait2);
    cpu.addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, lsr_abs_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, lsr_abs_wait4); // Write original value
    cpu.temp = op_lsr(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, lsr_abs_wait5);
    NEXT_INSTRUCTION(lsr_abs_fetch_wait);
}

void lsr_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lsr_abs_x_wait1);
    cpu.lo = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, lsr_abs_x_wait2);
    cpu.hi = bus.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), lsr_abs_x_wait3); // Dummy read
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, lsr_abs_x_wait4);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, lsr_abs_x_wait5); // Write original value
    cpu.temp = op_lsr(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, lsr_abs_x_wait6);
    NEXT_INSTRUCTION(lsr_abs_x_fetch_wait);
}

// ROL - Rotate Left
void rol_accumulator_func(void) {
    // ROL A is a 2-cycle instruction
    WAIT_READY_THEN_READ(cpu.pc, rol_a_wait);  // Dummy read
    cpu.a = op_rol(cpu.a);
    NEXT_INSTRUCTION(rol_a_fetch_wait);
}

void rol_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rol_zp_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, rol_zp_wait2);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rol_zp_wait3); // Write original value
    cpu.temp = op_rol(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rol_zp_wait4);
    NEXT_INSTRUCTION(rol_zp_fetch_wait);
}

void rol_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rol_zp_x_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, rol_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, rol_zp_x_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rol_zp_x_wait4); // Write original value
    cpu.temp = op_rol(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rol_zp_x_wait5);
    NEXT_INSTRUCTION(rol_zp_x_fetch_wait);
}

void rol_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rol_abs_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, rol_abs_wait2);
    cpu.addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, rol_abs_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rol_abs_wait4); // Write original value
    cpu.temp = op_rol(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rol_abs_wait5);
    NEXT_INSTRUCTION(rol_abs_fetch_wait);
}

void rol_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rol_abs_x_wait1);
    cpu.lo = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, rol_abs_x_wait2);
    cpu.hi = bus.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), rol_abs_x_wait3); // Dummy read
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, rol_abs_x_wait4);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rol_abs_x_wait5); // Write original value
    cpu.temp = op_rol(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rol_abs_x_wait6);
    NEXT_INSTRUCTION(rol_abs_x_fetch_wait);
}

// ROR - Rotate Right
void ror_accumulator_func(void) {
    // ROR A is a 2-cycle instruction
    WAIT_READY_THEN_READ(cpu.pc, ror_a_wait);  // Dummy read
    cpu.a = op_ror(cpu.a);
    NEXT_INSTRUCTION(ror_a_fetch_wait);
}

void ror_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ror_zp_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, ror_zp_wait2);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, ror_zp_wait3); // Write original value
    cpu.temp = op_ror(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, ror_zp_wait4);
    NEXT_INSTRUCTION(ror_zp_fetch_wait);
}

void ror_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ror_zp_x_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, ror_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, ror_zp_x_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, ror_zp_x_wait4); // Write original value
    cpu.temp = op_ror(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, ror_zp_x_wait5);
    NEXT_INSTRUCTION(ror_zp_x_fetch_wait);
}

void ror_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ror_abs_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, ror_abs_wait2);
    cpu.addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, ror_abs_wait3);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, ror_abs_wait4); // Write original value
    cpu.temp = op_ror(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, ror_abs_wait5);
    NEXT_INSTRUCTION(ror_abs_fetch_wait);
}

void ror_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ror_abs_x_wait1);
    cpu.lo = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, ror_abs_x_wait2);
    cpu.hi = bus.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), ror_abs_x_wait3); // Dummy read
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, ror_abs_x_wait4);
    cpu.temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, ror_abs_x_wait5); // Write original value
    cpu.temp = op_ror(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, ror_abs_x_wait6);
    NEXT_INSTRUCTION(ror_abs_x_fetch_wait);
}
