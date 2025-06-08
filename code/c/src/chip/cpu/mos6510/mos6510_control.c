#include "mos6510.h"

// Macro to define branch instructions
#define DEFINE_BRANCH(fn, cond) \
    void fn(mos6510_t* cpu) { mos6510_branch_helper(cpu, cond); }

// ============================================================================
// MOS 6510 CONTROL FLOW INSTRUCTIONS
// ============================================================================
// Branch, Jump, Call, Return and Break instructions

// Branch Instructions (alphabetical order)
DEFINE_BRANCH(bcc_func, !cpu_get_flag(cpu, FLAG_C)) // BCC - Branch if Carry Clear (0x90)
DEFINE_BRANCH(bcs_func,  cpu_get_flag(cpu, FLAG_C)) // BCS - Branch if Carry Set (0xB0)
DEFINE_BRANCH(beq_func,  cpu_get_flag(cpu, FLAG_Z)) // BEQ - Branch if Equal (0xF0)
DEFINE_BRANCH(bmi_func,  cpu_get_flag(cpu, FLAG_N)) // BMI - Branch if Minus (0x30)
DEFINE_BRANCH(bne_func, !cpu_get_flag(cpu, FLAG_Z)) // BNE - Branch if Not Equal (0xD0)
DEFINE_BRANCH(bpl_func, !cpu_get_flag(cpu, FLAG_N)) // BPL - Branch if Positive (0x10)
DEFINE_BRANCH(bvc_func, !cpu_get_flag(cpu, FLAG_V)) // BVC - Branch if Overflow Clear (0x50)
DEFINE_BRANCH(bvs_func,  cpu_get_flag(cpu, FLAG_V)) // BVS - Branch if Overflow Set (0x70)

// Break
void brk_func(mos6510_t* cpu) {
    cpu->pc++;  // Skip BRK signature byte
    mos6510_irq(cpu, cpu->p | FLAG_B); // Call IRQ handler
    CPU_OPCODE_FOOTER(cpu);
}

// Jump Instructions
void jmp_absolute_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->pc = (addr_hi << 8) | addr_lo;
    CPU_OPCODE_FOOTER(cpu);
}

void jmp_indirect_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t ptr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t ptr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, ptr);
    CPU_INTRA_CYCLE(cpu);
    // 6502 bug: if pointer is at page boundary, high byte wraps within page
    uint8_t addr_hi = mos6510_read_cycle(cpu, (ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
    cpu->pc = (addr_hi << 8) | addr_lo;
    CPU_OPCODE_FOOTER(cpu);
}

// Jump to Subroutine
void jsr_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, 0x0100 + cpu->sp);  // Dummy read from stack
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, 0x0100 + cpu->sp, (cpu->pc >> 8) & 0xFF);  // Push PC high
    cpu->sp--;
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, 0x0100 + cpu->sp, cpu->pc & 0xFF);  // Push PC low
    cpu->sp--;
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->pc = (addr_hi << 8) | addr_lo;
    CPU_OPCODE_FOOTER(cpu);
}

// Return from Interrupt
void rti_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, 0x0100 + cpu->sp);  // Dummy read from current SP
    cpu->sp++;
    CPU_INTRA_CYCLE(cpu);
    cpu->p = mos6510_read_cycle(cpu, 0x0100 + cpu->sp);
    cpu->p |= FLAG_U;  // Unused flag always set
    cpu->sp++;
    CPU_INTRA_CYCLE(cpu);
    uint8_t pc_lo = mos6510_read_cycle(cpu, 0x0100 + cpu->sp);
    cpu->sp++;
    CPU_INTRA_CYCLE(cpu);
    uint8_t pc_hi = mos6510_read_cycle(cpu, 0x0100 + cpu->sp);
    cpu->pc = (pc_hi << 8) | pc_lo;
    CPU_OPCODE_FOOTER(cpu);
}

// Return from Subroutine
void rts_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, 0x0100 + cpu->sp);  // Dummy read from current SP
    cpu->sp++;
    CPU_INTRA_CYCLE(cpu);
    uint8_t pc_lo = mos6510_read_cycle(cpu, 0x0100 + cpu->sp);
    cpu->sp++;
    CPU_INTRA_CYCLE(cpu);
    uint8_t pc_hi = mos6510_read_cycle(cpu, 0x0100 + cpu->sp);
    cpu->pc = (pc_hi << 8) | pc_lo;
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    cpu->pc++;  // RTS increments PC
    CPU_OPCODE_FOOTER(cpu);
}