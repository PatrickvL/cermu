#include "fam65xx_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY MISCELLANEOUS OPERATIONS
// ============================================================================

// ============================================================================
// COMPARE OPERATIONS
// ============================================================================

// CMP - Compare Accumulator
static inline void fam65xx_op_cmp(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->a - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->a >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

void fam65xx_op_cmp_immediate(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_cmp); }
void fam65xx_op_cmp_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_cmp); }
void fam65xx_op_cmp_zero_page_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_cmp); }
void fam65xx_op_cmp_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_cmp); }
void fam65xx_op_cmp_absolute_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_cmp); }
void fam65xx_op_cmp_absolute_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_cmp); }
void fam65xx_op_cmp_indirect_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_cmp); }
void fam65xx_op_cmp_indirect_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_cmp); }

// CPX - Compare X Register
static inline void fam65xx_op_cpx(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->x - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->x >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

void fam65xx_op_cpx_immediate(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_cpx); }
void fam65xx_op_cpx_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_cpx); }
void fam65xx_op_cpx_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_cpx); }

// CPY - Compare Y Register
static inline void fam65xx_op_cpy(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->y - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->y >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

void fam65xx_op_cpy_immediate(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_cpy); }
void fam65xx_op_cpy_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_cpy); }
void fam65xx_op_cpy_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_cpy); }

// ============================================================================
// BIT TEST OPERATIONS
// ============================================================================

// BIT - Bit Test
static inline void fam65xx_op_bit(fam65xx_t* cpu, uint8_t value) {
    uint8_t result = cpu->a & value;
    fam65xx_set_flag(cpu, FLAG_Z, result == 0);
    fam65xx_set_flag(cpu, FLAG_N, (value & 0x80) != 0);
    fam65xx_set_flag(cpu, FLAG_V, (value & 0x40) != 0);
}

void fam65xx_op_bit_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_bit); }
void fam65xx_op_bit_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_bit); }

// ============================================================================
//  NOP OPERATIONS
// ============================================================================

// NOP - No Operation

void fam65xx_op_nop(fam65xx_t* cpu) {
    // Implied addressing mode; does not use the helper.
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read cycle
    FAM65XX_OPCODE_FOOTER(cpu);
}

// "special" NOP (unofficial, opcode 0x89)
void fam65xx_op_nop_immediate_special(fam65xx_t* cpu) {
    /* Hardware-accurate implementation:
     * - Reads the immediate operand.
     * - Performs a logical AND between A and the immediate operand, but does NOT store the result.
     * - Sets N and Z flags as if AND A,#imm was executed.
     * - Does NOT modify A or memory.
     */
    uint8_t operand = fam65xx_read_cycle(cpu, cpu->pc++); // Fetch immediate operand
    uint8_t result = cpu->a & operand;
    fam65xx_set_nz_flags(cpu, result);
    FAM65XX_OPCODE_FOOTER(cpu);
}
