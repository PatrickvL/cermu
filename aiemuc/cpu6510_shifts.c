#include "cpu6510.h"

// ============================================================================
// MOS 6510 SHIFT AND ROTATE INSTRUCTIONS
// ============================================================================

// ASL - Arithmetic Shift Left
void asl_accumulator_func(cpu6510_state_t* cpu_dev) {
    // ASL A is a 2-cycle instruction
    CPU_READY_OR_STALL(cpu_dev, asl_a_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->a = op_asl(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, asl_a_fetch_wait);
}

void asl_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, asl_zp_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, asl_zp_wait2);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, asl_zp_wait3); // Write original value
    value = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, asl_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, asl_zp_fetch_wait);
}

void asl_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, asl_zp_x_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, asl_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, asl_zp_x_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, asl_zp_x_wait4); // Write original value
    value = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, asl_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, asl_zp_x_fetch_wait);
}

void asl_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, asl_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, asl_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev, asl_abs_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, asl_abs_wait4); // Write original value
    value = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, asl_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, asl_abs_fetch_wait);
}

void asl_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, asl_abs_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, asl_abs_x_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, asl_abs_x_wait3);
    (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, asl_abs_x_wait4);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, asl_abs_x_wait5); // Write original value
    value = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, asl_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, asl_abs_x_fetch_wait);
}

// LSR - Logical Shift Right
void lsr_accumulator_func(cpu6510_state_t* cpu_dev) {
    // LSR A is a 2-cycle instruction
    CPU_READY_OR_STALL(cpu_dev, lsr_a_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->a = op_lsr(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lsr_a_fetch_wait);
}

void lsr_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lsr_zp_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, lsr_zp_wait2);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, lsr_zp_wait3); // Write original value
    value = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, lsr_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, lsr_zp_fetch_wait);
}

void lsr_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lsr_zp_x_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, lsr_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, lsr_zp_x_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, lsr_zp_x_wait4); // Write original value
    value = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, lsr_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, lsr_zp_x_fetch_wait);
}

void lsr_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, lsr_abs_wait4); // Write original value
    value = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, lsr_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, lsr_abs_fetch_wait);
}

void lsr_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_x_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_x_wait3);
    (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_x_wait4);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, lsr_abs_x_wait5); // Write original value
    value = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, lsr_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, lsr_abs_x_fetch_wait);
}

// ROL - Rotate Left
void rol_accumulator_func(cpu6510_state_t* cpu_dev) {
    // ROL A is a 2-cycle instruction
    CPU_READY_OR_STALL(cpu_dev, rol_a_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->a = op_rol(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, rol_a_fetch_wait);
}

void rol_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, rol_zp_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, rol_zp_wait2);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, rol_zp_wait3); // Write original value
    value = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, rol_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, rol_zp_fetch_wait);
}

void rol_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, rol_zp_x_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, rol_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, rol_zp_x_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, rol_zp_x_wait4); // Write original value
    value = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, rol_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, rol_zp_x_fetch_wait);
}

void rol_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, rol_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, rol_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev, rol_abs_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, rol_abs_wait4); // Write original value
    value = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, rol_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, rol_abs_fetch_wait);
}

void rol_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, rol_abs_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, rol_abs_x_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, rol_abs_x_wait3);
    (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, rol_abs_x_wait4);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, rol_abs_x_wait5); // Write original value
    value = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, rol_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, rol_abs_x_fetch_wait);
}

// ROR - Rotate Right
void ror_accumulator_func(cpu6510_state_t* cpu_dev) {
    // ROR A is a 2-cycle instruction
    CPU_READY_OR_STALL(cpu_dev, ror_a_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->a = op_ror(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, ror_a_fetch_wait);
}

void ror_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ror_zp_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, ror_zp_wait2);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, ror_zp_wait3); // Write original value
    value = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, ror_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, ror_zp_fetch_wait);
}

void ror_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ror_zp_x_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, ror_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, ror_zp_x_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, ror_zp_x_wait4); // Write original value
    value = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, ror_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, ror_zp_x_fetch_wait);
}

void ror_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ror_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, ror_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev, ror_abs_wait3);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, ror_abs_wait4); // Write original value
    value = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, ror_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, ror_abs_fetch_wait);
}

void ror_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ror_abs_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, ror_abs_x_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, ror_abs_x_wait3);
    (void)cpu_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, ror_abs_x_wait4);
    uint8_t value = cpu_read_cycle(cpu_dev, cpu_dev->address);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, ror_abs_x_wait5); // Write original value
    value = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, ror_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, ror_abs_x_fetch_wait);
}
