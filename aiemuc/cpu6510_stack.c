#include "cpu6510.h"

// ============================================================================
// MOS 6510 STACK INSTRUCTIONS
// ============================================================================
// Push and Pull instructions for accumulator and processor status

// Pull instructions (alphabetical order)
void pla_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, pla_wait1);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_READY_OR_STALL(cpu_dev, pla_wait2);
    (void)cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);  // Dummy read from current SP
    cpu_dev->sp++;
    CPU_READY_OR_STALL(cpu_dev, pla_wait3);
    cpu_dev->a = cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, pla_fetch_wait);
}

void plp_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, plp_wait1);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_READY_OR_STALL(cpu_dev, plp_wait2);
    (void)cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);  // Dummy read from current SP
    cpu_dev->sp++;
    CPU_READY_OR_STALL(cpu_dev, plp_wait3);
    cpu_dev->p = cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->p |= FLAG_U;  // Unused flag always set
    cpu_dev->p &= ~FLAG_B; // B flag ignored when pulled
    NEXT_INSTRUCTION(cpu_dev, plp_fetch_wait);
}

// Push instructions
void pha_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, pha_wait1);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_READY_OR_STALL(cpu_dev, pha_wait2);
    cpu_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, cpu_dev->a);
    cpu_dev->sp--;
    NEXT_INSTRUCTION(cpu_dev, pha_fetch_wait);
}

void php_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, php_wait1);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_READY_OR_STALL(cpu_dev, php_wait2);
    cpu_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, cpu_dev->p | FLAG_B);  // B flag set on stack
    cpu_dev->sp--;
    NEXT_INSTRUCTION(cpu_dev, php_fetch_wait);
}
