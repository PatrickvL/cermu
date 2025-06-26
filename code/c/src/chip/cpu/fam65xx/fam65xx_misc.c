#include "fam65xx_core.h"
#include "fam65xx_arithmetic.h"

// ============================================================================
// SHARED MOS 6502 FAMILY MISCELLANEOUS OPERATIONS
// ============================================================================

static inline void fam65xx_op_cmp(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->a - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->a >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

static inline void fam65xx_op_cpx(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->x - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->x >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

static inline void fam65xx_op_cpy(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->y - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->y >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

static inline void fam65xx_op_bit(fam65xx_t* cpu, uint8_t value) {
    uint8_t result = cpu->a & value;
    fam65xx_set_flag(cpu, FLAG_Z, result == 0);
    fam65xx_set_flag(cpu, FLAG_N, (value & 0x80) != 0);
    fam65xx_set_flag(cpu, FLAG_V, (value & 0x40) != 0);
}

// ============================================================================
// COMPARE OPERATIONS
// ============================================================================

// CMP - Compare Accumulator
void fam65xx_op_cmp_immediate(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_cmp);
}

void fam65xx_op_cmp_zero_page(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_cmp);
}

void fam65xx_op_cmp_zero_page_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_cmp);
}

void fam65xx_op_cmp_absolute(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_cmp);
}

void fam65xx_op_cmp_absolute_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_cmp);
}

void fam65xx_op_cmp_absolute_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_cmp);
}

void fam65xx_op_cmp_indirect_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_cmp);
}

void fam65xx_op_cmp_indirect_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_cmp);
}

// CPX - Compare X Register
void fam65xx_op_cpx_immediate(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_cpx);
}

void fam65xx_op_cpx_zero_page(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_cpx);
}

void fam65xx_op_cpx_absolute(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_cpx);
}

// CPY - Compare Y Register
void fam65xx_op_cpy_immediate(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_cpy);
}

void fam65xx_op_cpy_zero_page(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_cpy);
}

void fam65xx_op_cpy_absolute(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_cpy);
}

// ============================================================================
// BIT TEST OPERATIONS
// ============================================================================

// BIT - Bit Test
void fam65xx_op_bit_zero_page(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_bit);
}

void fam65xx_op_bit_absolute(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_bit);
}

// ============================================================================
// NOP OPERATIONS (including illegal NOP variants)
// ============================================================================

// NOP - No Operation  
void fam65xx_op_nop(fam65xx_t* cpu) {
    (void)fam65xx_read_cycle(cpu, cpu->pc);      // T1: Dummy read
    FAM65XX_OPCODE_FOOTER(cpu);
}

// NOP - No Operation (Immediate) - illegal opcode
void fam65xx_op_nop_immediate(fam65xx_t* cpu) {
    (void)fam65xx_read_cycle(cpu, cpu->pc++); // Read and discard immediate value
    FAM65XX_OPCODE_FOOTER(cpu);
}

// NOP - No Operation (Zero Page) - illegal opcode
void fam65xx_op_nop_zero_page(fam65xx_t* cpu) {
    uint8_t addr = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    (void)fam65xx_read_cycle(cpu, addr); // Dummy read
    FAM65XX_OPCODE_FOOTER(cpu);
}

// NOP - No Operation (Zero Page,X) - illegal opcode
void fam65xx_op_nop_zero_page_x(fam65xx_t* cpu) {
    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    uint8_t addr = (base + cpu->x) & 0xFF;
    (void)fam65xx_read_cycle(cpu, addr); // Dummy read
    FAM65XX_OPCODE_FOOTER(cpu);
}

// NOP - No Operation (Absolute) - illegal opcode
void fam65xx_op_nop_absolute(fam65xx_t* cpu) {
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint16_t addr = (addr_hi << 8) | addr_lo;
    (void)fam65xx_read_cycle(cpu, addr); // Dummy read
    FAM65XX_OPCODE_FOOTER(cpu);
}

// NOP - No Operation (Absolute,X) - illegal opcode
void fam65xx_op_nop_absolute_x(fam65xx_t* cpu) {
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    uint16_t addr = base_addr + cpu->x;
    
    // Check for page boundary crossing
    if ((base_addr & 0xFF00) != (addr & 0xFF00)) {
        (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    }
    (void)fam65xx_read_cycle(cpu, addr); // Dummy read
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// JAM OPERATION (illegal)
// ============================================================================

// JAM - Halt and Catch Fire (illegal opcode)
void fam65xx_op_jam(fam65xx_t* cpu) {
    // JAM instruction halts the CPU by entering an infinite loop
    // The program counter is not incremented
    // This effectively freezes the CPU until reset
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Read current PC
    
    // Decrement PC to stay on the same instruction
    cpu->pc--;
    
    // Do not call FAM65XX_OPCODE_FOOTER - this breaks the normal flow
    // The CPU will keep executing this instruction indefinitely
}
