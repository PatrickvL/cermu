#include "fam65xx_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY CONTROL FLOW OPERATIONS
// ============================================================================

// Branch helper function implementation
FAM65XX_HELPER_PROTO(fam65xx_op_branch_helper), bool condition) {

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
    FAM65XX_HELPER_FOOTER(cpu);
}

// BPL - Branch if Positive
FAM65XX_OPCODE_PROTO(fam65xx_op_bpl) { FAM65XX_PROTO_RETURN FAM65XX_BRANCH_HELPER(cpu, !fam65xx_get_flag(cpu, FLAG_N)); }

// BMI - Branch if Minus
FAM65XX_OPCODE_PROTO(fam65xx_op_bmi) { FAM65XX_PROTO_RETURN FAM65XX_BRANCH_HELPER(cpu, fam65xx_get_flag(cpu, FLAG_N)); }

// BVC - Branch if Overflow Clear
FAM65XX_OPCODE_PROTO(fam65xx_op_bvc) { FAM65XX_PROTO_RETURN FAM65XX_BRANCH_HELPER(cpu, !fam65xx_get_flag(cpu, FLAG_V)); }

// BVS - Branch if Overflow Set
FAM65XX_OPCODE_PROTO(fam65xx_op_bvs) { FAM65XX_PROTO_RETURN FAM65XX_BRANCH_HELPER(cpu, fam65xx_get_flag(cpu, FLAG_V)); }

// BCC - Branch if Carry Clear
FAM65XX_OPCODE_PROTO(fam65xx_op_bcc) { FAM65XX_PROTO_RETURN FAM65XX_BRANCH_HELPER(cpu, !fam65xx_get_flag(cpu, FLAG_C)); }

// BCS - Branch if Carry Set
FAM65XX_OPCODE_PROTO(fam65xx_op_bcs) { FAM65XX_PROTO_RETURN FAM65XX_BRANCH_HELPER(cpu, fam65xx_get_flag(cpu, FLAG_C)); }

// BNE - Branch if Not Equal
FAM65XX_OPCODE_PROTO(fam65xx_op_bne) { FAM65XX_PROTO_RETURN FAM65XX_BRANCH_HELPER(cpu, !fam65xx_get_flag(cpu, FLAG_Z)); }

// BEQ - Branch if Equal
FAM65XX_OPCODE_PROTO(fam65xx_op_beq) { FAM65XX_PROTO_RETURN FAM65XX_BRANCH_HELPER(cpu, fam65xx_get_flag(cpu, FLAG_Z)); }

// ============================================================================
// JUMP AND SUBROUTINE OPERATIONS
// ============================================================================

// JMP - Jump Absolute
FAM65XX_OPCODE_PROTO(fam65xx_op_jmp_absolute) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->pc = (addr_hi << 8) | addr_lo;
    
    FAM65XX_OPCODE_FOOTER(cpu);
}

// JMP - Jump Indirect
FAM65XX_OPCODE_PROTO(fam65xx_op_jmp_indirect) {

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
FAM65XX_OPCODE_PROTO(fam65xx_op_jsr) {

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
FAM65XX_OPCODE_PROTO(fam65xx_op_rts) {

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
FAM65XX_OPCODE_PROTO(fam65xx_op_brk) {
#if 0
    (void)fam65xx_read_cycle(cpu, cpu->pc++); // Read and discard next byte
    fam65xx_interrupt_sequence(cpu, cpu->p | FLAG_B, 0xFFFE);
    FAM65XX_OPCODE_FOOTER(cpu);
else // As long as the IRQ vector ends up at BRK, we replace threaded dispatch with a loop
    do {
        (void)fam65xx_read_cycle(cpu, cpu->pc++);
        fam65xx_interrupt_sequence(cpu, cpu->p | FLAG_B, 0xFFFE);
        // Fetch next opcode
        uint8_t opcode = fam65xx_read_cycle(cpu, cpu->pc);
        if (opcode != 0x00) { // 0x00 is BRK
            (cpu)->opcode_handlers[opcode](cpu);
            return;
        }
        // If next opcode is BRK, repeat loop
    } while (1);
#endif
}

// RTI - Return from Interrupt
FAM65XX_OPCODE_PROTO(fam65xx_op_rti) {

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
FAM65XX_OPCODE_PROTO(fam65xx_op_clc) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_C, false);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// SEC - Set Carry Flag
FAM65XX_OPCODE_PROTO(fam65xx_op_sec) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_C, true);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// CLI - Clear Interrupt Disable Flag
FAM65XX_OPCODE_PROTO(fam65xx_op_cli) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_I, false);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// SEI - Set Interrupt Disable Flag
FAM65XX_OPCODE_PROTO(fam65xx_op_sei) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_I, true);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// CLV - Clear Overflow Flag
FAM65XX_OPCODE_PROTO(fam65xx_op_clv) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_V, false);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// CLD - Clear Decimal Flag
FAM65XX_OPCODE_PROTO(fam65xx_op_cld) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_D, false);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// SED - Set Decimal Flag
FAM65XX_OPCODE_PROTO(fam65xx_op_sed) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_set_flag(cpu, FLAG_D, true);
    FAM65XX_OPCODE_FOOTER(cpu);
}
