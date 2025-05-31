#include "cpu6510.h"

// ============================================================================
// MOS 6510 TRANSFER AND SYSTEM INSTRUCTIONS
// ============================================================================

// TAX - Transfer Accumulator to X (0xAA)
void tax_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, tax_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x = cpu_dev->a;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, tax_fetch_wait);
}

// TXA - Transfer X to Accumulator (0x8A)
void txa_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, txa_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->a = cpu_dev->x;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, txa_fetch_wait);
}

// TAY - Transfer Accumulator to Y (0xA8)
void tay_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, tay_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->y = cpu_dev->a;
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, tay_fetch_wait);
}

// TYA - Transfer Y to Accumulator (0x98)
void tya_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, tya_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->a = cpu_dev->y;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, tya_fetch_wait);
}

// TSX - Transfer Stack Pointer to X (0xBA)
void tsx_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, tsx_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x = cpu_dev->sp;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, tsx_fetch_wait);
}

// TXS - Transfer X to Stack Pointer (0x9A)
void txs_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, txs_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->sp = cpu_dev->x;
    NEXT_INSTRUCTION(cpu_dev, txs_fetch_wait);
}

// NOP variations - No Operation
void nop_zp_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_zp_wait1);
    cpu_dev->addr_abs = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, nop_zp_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);  // Read and discard
    NEXT_INSTRUCTION(cpu_dev, nop_zp_fetch_wait);
}

void nop_zp_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_zp_x_wait1);
    cpu_dev->addr_abs = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, nop_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, nop_zp_x_wait3);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);  // Read and discard
    NEXT_INSTRUCTION(cpu_dev, nop_zp_x_fetch_wait);
}

void nop_abs_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_abs_wait1);
    cpu_dev->addr_abs = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, nop_abs_wait2);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, nop_abs_wait3);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);  // Read and discard
    NEXT_INSTRUCTION(cpu_dev, nop_abs_fetch_wait);
}

void nop_abs_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_abs_x_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, nop_abs_x_wait2);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;

    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, nop_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs + cpu_dev->x);
    }

    CPU_READY_OR_STALL(cpu_dev, nop_abs_x_wait4);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs + cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, nop_abs_x_fetch_wait);
}

void nop_imm_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, nop_imm_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc++);  // Read and discard immediate value
    NEXT_INSTRUCTION(cpu_dev, nop_imm_fetch_wait);
}
