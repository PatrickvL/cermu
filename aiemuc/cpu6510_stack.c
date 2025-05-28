#include "cpu6510.h"
#include "bus.h"

// ============================================================================
// MOS 6510 STACK INSTRUCTIONS
// ============================================================================

// PHA - Push Accumulator (0x48)
void pha_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, pha_wait);  // Dummy read
    cpu_push(cpu.a);
    NEXT_INSTRUCTION(pha_fetch_wait);
}

// PLA - Pull Accumulator (0x68)
void pla_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, pla_wait1);  // Dummy read
    WAIT_READY_THEN_READ(0x0100 + cpu.sp, pla_wait2);  // Dummy read from stack
    cpu.a = cpu_pop();
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(pla_fetch_wait);
}

// PHP - Push Processor Status (0x08)
void php_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, php_wait);  // Dummy read
    cpu_push(cpu.p | FLAG_B);  // Push with B flag set
    NEXT_INSTRUCTION(php_fetch_wait);
}

// PLP - Pull Processor Status (0x28)
void plp_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, plp_wait1);  // Dummy read
    WAIT_READY_THEN_READ(0x0100 + cpu.sp, plp_wait2);  // Dummy read from stack
    cpu.p = cpu_pop();
    cpu.p |= FLAG_U;  // Unused flag always set
    cpu.p &= ~FLAG_B; // Clear B flag
    NEXT_INSTRUCTION(plp_fetch_wait);
}

// RTI - Return from Interrupt (0x40)
void rti_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, rti_wait1);  // Dummy read
    WAIT_READY_THEN_READ(0x0100 + cpu.sp, rti_wait2);  // Dummy read from stack
    cpu.p = cpu_pop();
    cpu.p |= FLAG_U;  // Unused flag always set
    cpu.p &= ~FLAG_B; // Clear B flag
    cpu.pc = cpu_pop();  // Pull PC low
    cpu.pc |= (cpu_pop() << 8);  // Pull PC high
    NEXT_INSTRUCTION(rti_fetch_wait);
}

// RTS - Return from Subroutine (0x60)
void rts_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, rts_wait1);  // Dummy read
    WAIT_READY_THEN_READ(0x0100 + cpu.sp, rts_wait2);  // Dummy read from stack
    cpu.pc = cpu_pop();  // Pull PC low
    cpu.pc |= (cpu_pop() << 8);  // Pull PC high
    cpu.pc++;  // RTS returns to address + 1
    WAIT_READY_THEN_READ(cpu.pc, rts_wait3);  // Dummy read
    NEXT_INSTRUCTION(rts_fetch_wait);
}