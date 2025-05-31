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
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, asl_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_zp_wait3); // Write original value
    cpu_dev->temp = op_asl(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, asl_zp_fetch_wait);
}

void asl_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, asl_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, asl_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, asl_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_zp_x_wait4); // Write original value
    cpu_dev->temp = op_asl(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, asl_zp_x_fetch_wait);
}

void asl_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, asl_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, asl_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, asl_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_abs_wait4); // Write original value
    cpu_dev->temp = op_asl(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, asl_abs_fetch_wait);
}

void asl_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, asl_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, asl_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, asl_abs_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, asl_abs_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_abs_x_wait5); // Write original value
    cpu_dev->temp = op_asl(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, asl_abs_x_wait6);
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
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lsr_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_zp_wait3); // Write original value
    cpu_dev->temp = op_lsr(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, lsr_zp_fetch_wait);
}

void lsr_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lsr_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lsr_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, lsr_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_zp_x_wait4); // Write original value
    cpu_dev->temp = op_lsr(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, lsr_zp_x_fetch_wait);
}

void lsr_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_abs_wait4); // Write original value
    cpu_dev->temp = op_lsr(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, lsr_abs_fetch_wait);
}

void lsr_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, lsr_abs_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_abs_x_wait5); // Write original value
    cpu_dev->temp = op_lsr(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, lsr_abs_x_wait6);
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
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, rol_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_zp_wait3); // Write original value
    cpu_dev->temp = op_rol(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, rol_zp_fetch_wait);
}

void rol_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, rol_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, rol_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, rol_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_zp_x_wait4); // Write original value
    cpu_dev->temp = op_rol(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, rol_zp_x_fetch_wait);
}

void rol_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, rol_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, rol_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, rol_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_abs_wait4); // Write original value
    cpu_dev->temp = op_rol(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, rol_abs_fetch_wait);
}

void rol_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, rol_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, rol_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, rol_abs_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, rol_abs_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_abs_x_wait5); // Write original value
    cpu_dev->temp = op_rol(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rol_abs_x_wait6);
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
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ror_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_zp_wait3); // Write original value
    cpu_dev->temp = op_ror(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, ror_zp_fetch_wait);
}

void ror_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ror_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ror_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, ror_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_zp_x_wait4); // Write original value
    cpu_dev->temp = op_ror(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, ror_zp_x_fetch_wait);
}

void ror_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ror_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ror_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, ror_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_abs_wait4); // Write original value
    cpu_dev->temp = op_ror(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, ror_abs_fetch_wait);
}

void ror_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ror_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ror_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, ror_abs_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, ror_abs_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->temp = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_abs_x_wait5); // Write original value
    cpu_dev->temp = op_ror(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, ror_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, ror_abs_x_fetch_wait);
}
