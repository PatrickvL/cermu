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

FAM65XX_OPCODE_PROTO(fam65xx_op_cmp_immediate) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_cmp); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cmp_zero_page) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_cmp); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cmp_zero_page_x) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_cmp); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cmp_absolute) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_cmp); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cmp_absolute_x) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_cmp); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cmp_absolute_y) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_cmp); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cmp_indirect_x) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_cmp); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cmp_indirect_y) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_cmp); }

// CPX - Compare X Register
static inline void fam65xx_op_cpx(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->x - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->x >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_cpx_immediate) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_cpx); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cpx_zero_page) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_cpx); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cpx_absolute) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_cpx); }

// CPY - Compare Y Register
static inline void fam65xx_op_cpy(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->y - value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->y >= value);
    fam65xx_set_nz_flags(cpu, result & 0xFF);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_cpy_immediate) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_cpy); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cpy_zero_page) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_cpy); }
FAM65XX_OPCODE_PROTO(fam65xx_op_cpy_absolute) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_cpy); }

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

FAM65XX_OPCODE_PROTO(fam65xx_op_bit_zero_page) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_bit); }
FAM65XX_OPCODE_PROTO(fam65xx_op_bit_absolute) { FAM65XX_PROTO_RETURN fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_bit); }

// ============================================================================
//  NOP OPERATIONS
// ============================================================================

// NOP - No Operation

FAM65XX_OPCODE_PROTO(fam65xx_op_nop) {
    // Implied addressing mode; does not use the helper.
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read cycle
    FAM65XX_OPCODE_FOOTER(cpu);
}

// "special" NOP (unofficial, opcode 0x89)
FAM65XX_OPCODE_PROTO(fam65xx_op_nop_immediate_special) {
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
