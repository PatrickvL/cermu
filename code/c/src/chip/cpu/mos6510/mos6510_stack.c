#include "mos6510.h"

// ============================================================================
// MOS 6510 STACK INSTRUCTIONS
// ============================================================================
// Push and Pull instructions for accumulator and processor status

// Pull instructions (alphabetical order)
void pla_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);  // Dummy read from current SP
    cpu_dev->sp++;
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->a = mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void plp_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);  // Dummy read from current SP
    cpu_dev->sp++;
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->p = mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->p |= FLAG_U;  // Unused flag always set
    cpu_dev->p &= ~FLAG_B; // B flag ignored when pulled
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Push instructions
void pha_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, cpu_dev->a);
    cpu_dev->sp--;
    CPU_OPCODE_FOOTER(cpu_dev);
}

void php_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, cpu_dev->p | FLAG_B);  // B flag set on stack
    cpu_dev->sp--;
    CPU_OPCODE_FOOTER(cpu_dev);
}
