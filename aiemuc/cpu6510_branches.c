#include "cpu6510.h"
#include "bus.h"

// ============================================================================
// MOS 6510 BRANCH AND JUMP INSTRUCTIONS
// ============================================================================

// Branch Instructions
void bcc_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, bcc_wait1);
    cpu_dev->addr_rel = bus.data;
    if (!(cpu_dev->p & FLAG_C)) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, bcc_wait2); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF), bcc_wait3);
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bcc_fetch_wait);
}

void bcs_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, bcs_wait1);
    cpu_dev->addr_rel = bus.data;
    if (cpu_dev->p & FLAG_C) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, bcs_wait2); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF), bcs_wait3);
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bcs_fetch_wait);
}

void beq_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, beq_wait1);
    cpu_dev->addr_rel = bus.data;
    if (cpu_dev->p & FLAG_Z) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, beq_wait2); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF), beq_wait3);
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, beq_fetch_wait);
}

void bne_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, bne_wait1);
    cpu_dev->addr_rel = bus.data;
    if (!(cpu_dev->p & FLAG_Z)) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, bne_wait2); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF), bne_wait3);
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bne_fetch_wait);
}

void bmi_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, bmi_wait1);
    cpu_dev->addr_rel = bus.data;
    if (cpu_dev->p & FLAG_N) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, bmi_wait2); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF), bmi_wait3);
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bmi_fetch_wait);
}

void bpl_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, bpl_wait1);
    cpu_dev->addr_rel = bus.data;
    if (!(cpu_dev->p & FLAG_N)) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, bpl_wait2); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF), bpl_wait3);
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bpl_fetch_wait);
}

void bvc_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, bvc_wait1);
    cpu_dev->addr_rel = bus.data;
    if (!(cpu_dev->p & FLAG_V)) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, bvc_wait2); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF), bvc_wait3);
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bvc_fetch_wait);
}

void bvs_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, bvs_wait1);
    cpu_dev->addr_rel = bus.data;
    if (cpu_dev->p & FLAG_V) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, bvs_wait2); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + cpu_dev->addr_rel) & 0xFF), bvs_wait3);
        }
        cpu_dev->pc += (int8_t)cpu_dev->addr_rel;
    }
    NEXT_INSTRUCTION(cpu_dev, bvs_fetch_wait);
}

// Jump Instructions
void jmp_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, jmp_abs_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, jmp_abs_wait2);
    cpu_dev->addr_abs |= (bus.data << 8);
    cpu_dev->pc = cpu_dev->addr_abs;
    NEXT_INSTRUCTION(cpu_dev, jmp_abs_fetch_wait);
}

void jmp_indirect_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, jmp_ind_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, jmp_ind_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, jmp_ind_wait3);
    cpu_dev->lo = bus.data;
    
    // 6502 bug: if low byte is $FF, high byte wraps within same page
    if ((cpu_dev->addr_abs & 0xFF) == 0xFF) {
        cpu_dev->addr_abs = (cpu_dev->addr_abs & 0xFF00);
    } else {
        cpu_dev->addr_abs++;
    }
    
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, jmp_ind_wait4);
    cpu_dev->hi = bus.data;
    cpu_dev->pc = (cpu_dev->hi << 8) | cpu_dev->lo;
    NEXT_INSTRUCTION(cpu_dev, jmp_ind_fetch_wait);
}

// Subroutine Instructions
void jsr_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, jsr_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, 0x0100 + cpu_dev->sp, jsr_wait2); // Dummy read from stack
    cpu_push(cpu_dev, (cpu_dev->pc >> 8) & 0xFF); // Push PC high byte (return address - 1)
    cpu_push(cpu_dev, cpu_dev->pc & 0xFF);         // Push PC low byte
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, jsr_wait3);
    cpu_dev->hi = bus.data;
    cpu_dev->pc = (cpu_dev->hi << 8) | cpu_dev->lo;
    NEXT_INSTRUCTION(cpu_dev, jsr_fetch_wait);
}

// void rts_func(cpu6510_state_t* cpu_dev) {
//     WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, rts_wait1); // Dummy read
//     WAIT_READY_THEN_READ(cpu_dev, 0x0100 + cpu_dev->sp, rts_wait2); // Dummy read from stack
//     cpu_dev->lo = cpu_pull(); // Pull PC low byte
//     cpu_dev->hi = cpu_pull(); // Pull PC high byte
//     cpu_dev->pc = (cpu_dev->hi << 8) | cpu_dev->lo;
//     WAIT_READY_