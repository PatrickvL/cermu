#include "cpu6510.h"

// ============================================================================
// MOS 6510 SHIFT AND ROTATE INSTRUCTIONS
// ============================================================================

// ASL - Arithmetic Shift Left
void asl_accumulator_func(cpu6510_state_t* cpu_dev) {
    // ASL A is a 2-cycle instruction
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, asl_a_wait);  // Dummy read
    cpu_dev->a = op_asl(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, asl_a_fetch_wait);
}

void asl_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, asl_zp_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, asl_zp_wait2);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_zp_wait3); // Write original value
    cpu_dev->temp = op_asl(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, asl_zp_fetch_wait);
}

void asl_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, asl_zp_x_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, asl_zp_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, asl_zp_x_wait3);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_zp_x_wait4); // Write original value
    cpu_dev->temp = op_asl(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, asl_zp_x_fetch_wait);
}

void asl_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, asl_abs_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, asl_abs_wait2);
    cpu_dev->addr_abs |= (cpu_dev->bus->data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, asl_abs_wait3);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_abs_wait4); // Write original value
    cpu_dev->temp = op_asl(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, asl_abs_fetch_wait);
}

void asl_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, asl_abs_x_wait1);
    cpu_dev->lo = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, asl_abs_x_wait2);
    cpu_dev->hi = cpu_dev->bus->data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF), asl_abs_x_wait3); // Dummy read
    cpu_dev->addr_abs += cpu_dev->x;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, asl_abs_x_wait4);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_abs_x_wait5); // Write original value
    cpu_dev->temp = op_asl(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, asl_abs_x_fetch_wait);
}

// LSR - Logical Shift Right
void lsr_accumulator_func(cpu6510_state_t* cpu_dev) {
    // LSR A is a 2-cycle instruction
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, lsr_a_wait);  // Dummy read
    cpu_dev->a = op_lsr(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lsr_a_fetch_wait);
}

void lsr_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lsr_zp_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lsr_zp_wait2);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_zp_wait3); // Write original value
    cpu_dev->temp = op_lsr(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, lsr_zp_fetch_wait);
}

void lsr_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lsr_zp_x_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lsr_zp_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lsr_zp_x_wait3);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_zp_x_wait4); // Write original value
    cpu_dev->temp = op_lsr(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, lsr_zp_x_fetch_wait);
}

void lsr_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lsr_abs_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lsr_abs_wait2);
    cpu_dev->addr_abs |= (cpu_dev->bus->data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lsr_abs_wait3);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_abs_wait4); // Write original value
    cpu_dev->temp = op_lsr(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, lsr_abs_fetch_wait);
}

void lsr_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lsr_abs_x_wait1);
    cpu_dev->lo = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lsr_abs_x_wait2);
    cpu_dev->hi = cpu_dev->bus->data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF), lsr_abs_x_wait3); // Dummy read
    cpu_dev->addr_abs += cpu_dev->x;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lsr_abs_x_wait4);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_abs_x_wait5); // Write original value
    cpu_dev->temp = op_lsr(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, lsr_abs_x_fetch_wait);
}

// ROL - Rotate Left
void rol_accumulator_func(cpu6510_state_t* cpu_dev) {
    // ROL A is a 2-cycle instruction
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, rol_a_wait);  // Dummy read
    cpu_dev->a = op_rol(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, rol_a_fetch_wait);
}

void rol_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rol_zp_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rol_zp_wait2);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_zp_wait3); // Write original value
    cpu_dev->temp = op_rol(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, rol_zp_fetch_wait);
}

void rol_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rol_zp_x_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rol_zp_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rol_zp_x_wait3);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_zp_x_wait4); // Write original value
    cpu_dev->temp = op_rol(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, rol_zp_x_fetch_wait);
}

void rol_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rol_abs_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rol_abs_wait2);
    cpu_dev->addr_abs |= (cpu_dev->bus->data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rol_abs_wait3);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_abs_wait4); // Write original value
    cpu_dev->temp = op_rol(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, rol_abs_fetch_wait);
}

void rol_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rol_abs_x_wait1);
    cpu_dev->lo = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rol_abs_x_wait2);
    cpu_dev->hi = cpu_dev->bus->data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF), rol_abs_x_wait3); // Dummy read
    cpu_dev->addr_abs += cpu_dev->x;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rol_abs_x_wait4);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_abs_x_wait5); // Write original value
    cpu_dev->temp = op_rol(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, rol_abs_x_fetch_wait);
}

// ROR - Rotate Right
void ror_accumulator_func(cpu6510_state_t* cpu_dev) {
    // ROR A is a 2-cycle instruction
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, ror_a_wait);  // Dummy read
    cpu_dev->a = op_ror(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, ror_a_fetch_wait);
}

void ror_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, ror_zp_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, ror_zp_wait2);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_zp_wait3); // Write original value
    cpu_dev->temp = op_ror(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, ror_zp_fetch_wait);
}

void ror_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, ror_zp_x_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, ror_zp_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, ror_zp_x_wait3);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_zp_x_wait4); // Write original value
    cpu_dev->temp = op_ror(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, ror_zp_x_fetch_wait);
}

void ror_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, ror_abs_wait1);
    cpu_dev->addr_abs = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, ror_abs_wait2);
    cpu_dev->addr_abs |= (cpu_dev->bus->data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, ror_abs_wait3);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_abs_wait4); // Write original value
    cpu_dev->temp = op_ror(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, ror_abs_fetch_wait);
}

void ror_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, ror_abs_x_wait1);
    cpu_dev->lo = cpu_dev->bus->data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, ror_abs_x_wait2);
    cpu_dev->hi = cpu_dev->bus->data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF), ror_abs_x_wait3); // Dummy read
    cpu_dev->addr_abs += cpu_dev->x;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, ror_abs_x_wait4);
    cpu_dev->temp = cpu_dev->bus->data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_abs_x_wait5); // Write original value
    cpu_dev->temp = op_ror(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, ror_abs_x_fetch_wait);
}
