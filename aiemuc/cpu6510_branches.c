#include "cpu6510.h"

// ============================================================================
// MOS 6510 BRANCH AND JUMP INSTRUCTIONS
// ============================================================================

// Branch Instructions
void bcc_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_C)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev);
}

void bcs_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_C) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev);
}

void beq_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_Z) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev);
}

void bne_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_Z)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev);
}

void bmi_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_N) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev);
}

void bpl_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_N)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev);
}

void bvc_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_V)) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev);
}

void bvs_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t rel_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_V) {
        // Branch taken
        CPU_READY_OR_STALL(cpu_dev);
        (void)cpu_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_READY_OR_STALL(cpu_dev);
            (void)cpu_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    NEXT_INSTRUCTION(cpu_dev);
}

// Jump Instructions
void jmp_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    cpu_dev->pc = cpu_dev->address;
    NEXT_INSTRUCTION(cpu_dev);
}

void jmp_indirect_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t pc_lo = cpu_read_cycle(cpu_dev, cpu_dev->address);
    
    // 6502 bug: if low byte is $FF, high byte wraps within same page
    if ((cpu_dev->address & 0xFF) == 0xFF) {
        cpu_dev->address = (cpu_dev->address & 0xFF00);
    } else {
        cpu_dev->address++;
    }
    
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t pc_hi = cpu_read_cycle(cpu_dev, cpu_dev->address);
    cpu_dev->pc = (pc_hi << 8) | pc_lo;
    NEXT_INSTRUCTION(cpu_dev);
}

// Subroutine Instructions
void jsr_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp); // Dummy read from stack
    cpu_push(cpu_dev, (cpu_dev->pc >> 8) & 0xFF); // Push PC high byte (return address - 1)
    cpu_push(cpu_dev, cpu_dev->pc & 0xFF);         // Push PC low byte
    CPU_READY_OR_STALL(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->pc = (addr_hi << 8) | addr_lo;
    NEXT_INSTRUCTION(cpu_dev);
}
