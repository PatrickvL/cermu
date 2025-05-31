#include "cpu6510.h"

// ============================================================================
// MOS 6510 BRANCH AND JUMP INSTRUCTIONS
// ============================================================================

// Branch Instructions
void bcc_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bcc_wait1);
    cpu_dev->addr_rel = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_C)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bcc_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bcc_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF));
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bcc_fetch_wait);
}

void bcs_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bcs_wait1);
    cpu_dev->addr_rel = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_C) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bcs_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bcs_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF));
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bcs_fetch_wait);
}

void beq_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, beq_wait1);
    cpu_dev->addr_rel = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_Z) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, beq_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, beq_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF));
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, beq_fetch_wait);
}

void bne_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bne_wait1);
    cpu_dev->addr_rel = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_Z)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bne_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bne_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF));
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bne_fetch_wait);
}

void bmi_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bmi_wait1);
    cpu_dev->addr_rel = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_N) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bmi_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bmi_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF));
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bmi_fetch_wait);
}

void bpl_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bpl_wait1);
    cpu_dev->addr_rel = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_N)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bpl_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bpl_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF));
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bpl_fetch_wait);
}

void bvc_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bvc_wait1);
    cpu_dev->addr_rel = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_V)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bvc_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bvc_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF));
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bvc_fetch_wait);
}

void bvs_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bvs_wait1);
    cpu_dev->addr_rel = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_V) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev, bvs_wait2);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev, bvs_wait3);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF));
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bvs_fetch_wait);
}

// Jump Instructions
void jmp_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, jmp_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, jmp_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    cpu_dev->pc = cpu_dev->addr_abs;
    NEXT_INSTRUCTION(cpu_dev, jmp_abs_fetch_wait);
}

void jmp_indirect_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait2);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait3);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    
    // 6502 bug: if low byte is $FF, high byte wraps within same page
    if ((cpu_dev->addr_abs & 0xFF) == 0xFF) {
        cpu_dev->addr_abs = (cpu_dev->addr_abs & 0xFF00);
    } else {
        cpu_dev->addr_abs++;
    }
    
    CPU_READY_OR_STALL(cpu_dev, jmp_ind_wait4);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->pc = (cpu_dev->hi << 8) | cpu_dev->lo;
    NEXT_INSTRUCTION(cpu_dev, jmp_ind_fetch_wait);
}

// Subroutine Instructions
void jsr_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, jsr_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, jsr_wait2);
    (void)cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp); // Dummy read from stack
    cpu_push(cpu_dev, (cpu_dev->pc >> 8) & 0xFF); // Push PC high byte (return address - 1)
    cpu_push(cpu_dev, cpu_dev->pc & 0xFF);         // Push PC low byte
    CPU_READY_OR_STALL(cpu_dev, jsr_wait3);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->pc = (cpu_dev->hi << 8) | cpu_dev->lo;
    NEXT_INSTRUCTION(cpu_dev, jsr_fetch_wait);
}
