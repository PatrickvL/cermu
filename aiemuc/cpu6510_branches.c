#include "cpu6510.h"
#include "bus.h"

// ============================================================================
// MOS 6510 BRANCH AND JUMP INSTRUCTIONS
// ============================================================================

// Branch Instructions
void bcc_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, bcc_wait1);
    cpu.addr_rel = bus.data;
    if (!(cpu.p & FLAG_C)) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu.pc, bcc_wait2); // Dummy read
        if ((cpu.pc & 0xFF00) != ((cpu.pc + cpu.addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ((cpu.pc & 0xFF00) | ((cpu.pc + cpu.addr_rel) & 0xFF), bcc_wait3);
        }
        cpu.pc += (int8_t)cpu.addr_rel;
    }
    NEXT_INSTRUCTION(bcc_fetch_wait);
}

void bcs_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, bcs_wait1);
    cpu.addr_rel = bus.data;
    if (cpu.p & FLAG_C) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu.pc, bcs_wait2); // Dummy read
        if ((cpu.pc & 0xFF00) != ((cpu.pc + cpu.addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ((cpu.pc & 0xFF00) | ((cpu.pc + cpu.addr_rel) & 0xFF), bcs_wait3);
        }
        cpu.pc += (int8_t)cpu.addr_rel;
    }
    NEXT_INSTRUCTION(bcs_fetch_wait);
}

void beq_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, beq_wait1);
    cpu.addr_rel = bus.data;
    if (cpu.p & FLAG_Z) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu.pc, beq_wait2); // Dummy read
        if ((cpu.pc & 0xFF00) != ((cpu.pc + cpu.addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ((cpu.pc & 0xFF00) | ((cpu.pc + cpu.addr_rel) & 0xFF), beq_wait3);
        }
        cpu.pc += (int8_t)cpu.addr_rel;
    }
    NEXT_INSTRUCTION(beq_fetch_wait);
}

void bne_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, bne_wait1);
    cpu.addr_rel = bus.data;
    if (!(cpu.p & FLAG_Z)) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu.pc, bne_wait2); // Dummy read
        if ((cpu.pc & 0xFF00) != ((cpu.pc + cpu.addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ((cpu.pc & 0xFF00) | ((cpu.pc + cpu.addr_rel) & 0xFF), bne_wait3);
        }
        cpu.pc += (int8_t)cpu.addr_rel;
    }
    NEXT_INSTRUCTION(bne_fetch_wait);
}

void bmi_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, bmi_wait1);
    cpu.addr_rel = bus.data;
    if (cpu.p & FLAG_N) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu.pc, bmi_wait2); // Dummy read
        if ((cpu.pc & 0xFF00) != ((cpu.pc + cpu.addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ((cpu.pc & 0xFF00) | ((cpu.pc + cpu.addr_rel) & 0xFF), bmi_wait3);
        }
        cpu.pc += (int8_t)cpu.addr_rel;
    }
    NEXT_INSTRUCTION(bmi_fetch_wait);
}

void bpl_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, bpl_wait1);
    cpu.addr_rel = bus.data;
    if (!(cpu.p & FLAG_N)) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu.pc, bpl_wait2); // Dummy read
        if ((cpu.pc & 0xFF00) != ((cpu.pc + cpu.addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ((cpu.pc & 0xFF00) | ((cpu.pc + cpu.addr_rel) & 0xFF), bpl_wait3);
        }
        cpu.pc += (int8_t)cpu.addr_rel;
    }
    NEXT_INSTRUCTION(bpl_fetch_wait);
}

void bvc_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, bvc_wait1);
    cpu.addr_rel = bus.data;
    if (!(cpu.p & FLAG_V)) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu.pc, bvc_wait2); // Dummy read
        if ((cpu.pc & 0xFF00) != ((cpu.pc + cpu.addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ((cpu.pc & 0xFF00) | ((cpu.pc + cpu.addr_rel) & 0xFF), bvc_wait3);
        }
        cpu.pc += (int8_t)cpu.addr_rel;
    }
    NEXT_INSTRUCTION(bvc_fetch_wait);
}

void bvs_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, bvs_wait1);
    cpu.addr_rel = bus.data;
    if (cpu.p & FLAG_V) {
        // Branch taken
        WAIT_READY_THEN_READ(cpu.pc, bvs_wait2); // Dummy read
        if ((cpu.pc & 0xFF00) != ((cpu.pc + cpu.addr_rel) & 0xFF00)) {
            // Page crossed - extra cycle
            WAIT_READY_THEN_READ((cpu.pc & 0xFF00) | ((cpu.pc + cpu.addr_rel) & 0xFF), bvs_wait3);
        }
        cpu.pc += (int8_t)cpu.addr_rel;
    }
    NEXT_INSTRUCTION(bvs_fetch_wait);
}

// Jump Instructions
void jmp_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, jmp_abs_wait1);
    cpu.addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, jmp_abs_wait2);
    cpu.addr_abs |= (bus.data << 8);
    cpu.pc = cpu.addr_abs;
    NEXT_INSTRUCTION(jmp_abs_fetch_wait);
}

void jmp_indirect_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, jmp_ind_wait1);
    cpu.lo = bus.data;
    WAIT_READY_THEN_READ(cpu.pc++, jmp_ind_wait2);
    cpu.hi = bus.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    WAIT_READY_THEN_READ(cpu.addr_abs, jmp_ind_wait3);
    cpu.lo = bus.data;
    
    // 6502 bug: if low byte is $FF, high byte wraps within same page
    if ((cpu.addr_abs & 0xFF) == 0xFF) {
        cpu.addr_abs = (cpu.addr_abs & 0xFF00);
    } else {
        cpu.addr_abs++;
    }
    
    WAIT_READY_THEN_READ(cpu.addr_abs, jmp_ind_wait4);
    cpu.hi = bus.data;
    cpu.pc = (cpu.hi << 8) | cpu.lo;
    NEXT_INSTRUCTION(jmp_ind_fetch_wait);
}

// Subroutine Instructions
void jsr_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, jsr_wait1);
    cpu.lo = bus.data;
    WAIT_READY_THEN_READ(0x0100 + cpu.sp, jsr_wait2); // Dummy read from stack
    cpu_push((cpu.pc >> 8) & 0xFF); // Push PC high byte (return address - 1)
    cpu_push(cpu.pc & 0xFF);         // Push PC low byte
    WAIT_READY_THEN_READ(cpu.pc++, jsr_wait3);
    cpu.hi = bus.data;
    cpu.pc = (cpu.hi << 8) | cpu.lo;
    NEXT_INSTRUCTION(jsr_fetch_wait);
}

// void rts_func(void) {
//     WAIT_READY_THEN_READ(cpu.pc, rts_wait1); // Dummy read
//     WAIT_READY_THEN_READ(0x0100 + cpu.sp, rts_wait2); // Dummy read from stack
//     cpu.lo = cpu_pull(); // Pull PC low byte
//     cpu.hi = cpu_pull(); // Pull PC high byte
//     cpu.pc = (cpu.hi << 8) | cpu.lo;
//     WAIT_READY_