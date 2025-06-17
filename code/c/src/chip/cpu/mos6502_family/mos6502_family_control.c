#include "mos6502_family_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY CONTROL FLOW OPERATIONS
// ============================================================================
// Note: mos6502_family_branch_helper is now inlined in the header for performance

// BPL - Branch if Positive
void mos6502_family_bpl(mos6502_family_t* cpu) {
    mos6502_family_branch_helper(cpu, !mos6502_family_get_flag(cpu, FLAG_N));
}

// BMI - Branch if Minus
void mos6502_family_bmi(mos6502_family_t* cpu) {
    mos6502_family_branch_helper(cpu, mos6502_family_get_flag(cpu, FLAG_N));
}

// BVC - Branch if Overflow Clear
void mos6502_family_bvc(mos6502_family_t* cpu) {
    mos6502_family_branch_helper(cpu, !mos6502_family_get_flag(cpu, FLAG_V));
}

// BVS - Branch if Overflow Set
void mos6502_family_bvs(mos6502_family_t* cpu) {
    mos6502_family_branch_helper(cpu, mos6502_family_get_flag(cpu, FLAG_V));
}

// BCC - Branch if Carry Clear
void mos6502_family_bcc(mos6502_family_t* cpu) {
    mos6502_family_branch_helper(cpu, !mos6502_family_get_flag(cpu, FLAG_C));
}

// BCS - Branch if Carry Set
void mos6502_family_bcs(mos6502_family_t* cpu) {
    mos6502_family_branch_helper(cpu, mos6502_family_get_flag(cpu, FLAG_C));
}

// BNE - Branch if Not Equal
void mos6502_family_bne(mos6502_family_t* cpu) {
    mos6502_family_branch_helper(cpu, !mos6502_family_get_flag(cpu, FLAG_Z));
}

// BEQ - Branch if Equal
void mos6502_family_beq(mos6502_family_t* cpu) {
    mos6502_family_branch_helper(cpu, mos6502_family_get_flag(cpu, FLAG_Z));
}

// ============================================================================
// JUMP AND SUBROUTINE OPERATIONS
// ============================================================================

// JMP - Jump Absolute
void mos6502_family_jmp_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// JMP - Jump Indirect
void mos6502_family_jmp_indirect(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t ptr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t ptr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, ptr);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    // Bug in 6502: if ptr is $xxFF, high byte comes from $xx00 instead of $xx00+1
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, (ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// JSR - Jump to Subroutine
void mos6502_family_jsr(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, 0x0100 | cpu->sp); // Dummy read
    
    // Push return address - 1 (JSR pushes PC-1)
    uint16_t return_addr = cpu->pc; // PC already points to high byte
    mos6502_family_push(cpu, (return_addr >> 8) & 0xFF);
    mos6502_family_push(cpu, return_addr & 0xFF);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// RTS - Return from Subroutine
void mos6502_family_rts(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    
    // Pull return address
    uint8_t addr_lo = mos6502_family_pull(cpu);
    uint8_t addr_hi = mos6502_family_pull(cpu);
    cpu->pc = ((addr_hi << 8) | addr_lo) + 1; // RTS increments PC
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// INTERRUPT OPERATIONS
// ============================================================================

// BRK - Break
void mos6502_family_brk(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc++); // Read and discard next byte
    
    // Push PC+1 (BRK increments PC before pushing)
    mos6502_family_push(cpu, (cpu->pc >> 8) & 0xFF);
    mos6502_family_push(cpu, cpu->pc & 0xFF);
    
    // Push status with B flag set
    mos6502_family_push(cpu, cpu->p | FLAG_B | FLAG_U);
    
    // Set interrupt disable flag
    mos6502_family_set_flag(cpu, FLAG_I, true);
    
    // Load IRQ vector
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, 0xFFFE);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, 0xFFFF);
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// RTI - Return from Interrupt
void mos6502_family_rti(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    
    // Pull status register
    cpu->p = mos6502_family_pull(cpu);
    cpu->p |= FLAG_U; // Unused flag always set
    
    // Pull program counter
    uint8_t addr_lo = mos6502_family_pull(cpu);
    uint8_t addr_hi = mos6502_family_pull(cpu);
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// FLAG SET/CLEAR OPERATIONS
// ============================================================================

// CLC - Clear Carry Flag
void mos6502_family_clc(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_set_flag(cpu, FLAG_C, false);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// SEC - Set Carry Flag
void mos6502_family_sec(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_set_flag(cpu, FLAG_C, true);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// CLI - Clear Interrupt Disable Flag
void mos6502_family_cli(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_set_flag(cpu, FLAG_I, false);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// SEI - Set Interrupt Disable Flag
void mos6502_family_sei(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_set_flag(cpu, FLAG_I, true);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// CLV - Clear Overflow Flag
void mos6502_family_clv(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_set_flag(cpu, FLAG_V, false);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// CLD - Clear Decimal Flag
void mos6502_family_cld(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_set_flag(cpu, FLAG_D, false);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// SED - Set Decimal Flag
void mos6502_family_sed(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_set_flag(cpu, FLAG_D, true);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}
