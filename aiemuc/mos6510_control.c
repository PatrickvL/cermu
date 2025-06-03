#include "mos6510.h"

// ============================================================================
// MOS 6510 CONTROL FLOW INSTRUCTIONS
// ============================================================================
// Branch, Jump, Call, Return and Break instructions

// Branch Instructions (alphabetical order)
void bcc_func(mos6510_t* cpu_dev) {
    mos6510_branch_helper(cpu_dev, !cpu_get_flag(cpu_dev, FLAG_C));
}

void bcs_func(mos6510_t* cpu_dev) {
    mos6510_branch_helper(cpu_dev, cpu_get_flag(cpu_dev, FLAG_C));
}

void beq_func(mos6510_t* cpu_dev) {
    mos6510_branch_helper(cpu_dev, cpu_get_flag(cpu_dev, FLAG_Z));
}

void bmi_func(mos6510_t* cpu_dev) {
    mos6510_branch_helper(cpu_dev, cpu_get_flag(cpu_dev, FLAG_N));
}

void bne_func(mos6510_t* cpu_dev) {
    mos6510_branch_helper(cpu_dev, !cpu_get_flag(cpu_dev, FLAG_Z));
}

void bpl_func(mos6510_t* cpu_dev) {
    mos6510_branch_helper(cpu_dev, !cpu_get_flag(cpu_dev, FLAG_N));
}

void bvc_func(mos6510_t* cpu_dev) {
    mos6510_branch_helper(cpu_dev, !cpu_get_flag(cpu_dev, FLAG_V));
}

void bvs_func(mos6510_t* cpu_dev) {
    mos6510_branch_helper(cpu_dev, cpu_get_flag(cpu_dev, FLAG_V));
}

// Break
void brk_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc++);  // Read next byte (padding)
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, (cpu_dev->pc >> 8) & 0xFF);  // Push PC high
    cpu_dev->sp--;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, cpu_dev->pc & 0xFF);  // Push PC low
    cpu_dev->sp--;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, cpu_dev->p | FLAG_B);  // Push P with B set
    cpu_dev->sp--;
    cpu_dev->p |= FLAG_I;  // Set interrupt disable
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t pc_lo = mos6510_read_cycle(cpu_dev, 0xFFFE);  // Read IRQ vector low
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t pc_hi = mos6510_read_cycle(cpu_dev, 0xFFFF);  // Read IRQ vector high
    cpu_dev->pc = (pc_hi << 8) | pc_lo;
    CPU_OPCODE_FOOTER(cpu_dev);
}

void brk_instruction_func(mos6510_t* cpu_dev) {
    cpu_dev->pc++;  // Skip BRK signature byte
    mos6510_irq(cpu_dev, cpu_dev->p | FLAG_B); // Call IRQ handler
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Jump Instructions
void jmp_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->pc = (addr_hi << 8) | addr_lo;
    CPU_OPCODE_FOOTER(cpu_dev);
}

void jmp_indirect_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t ptr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t ptr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, ptr);
    CPU_INTRA_CYCLE(cpu_dev);
    // 6502 bug: if pointer is at page boundary, high byte wraps within page
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
    cpu_dev->pc = (addr_hi << 8) | addr_lo;
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Jump to Subroutine
void jsr_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);  // Dummy read from stack
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, (cpu_dev->pc >> 8) & 0xFF);  // Push PC high
    cpu_dev->sp--;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, cpu_dev->pc & 0xFF);  // Push PC low
    cpu_dev->sp--;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->pc = (addr_hi << 8) | addr_lo;
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Return from Interrupt
void rti_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);  // Dummy read from current SP
    cpu_dev->sp++;
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->p = mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->p |= FLAG_U;  // Unused flag always set
    cpu_dev->sp++;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t pc_lo = mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->sp++;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t pc_hi = mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->pc = (pc_hi << 8) | pc_lo;
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Return from Subroutine
void rts_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);  // Dummy read from current SP
    cpu_dev->sp++;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t pc_lo = mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->sp++;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t pc_hi = mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->pc = (pc_hi << 8) | pc_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->pc++;  // RTS increments PC
    CPU_OPCODE_FOOTER(cpu_dev);
}