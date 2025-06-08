#include "mos6510.h"

// ============================================================================
// MOS 6510 MISCELLANEOUS INSTRUCTIONS
// ============================================================================
// NOP variants and other miscellaneous instructions

// NOP - No Operation (official)
void nop_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    CPU_OPCODE_FOOTER(cpu);
}

// NOP variants (illegal/unofficial) - alphabetical by addressing mode
void nop_absolute_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = addr_lo;
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address);  // Read and discard
    CPU_OPCODE_FOOTER(cpu);
}

void nop_absolute_x_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu->address = base + cpu->x;

    // Check for page crossing - some NOPs do this, some don't
    if ((base & 0xFF00) != (cpu->address & 0xFF00)) {
        CPU_INTRA_CYCLE(cpu);
        (void)mos6510_read_cycle(cpu, base); // Dummy read from base
    }

    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address);
    CPU_OPCODE_FOOTER(cpu);
}

void nop_immediate_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc++);  // Read and discard immediate value
    CPU_OPCODE_FOOTER(cpu);
}

void nop_zero_page_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    cpu->address = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address);  // Read and discard
    CPU_OPCODE_FOOTER(cpu);
}

void nop_zero_page_x_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    cpu->address = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address); // Dummy read
    cpu->address = (cpu->address + cpu->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address);  // Read and discard
    CPU_OPCODE_FOOTER(cpu);
}