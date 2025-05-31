#include "cpu6510.h"

// ============================================================================
// MOS 6510 CONTROL FLOW INSTRUCTIONS
// ============================================================================
// Branch, Jump, Call, Return and Break instructions

// Branch Instructions (alphabetical order)
void bcc_func(cpu6510_state_t* cpu_dev) {
    int8_t offset = (int8_t)addr_imm(cpu_dev);
    if (!cpu_get_flag(cpu_dev, FLAG_C)) {
        uint16_t new_pc = cpu_dev->pc + offset;
        if ((cpu_dev->pc & 0xFF00) != (new_pc & 0xFF00)) {
            // Page boundary crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bcc_page_cross_wait);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | (new_pc & 0x00FF));
        }
        CPU_READY_OR_STALL(cpu_dev, bcc_branch_wait);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
        cpu_dev->pc = new_pc;
    }
    NEXT_INSTRUCTION(cpu_dev, bcc_fetch_wait);
}

void bcs_func(cpu6510_state_t* cpu_dev) {
    int8_t offset = (int8_t)addr_imm(cpu_dev);
    if (cpu_get_flag(cpu_dev, FLAG_C)) {
        uint16_t new_pc = cpu_dev->pc + offset;
        if ((cpu_dev->pc & 0xFF00) != (new_pc & 0xFF00)) {
            CPU_READY_OR_STALL(cpu_dev, bcs_page_cross_wait);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | (new_pc & 0x00FF));
        }
        CPU_READY_OR_STALL(cpu_dev, bcs_branch_wait);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);
        cpu_dev->pc = new_pc;
    }
    NEXT_INSTRUCTION(cpu_dev, bcs_fetch_wait);
}

void beq_func(cpu6510_state_t* cpu_dev) {
    int8_t offset = (int8_t)addr_imm(cpu_dev);
    if (cpu_get_flag(cpu_dev, FLAG_Z)) {
        uint16_t new_pc = cpu_dev->pc + offset;
        if ((cpu_dev->pc & 0xFF00) != (new_pc & 0xFF00)) {
            CPU_READY_OR_STALL(cpu_dev, beq_page_cross_wait);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | (new_pc & 0x00FF));
        }
        CPU_READY_OR_STALL(cpu_dev, beq_branch_wait);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);
        cpu_dev->pc = new_pc;
    }
    NEXT_INSTRUCTION(cpu_dev, beq_fetch_wait);
}

void bmi_func(cpu6510_state_t* cpu_dev) {
    int8_t offset = (int8_t)addr_imm(cpu_dev);
    if (cpu_get_flag(cpu_dev, FLAG_N)) {
        uint16_t new_pc = cpu_dev->pc + offset;
        if ((cpu_dev->pc & 0xFF00) != (new_pc & 0xFF00)) {
            CPU_READY_OR_STALL(cpu_dev, bmi_page_cross_wait);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | (new_pc & 0x00FF));
        }
        CPU_READY_OR_STALL(cpu_dev, bmi_branch_wait);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);
        cpu_dev->pc = new_pc;
    }
    NEXT_INSTRUCTION(cpu_dev, bmi_fetch_wait);
}

void bne_func(cpu6510_state_t* cpu_dev) {
    int8_t offset = (int8_t)addr_imm(cpu_dev);
    if (!cpu_get_flag(cpu_dev, FLAG_Z)) {
        uint16_t new_pc = cpu_dev->pc + offset;
        if ((cpu_dev->pc & 0xFF00) != (new_pc & 0xFF00)) {
            CPU_READY_OR_STALL(cpu_dev, bne_page_cross_wait);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | (new_pc & 0x00FF));
        }
        CPU_READY_OR_STALL(cpu_dev, bne_branch_wait);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);
        cpu_dev->pc = new_pc;
    }
    NEXT_INSTRUCTION(cpu_dev, bne_fetch_wait);
}

void bpl_func(cpu6510_state_t* cpu_dev) {
    int8_t offset = (int8_t)addr_imm(cpu_dev);
    if (!cpu_get_flag(cpu_dev, FLAG_N)) {
        uint16_t new_pc = cpu_dev->pc + offset;
        if ((cpu_dev->pc & 0xFF00) != (new_pc & 0xFF00)) {
            CPU_READY_OR_STALL(cpu_dev, bpl_page_cross_wait);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | (new_pc & 0x00FF));
        }
        CPU_READY_OR_STALL(cpu_dev, bpl_branch_wait);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);
        cpu_dev->pc = new_pc;
    }
    NEXT_INSTRUCTION(cpu_dev, bpl_fetch_wait);
}

void bvc_func(cpu6510_state_t* cpu_dev) {
    int8_t offset = (int8_t)addr_imm(cpu_dev);
    if (!cpu_get_flag(cpu_dev, FLAG_V)) {
        uint16_t new_pc = cpu_dev->pc + offset;
        if ((cpu_dev->pc & 0xFF00) != (new_pc & 0xFF00)) {
            CPU_READY_OR_STALL(cpu_dev, bvc_page_cross_wait);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | (new_pc & 0x00FF));
        }
        CPU_READY_OR_STALL(cpu_dev, bvc_branch_wait);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);
        cpu_dev->pc = new_pc;
    }
    NEXT_INSTRUCTION(cpu_dev, bvc_fetch_wait);
}

