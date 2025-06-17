#include "mos6502_family_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY MISCELLANEOUS OPERATIONS
// ============================================================================

// ============================================================================
// COMPARE OPERATIONS
// ============================================================================

// CMP - Compare Accumulator
void mos6502_family_cmp_immediate(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_imm, mos6502_family_op_cmp);
}

void mos6502_family_cmp_zero_page(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_zp, mos6502_family_op_cmp);
}

void mos6502_family_cmp_zero_page_x(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_zpx, mos6502_family_op_cmp);
}

void mos6502_family_cmp_absolute(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_abs, mos6502_family_op_cmp);
}

void mos6502_family_cmp_absolute_x(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_absx, mos6502_family_op_cmp);
}

void mos6502_family_cmp_absolute_y(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_absy, mos6502_family_op_cmp);
}

void mos6502_family_cmp_indirect_x(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_indx, mos6502_family_op_cmp);
}

void mos6502_family_cmp_indirect_y(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_indy, mos6502_family_op_cmp);
}

// CPX - Compare X Register
void mos6502_family_cpx_immediate(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_imm, mos6502_family_op_cpx);
}

void mos6502_family_cpx_zero_page(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_zp, mos6502_family_op_cpx);
}

void mos6502_family_cpx_absolute(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_abs, mos6502_family_op_cpx);
}

// CPY - Compare Y Register
void mos6502_family_cpy_immediate(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_imm, mos6502_family_op_cpy);
}

void mos6502_family_cpy_zero_page(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_zp, mos6502_family_op_cpy);
}

void mos6502_family_cpy_absolute(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_abs, mos6502_family_op_cpy);
}

// ============================================================================
// BIT TEST OPERATIONS
// ============================================================================

// BIT - Bit Test
void mos6502_family_bit_zero_page(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_zp, mos6502_family_op_bit);
}

void mos6502_family_bit_absolute(mos6502_family_t* cpu) {
    mos6502_family_arithmetic_helper(cpu, mos6502_family_addr_abs, mos6502_family_op_bit);
}

// ============================================================================
// NOP OPERATIONS (including illegal NOP variants)
// ============================================================================

// NOP - No Operation
void mos6502_family_nop(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// NOP - No Operation (Immediate) - illegal opcode
void mos6502_family_nop_immediate(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc++); // Read and discard immediate value
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// NOP - No Operation (Zero Page) - illegal opcode
void mos6502_family_nop_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, addr); // Dummy read
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// NOP - No Operation (Zero Page,X) - illegal opcode
void mos6502_family_nop_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    uint8_t addr = (base + cpu->x) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, addr); // Dummy read
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// NOP - No Operation (Absolute) - illegal opcode
void mos6502_family_nop_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    uint16_t addr = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, addr); // Dummy read
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// NOP - No Operation (Absolute,X) - illegal opcode
void mos6502_family_nop_absolute_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    uint16_t addr = base_addr + cpu->x;
    
    // Check for page boundary crossing
    if ((base_addr & 0xFF00) != (addr & 0xFF00)) {
        MOS6502_FAMILY_INTRA_CYCLE(cpu);
        (void)mos6502_family_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    }
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, addr); // Dummy read
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// JAM OPERATION (illegal)
// ============================================================================

// JAM - Halt and Catch Fire (illegal opcode)
void mos6502_family_jam(mos6502_family_t* cpu) {
    // JAM instruction halts the CPU by entering an infinite loop
    // The program counter is not incremented
    // This effectively freezes the CPU until reset
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Read current PC
    
    // Decrement PC to stay on the same instruction
    cpu->pc--;
    
    // Do not call MOS6502_FAMILY_OPCODE_FOOTER - this breaks the normal flow
    // The CPU will keep executing this instruction indefinitely
}
