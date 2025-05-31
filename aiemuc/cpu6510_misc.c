#include "cpu6510.h"

// ============================================================================
// MOS 6510 MISCELLANEOUS INSTRUCTIONS
// ============================================================================
// NOP variants and other miscellaneous instructions

// NOP - No Operation (official)
void nop_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_OPCODE_FOOTER(cpu_dev);
}

// NOP variants (illegal/unofficial) - alphabetical by addressing mode
void nop_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Read and discard
    CPU_OPCODE_FOOTER(cpu_dev);
}

void nop_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu_dev->address = base + cpu_dev->x;

    // Check for page crossing - some NOPs do this, some don't
    if ((base & 0xFF00) != (cpu_dev->address & 0xFF00)) {
        CPU_INTRA_CYCLE(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, base); // Dummy read from base
    }

    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void nop_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc++);  // Read and discard immediate value
    CPU_OPCODE_FOOTER(cpu_dev);
}

void nop_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Read and discard
    CPU_OPCODE_FOOTER(cpu_dev);
}

void nop_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Read and discard
    CPU_OPCODE_FOOTER(cpu_dev);
}