void bvs_func(cpu6510_state_t* cpu_dev) {
    int8_t offset = (int8_t)addr_imm(cpu_dev);
    if (cpu_get_flag(cpu_dev, FLAG_V)) {
        uint16_t new_pc = cpu_dev->pc + offset;
        if ((cpu_dev->pc & 0xFF00) != (new_pc & 0xFF00)) {
            CPU_READY_OR_STALL(cpu_dev, bvs_page_cross_wait);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | (new_pc & 0x00FF));
        }
        CPU_READY_OR_STALL(cpu_dev, bvs_branch_wait);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);
        cpu_dev->pc = new_pc;
    }
    NEXT_INSTRUCTION(cpu_dev, bvs_fetch_wait);
}

// Break
void brk_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, brk_wait1);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc++);  // Read next byte (padding)
    CPU_READY_OR_STALL(cpu_dev, brk_wait2);
    cpu_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, (cpu_dev->pc >> 8) & 0xFF);  // Push PC high
    cpu_dev->sp--;
    CPU_READY_OR_STALL(cpu_dev, brk_wait3);
    cpu_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, cpu_dev->pc & 0xFF);  // Push PC low
    cpu_dev->sp--;
    CPU_READY_OR_STALL(cpu_dev, brk_wait4);
    cpu_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, cpu_dev->p | FLAG_B);  // Push P with B set
    cpu_dev->sp--;
    cpu_dev->p |= FLAG_I;  // Set interrupt disable
    CPU_READY_OR_STALL(cpu_dev, brk_wait5);
    uint8_t pc_lo = cpu_read_cycle(cpu_dev, 0xFFFE);  // Read IRQ vector low
    CPU_READY_OR_STALL(cpu_dev, brk_wait6);
    uint8_t pc_hi = cpu_read_cycle(cpu_dev, 0xFFFF);  // Read IRQ vector high
    cpu_dev->pc = (pc_hi << 8) | pc_lo;
    NEXT_INSTRUCTION(cpu_dev, brk_fetch_wait);
}

void brk_instruction_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->pc++;  // Skip BRK signature byte
    cpu6510_irq(cpu_dev, cpu_dev->p | FLAG_B); // Call IRQ handler
    NEXT_INSTRUCTION(cpu_dev, brk_fetch_wait);
}

// Jump Instructions
void jmp_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, jmp_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, jmp_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->pc = (addr_hi << 8) | addr_lo;
    NEXT_INSTRUCTION(cpu_dev, jmp_abs_fetch_wait);
}

void jmp_indirect_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait1);
    uint8_t ptr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait2);
    uint8_t ptr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait3);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, ptr);
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait4);
    // 6502 bug: if pointer is at page boundary, high byte wraps within page
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
    cpu_dev->pc = (addr_hi << 8) | addr_lo;
    NEXT_INSTRUCTION(cpu_dev, jmp_ind_fetch_wait);
}

// Jump to Subroutine
void jsr_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, jsr_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, jsr_wait2);
    (void)cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);  // Dummy read from stack
    CPU_READY_OR_STALL(cpu_dev, jsr_wait3);
    cpu_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, (cpu_dev->pc >> 8) & 0xFF);  // Push PC high
    cpu_dev->sp--;
    CPU_READY_OR_STALL(cpu_dev, jsr_wait4);
    cpu_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, cpu_dev->pc & 0xFF);  // Push PC low
    cpu_dev->sp--;
    CPU_READY_OR_STALL(cpu_dev, jsr_wait5);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->pc = (addr_hi << 8) | addr_lo;
    NEXT_INSTRUCTION(cpu_dev, jsr_fetch_wait);
}

// Return from Interrupt
void rti_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, rti_wait1);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_READY_OR_STALL(cpu_dev, rti_wait2);
    (void)cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);  // Dummy read from current SP
    cpu_dev->sp++;
    CPU_READY_OR_STALL(cpu_dev, rti_wait3);
    cpu_dev->p = cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->p |= FLAG_U;  // Unused flag always set
    cpu_dev->sp++;
    CPU_READY_OR_STALL(cpu_dev, rti_wait4);
    uint8_t pc_lo = cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->sp++;
    CPU_READY_OR_STALL(cpu_dev, rti_wait5);
    uint8_t pc_hi = cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->pc = (pc_hi << 8) | pc_lo;
    NEXT_INSTRUCTION(cpu_dev, rti_fetch_wait);
}

// Return from Subroutine
void rts_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, rts_wait1);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    CPU_READY_OR_STALL(cpu_dev, rts_wait2);
    (void)cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);  // Dummy read from current SP
    cpu_dev->sp++;
    CPU_READY_OR_STALL(cpu_dev, rts_wait3);
    uint8_t pc_lo = cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->sp++;
    CPU_READY_OR_STALL(cpu_dev, rts_wait4);
    uint8_t pc_hi = cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
    cpu_dev->pc = (pc_hi << 8) | pc_lo;
    CPU_READY_OR_STALL(cpu_dev, rts_wait5);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->pc++;  // RTS increments PC
    NEXT_INSTRUCTION(cpu_dev, rts_fetch_wait);
}