#include "cpu6510.h"

// ============================================================================
// MOS 6510 MISCELLANEOUS INSTRUCTIONS
// ============================================================================
// NOP variants and other miscellaneous instructions

// NOP - No Operation (official)
void nop_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    NEXT_INSTRUCTION(cpu_dev, nop_fetch_wait);
}

// NOP variants (illegal/unofficial) - alphabetical by addressing mode
void nop_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, nop_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_READY_OR_STALL(cpu_dev, nop_abs_wait3);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Read and discard
    NEXT_INSTRUCTION(cpu_dev, nop_abs_fetch_wait);
}

void nop_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_abs_x_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, nop_abs_x_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu_dev->address = base + cpu_dev->x;

    // Check for page crossing - some NOPs do this, some don't
    if ((base & 0xFF00) != (cpu_dev->address & 0xFF00)) {
        CPU_READY_OR_STALL(cpu_dev, nop_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, base); // Dummy read from base
    }

    CPU_READY_OR_STALL(cpu_dev, nop_abs_x_wait4);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);
    NEXT_INSTRUCTION(cpu_dev, nop_abs_x_fetch_wait);
}

void nop_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_imm_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc++);  // Read and discard immediate value
    NEXT_INSTRUCTION(cpu_dev, nop_imm_fetch_wait);
}

void nop_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_zp_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, nop_zp_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Read and discard
    NEXT_INSTRUCTION(cpu_dev, nop_zp_fetch_wait);
}

void nop_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_zp_x_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, nop_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, nop_zp_x_wait3);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Read and discard
    NEXT_INSTRUCTION(cpu_dev, nop_zp_x_fetch_wait);
}