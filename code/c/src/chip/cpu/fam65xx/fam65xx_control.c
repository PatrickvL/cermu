#include "fam65xx_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY CONTROL FLOW OPERATIONS
// ============================================================================

// Branch helper function implementation
static inline void fam65xx_op_branch_helper(fam65xx_t* cpu, bool condition) {

    int8_t offset = (int8_t)fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    
    if (condition) {
        uint16_t old_pc = cpu->pc;
        cpu->pc += offset;
        
        // Extra cycle for taking the branch
    
        (void)fam65xx_read_cycle(cpu, old_pc); // Dummy read
        
        // Extra cycle if page boundary crossed
        if ((old_pc & 0xFF00) != (cpu->pc & 0xFF00)) {
        
            (void)fam65xx_read_cycle(cpu, (old_pc & 0xFF00) | (cpu->pc & 0xFF)); // Dummy read
        }
    }
    FAM65XX_OPCODE_FOOTER(cpu);
}

// BPL - Branch if Positive
void fam65xx_op_bpl(fam65xx_t* cpu) {
    fam65xx_op_branch_helper(cpu, !fam65xx_get_flag(cpu, FLAG_N));
}

// BMI - Branch if Minus
void fam65xx_op_bmi(fam65xx_t* cpu) {
    fam65xx_op_branch_helper(cpu, fam65xx_get_flag(cpu, FLAG_N));
}

// BVC - Branch if Overflow Clear
void fam65xx_op_bvc(fam65xx_t* cpu) {
    fam65xx_op_branch_helper(cpu, !fam65xx_get_flag(cpu, FLAG_V));
}

// BVS - Branch if Overflow Set
void fam65xx_op_bvs(fam65xx_t* cpu) {
    fam65xx_op_branch_helper(cpu, fam65xx_get_flag(cpu, FLAG_V));
}

// BCC - Branch if Carry Clear
void fam65xx_op_bcc(fam65xx_t* cpu) {
    fam65xx_op_branch_helper(cpu, !fam65xx_get_flag(cpu, FLAG_C));
}

// BCS - Branch if Carry Set
void fam65xx_op_bcs(fam65xx_t* cpu) {
    fam65xx_op_branch_helper(cpu, fam65xx_get_flag(cpu, FLAG_C));
}

// BNE - Branch if Not Equal
void fam65xx_op_bne(fam65xx_t* cpu) {
    fam65xx_op_branch_helper(cpu, !fam65xx_get_flag(cpu, FLAG_Z));
}

// BEQ - Branch if Equal
void fam65xx_op_beq(fam65xx_t* cpu) {
    fam65xx_op_branch_helper(cpu, fam65xx_get_flag(cpu, FLAG_Z));
}

// ============================================================================
// JUMP AND SUBROUTINE OPERATIONS
// ============================================================================

// JMP - Jump Absolute
void fam65xx_op_jmp_absolute(fam65xx_t* cpu) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    FAM65XX_OPCODE_FOOTER(cpu);
}

// JMP - Jump Indirect
void fam65xx_op_jmp_indirect(fam65xx_t* cpu) {

    uint8_t ptr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t ptr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;
    

    uint8_t addr_lo = fam65xx_read_cycle(cpu, ptr);  // Bus read cycle

    // Bug in 6502: if ptr is $xxFF, high byte comes from $xx00 instead of $xx00+1
    uint8_t addr_hi = fam65xx_read_cycle(cpu, (ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    FAM65XX_OPCODE_FOOTER(cpu);
}

// JSR - Jump to Subroutine
void fam65xx_op_jsr(fam65xx_t* cpu) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    

    (void)fam65xx_read_cycle(cpu, 0x0100 | cpu->sp); // Dummy read
    
    // Push return address - 1 (JSR pushes PC-1)
    uint16_t return_addr = cpu->pc; // PC already points to high byte
    fam65xx_push(cpu, (return_addr >> 8) & 0xFF);
    fam65xx_push(cpu, return_addr & 0xFF);
    

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc);  // T1: Dummy read
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    FAM65XX_OPCODE_FOOTER(cpu);
}

// RTS - Return from Subroutine
void fam65xx_op_rts(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    
    // Pull return address
    uint8_t addr_lo = fam65xx_pull(cpu);
    uint8_t addr_hi = fam65xx_pull(cpu);
    cpu->pc = ((addr_hi << 8) | addr_lo) + 1; // RTS increments PC
    

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// INTERRUPT OPERATIONS
// ============================================================================

// BRK - Break
void fam65xx_op_brk(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc++); // Read and discard next byte
    
    // Push PC+1 (BRK increments PC before pushing)
    fam65xx_push(cpu, (cpu->pc >> 8) & 0xFF);
    fam65xx_push(cpu, cpu->pc & 0xFF);
    
    // Push status with B flag set
    fam65xx_push(cpu, cpu->p | FLAG_B | FLAG_U);
    
    // Set interrupt disable flag
    fam65xx_set_flag(cpu, FLAG_I, true);
    
    // Load IRQ vector

    uint8_t addr_lo = fam65xx_read_cycle(cpu, 0xFFFE);  // Bus read cycle

    uint8_t addr_hi = fam65xx_read_cycle(cpu, 0xFFFF);  // Bus read cycle
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    FAM65XX_OPCODE_FOOTER(cpu);
}

// RTI - Return from Interrupt
void fam65xx_op_rti(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    
    // Pull status register
    cpu->p = fam65xx_pull(cpu);
    cpu->p |= FLAG_U; // Unused flag always set
    
    // Pull program counter
    uint8_t addr_lo = fam65xx_pull(cpu);
    uint8_t addr_hi = fam65xx_pull(cpu);
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// FLAG SET/CLEAR OPERATIONS
// ============================================================================

// CLC - Clear Carry Flag
void fam65xx_op_clc(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_C, false);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// SEC - Set Carry Flag
void fam65xx_op_sec(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_C, true);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// CLI - Clear Interrupt Disable Flag
void fam65xx_op_cli(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_I, false);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// SEI - Set Interrupt Disable Flag
void fam65xx_op_sei(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_I, true);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// CLV - Clear Overflow Flag
void fam65xx_op_clv(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_V, false);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// CLD - Clear Decimal Flag
void fam65xx_op_cld(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_D, false);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// SED - Set Decimal Flag
void fam65xx_op_sed(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_D, true);
    FAM65XX_OPCODE_FOOTER(cpu);
}
