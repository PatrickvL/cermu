#include "cpu6510.h"

// ============================================================================
// MOS 6510 FLAG INSTRUCTIONS
// ============================================================================
// Clear and Set flag instructions (alphabetical order)

// Clear flag instructions
void clc_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, clc_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p &= ~FLAG_C;
    NEXT_INSTRUCTION(cpu_dev, clc_fetch_wait);
}

void cld_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cld_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p &= ~FLAG_D;
    NEXT_INSTRUCTION(cpu_dev, cld_fetch_wait);
}

void cli_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cli_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p &= ~FLAG_I;
    NEXT_INSTRUCTION(cpu_dev, cli_fetch_wait);
}

void clv_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, clv_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p &= ~FLAG_V;
    NEXT_INSTRUCTION(cpu_dev, clv_fetch_wait);
}

// Set flag instructions
void sec_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sec_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p |= FLAG_C;
    NEXT_INSTRUCTION(cpu_dev, sec_fetch_wait);
}

void sed_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sed_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p |= FLAG_D;
    NEXT_INSTRUCTION(cpu_dev, sed_fetch_wait);
}

void sei_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sei_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p |= FLAG_I;
    NEXT_INSTRUCTION(cpu_dev, sei_fetch_wait);
}
