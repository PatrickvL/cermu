#include "cpu6510.h"

// ============================================================================
// MOS 6510 BRANCH AND JUMP INSTRUCTIONS
// ============================================================================

// Branch Instructions
void bcc_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bcc_wait1);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_C)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bcc_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bcc_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev, bcc_fetch_wait);
}

void bcs_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bcs_wait1);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_C) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bcs_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bcs_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev, bcs_fetch_wait);
}

void beq_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, beq_wait1);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_Z) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, beq_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, beq_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev, beq_fetch_wait);
}

void bne_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bne_wait1);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_Z)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bne_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bne_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev, bne_fetch_wait);
}

void bmi_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bmi_wait1);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_N) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bmi_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bmi_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev, bmi_fetch_wait);
}

void bpl_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bpl_wait1);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_N)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bpl_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bpl_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev, bpl_fetch_wait);
}

void bvc_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bvc_wait1);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_V)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bvc_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bvc_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev, bvc_fetch_wait);
}

void bvs_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bvs_wait1);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_V) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bvs_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bvs_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev, bvs_fetch_wait);
}

// Jump Instructions
void jmp_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, jmp_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, jmp_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    cpu_dev->pc = cpu_dev->address;
    NEXT_INSTRUCTION(cpu_dev, jmp_abs_fetch_wait);
}

void jmp_indirect_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait3);
    uint8_t pc_lo = cpu_read_cycle(cpu_dev, cpu_dev->address);
    
    // 6502 bug: if low byte is $FF, high byte wraps within same page
    if ((cpu_dev->address & 0xFF) == 0xFF) {
        cpu_dev->address = (cpu_dev->address & 0xFF00);
    } else {
        cpu_dev->address++;
    }
    
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait4);
    uint8_t pc_hi = cpu_read_cycle(cpu_dev, cpu_dev->address);
    cpu_dev->pc = (pc_hi << 8) | pc_lo;
    NEXT_INSTRUCTION(cpu_dev, jmp_ind_fetch_wait);
}

// Subroutine Instructions
void jsr_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, jsr_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, jsr_wait2);
    (void)cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp); // Dummy read from stack
    cpu_push(cpu_dev, (cpu_dev->pc >> 8) & 0xFF); // Push PC high byte (return address - 1)
    cpu_push(cpu_dev, cpu_dev->pc & 0xFF);         // Push PC low byte
    CPU_READY_OR_STALL(cpu_dev, jsr_wait3);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->pc = (addr_hi << 8) | addr_lo;
    NEXT_INSTRUCTION(cpu_dev, jsr_fetch_wait);
}
