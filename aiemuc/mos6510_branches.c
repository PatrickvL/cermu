#include "mos6510.h"

// ============================================================================
// MOS 6510 BRANCH AND JUMP INSTRUCTIONS
// ============================================================================

// Branch Instructions
void bcc_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t rel_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_C)) {
        // Branch taken
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_INTRA_CYCLE(cpu_dev);
            (void)mos6510_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    CPU_OPCODE_FOOTER(cpu_dev);
}

void bcs_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t rel_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_C) {
        // Branch taken
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_INTRA_CYCLE(cpu_dev);
            (void)mos6510_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    CPU_OPCODE_FOOTER(cpu_dev);
}

void beq_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t rel_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_Z) {
        // Branch taken
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_INTRA_CYCLE(cpu_dev);
            (void)mos6510_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    CPU_OPCODE_FOOTER(cpu_dev);
}

void bne_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t rel_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_Z)) {
        // Branch taken
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_INTRA_CYCLE(cpu_dev);
            (void)mos6510_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    CPU_OPCODE_FOOTER(cpu_dev);
}

void bmi_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t rel_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_N) {
        // Branch taken
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_INTRA_CYCLE(cpu_dev);
            (void)mos6510_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    CPU_OPCODE_FOOTER(cpu_dev);
}

void bpl_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t rel_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_N)) {
        // Branch taken
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_INTRA_CYCLE(cpu_dev);
            (void)mos6510_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    CPU_OPCODE_FOOTER(cpu_dev);
}

void bvc_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t rel_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    if (!(cpu_dev->p & FLAG_V)) {
        // Branch taken
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_INTRA_CYCLE(cpu_dev);
            (void)mos6510_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    CPU_OPCODE_FOOTER(cpu_dev);
}

void bvs_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t rel_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    if (cpu_dev->p & FLAG_V) {
        // Branch taken
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        if ((cpu_dev->pc & 0xFF00) != ((cpu_dev->pc + rel_addr) & 0xFF00)) {
            // Page crossed - extra cycle
            CPU_INTRA_CYCLE(cpu_dev);
            (void)mos6510_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | ((cpu_dev->pc + rel_addr) & 0xFF));
        }
        cpu_dev->pc += (int8_t)rel_addr;
    }
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Jump Instructions
void jmp_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    cpu_dev->pc = cpu_dev->address;
    CPU_OPCODE_FOOTER(cpu_dev);
}

void jmp_indirect_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t pc_lo = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    
    // 6502 bug: if low byte is $FF, high byte wraps within same page
    if ((cpu_dev->address & 0xFF) == 0xFF) {
        cpu_dev->address = (cpu_dev->address & 0xFF00);
    } else {
        cpu_dev->address++;
    }
    
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t pc_hi = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    cpu_dev->pc = (pc_hi << 8) | pc_lo;
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Subroutine Instructions
void jsr_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp); // Dummy read from stack
    cpu_push(cpu_dev, (cpu_dev->pc >> 8) & 0xFF); // Push PC high byte (return address - 1)
    cpu_push(cpu_dev, cpu_dev->pc & 0xFF);         // Push PC low byte
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->pc = (addr_hi << 8) | addr_lo;
    CPU_OPCODE_FOOTER(cpu_dev);
}
