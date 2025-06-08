#include "mos6510.h"

// ============================================================================
// MOS 6510 STACK INSTRUCTIONS
// ============================================================================
// Push and Pull instructions for accumulator and processor status

// Pull instructions (alphabetical order)
void pla_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, 0x0100 + cpu->sp);  // Dummy read from current SP
    cpu->sp++;
    CPU_INTRA_CYCLE(cpu);
    cpu->a = mos6510_read_cycle(cpu, 0x0100 + cpu->sp);
    mos6510_set_zn(cpu, cpu->a);
    CPU_OPCODE_FOOTER(cpu);
}

void plp_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, 0x0100 + cpu->sp);  // Dummy read from current SP
    cpu->sp++;
    CPU_INTRA_CYCLE(cpu);
    cpu->p = mos6510_read_cycle(cpu, 0x0100 + cpu->sp);
    cpu->p |= FLAG_U;  // Unused flag always set
    cpu->p &= ~FLAG_B; // B flag ignored when pulled
    CPU_OPCODE_FOOTER(cpu);
}

// Push instructions
void pha_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, 0x0100 + cpu->sp, cpu->a);
    cpu->sp--;
    CPU_OPCODE_FOOTER(cpu);
}

void php_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, 0x0100 + cpu->sp, cpu->p | FLAG_B);  // B flag set on stack
    cpu->sp--;
    CPU_OPCODE_FOOTER(cpu);
}
