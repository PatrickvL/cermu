#include "cpu6510.h"
#include "bus.h"

// ============================================================================
// MOS 6510 STACK INSTRUCTIONS
// ============================================================================

// PHA - Push Accumulator (0x48)
void pha_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, pha_wait);  // Dummy read
    cpu_push(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, pha_fetch_wait);
}

// PLA - Pull Accumulator (0x68)
void pla_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, pla_wait1);  // Dummy read
    WAIT_READY_THEN_READ(cpu_dev, 0x0100 + cpu_dev->sp, pla_wait2);  // Dummy read from stack
    cpu_dev->a = cpu_pop(cpu_dev);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, pla_fetch_wait);
}

// PHP - Push Processor Status (0x08)
void php_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, php_wait);  // Dummy read
    cpu_push(cpu_dev, cpu_dev->p | FLAG_B);  // Push with B flag set
    NEXT_INSTRUCTION(cpu_dev, php_fetch_wait);
}

// PLP - Pull Processor Status (0x28)
void plp_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, plp_wait1);  // Dummy read
    WAIT_READY_THEN_READ(cpu_dev, 0x0100 + cpu_dev->sp, plp_wait2);  // Dummy read from stack
    cpu_dev->p = cpu_pop(cpu_dev);
    cpu_dev->p |= FLAG_U;  // Unused flag always set
    cpu_dev->p &= ~FLAG_B; // Clear B flag
    NEXT_INSTRUCTION(cpu_dev, plp_fetch_wait);
}

// RTI - Return from Interrupt (0x40)
void rti_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, rti_wait1);  // Dummy read
    WAIT_READY_THEN_READ(cpu_dev, 0x0100 + cpu_dev->sp, rti_wait2);  // Dummy read from stack
    cpu_dev->p = cpu_pop(cpu_dev);
    cpu_dev->p |= FLAG_U;  // Unused flag always set
    cpu_dev->p &= ~FLAG_B; // Clear B flag
    cpu_dev->pc = cpu_pop(cpu_dev);  // Pull PC low
    cpu_dev->pc |= (cpu_pop(cpu_dev) << 8);  // Pull PC high
    NEXT_INSTRUCTION(cpu_dev, rti_fetch_wait);
}

// RTS - Return from Subroutine (0x60)
void rts_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, rts_wait1);  // Dummy read
    WAIT_READY_THEN_READ(cpu_dev, 0x0100 + cpu_dev->sp, rts_wait2);  // Dummy read from stack
    cpu_dev->pc = cpu_pop(cpu_dev);  // Pull PC low
    cpu_dev->pc |= (cpu_pop(cpu_dev) << 8);  // Pull PC high
    cpu_dev->pc++;  // RTS returns to address + 1
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, rts_wait3);  // Dummy read
    NEXT_INSTRUCTION(cpu_dev, rts_fetch_wait);
}