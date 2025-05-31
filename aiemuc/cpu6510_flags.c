#include "cpu6510.h"

// ============================================================================
// MOS 6510 FLAG INSTRUCTIONS
// ============================================================================
// Clear and Set flag instructions (alphabetical order)

// Clear flag instructions
void clc_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p &= ~FLAG_C;
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cld_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p &= ~FLAG_D;
    CPU_OPCODE_FOOTER(cpu_dev);
}

void cli_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p &= ~FLAG_I;
    CPU_OPCODE_FOOTER(cpu_dev);
}

void clv_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p &= ~FLAG_V;
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Set flag instructions
void sec_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p |= FLAG_C;
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sed_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p |= FLAG_D;
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sei_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p |= FLAG_I;
    CPU_OPCODE_FOOTER(cpu_dev);
}
