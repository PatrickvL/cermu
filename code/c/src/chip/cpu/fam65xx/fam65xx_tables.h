#pragma once
/*
 * fam65xx_tables.h - MOS 65xx Family CPU Lookup Tables and Enums
 *
 * This file contains:
 * - Addressing mode and operation enums
 * - Function pointer tables for addressing modes and operations
 * - Complete 256-entry opcode lookup table
 * - Compact opcode encoding macros
 *
 * Note: This file includes all operation files to eliminate forward declarations.
 * The lookup tables reference the actual function implementations directly.
 */

#include "fam65xx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ADDRESSING MODE AND OPERATION ENUMS
// ============================================================================
// Addressing modes describe how operands are fetched.
// Operations describe what the CPU does with those operands.
// These are combined in the opcode table to minimize redundancy.
//
// AM_NON (0): No addressing mode handler needed
//   - Used by: Implicit, Immediate, Accumulator, and Relative modes
//   - These modes either have no operand, operand in next byte, or
//     operate directly on registers without memory access

typedef enum {
    AM_NON = 0, /* No addressing handler (Implicit/Accumulator/Relative) */
    AM_IMM,     /* Immediate - operand is next byte */
    AM_ZER,     /* Zero Page - operand at $00nn */
    AM_ZPX,     /* Zero Page,X - operand at ($00nn + X) & 0xFF */
    AM_ZPY,     /* Zero Page,Y - operand at ($00nn + Y) & 0xFF */
    AM_ABS,     /* Absolute - operand at $nnnn */
    AM_ABX,     /* Absolute,X - operand at $nnnn + X */
    AM_ABY,     /* Absolute,Y - operand at $nnnn + Y */
    AM_IND,     /* Indirect - jump target at ($nnnn) */
    AM_INX,     /* Indexed Indirect - operand at (($nn + X) & 0xFF) */
    AM_INY,     /* Indirect Indexed - operand at ($nn) + Y */
    AM_COUNT
} AddrMode;

/* Aliases for documentation/clarity (all map to AM_NON) */
#define AM_IMP  AM_NON  /* Implied/Implicit - no operand */
#define AM_ACC  AM_NON  /* Accumulator - operate on A register */
#define AM_REL  AM_NON  /* Relative - branch offset */

typedef enum {
    OP_LDA, OP_LDX, OP_LDY,
    OP_STA, OP_STX, OP_STY,
    OP_ADC, OP_SBC,
    OP_AND, OP_ORA, OP_EOR,
    OP_CMP, OP_CPX, OP_CPY,
    OP_ASL, OP_LSR, OP_ROL, OP_ROR,
    OP_INC, OP_DEC,
    OP_INX, OP_INY, OP_DEX, OP_DEY,
    OP_TAX, OP_TAY, OP_TXA, OP_TYA, OP_TSX, OP_TXS,
    OP_PHA, OP_PHP, OP_PLA, OP_PLP,
    OP_BCC, OP_BCS, OP_BEQ, OP_BNE, OP_BMI, OP_BPL, OP_BVC, OP_BVS,
    OP_CLC, OP_SEC, OP_CLI, OP_SEI, OP_CLD, OP_SED, OP_CLV,
    OP_JMP, OP_JSR, OP_RTS, OP_RTI, OP_BRK,
    OP_BIT, OP_NOP, OP_JAM,
    // 65C02 enhancements
    OP_BRA,
    // Illegal opcodes - combination instructions
    OP_LAX, OP_SAX, OP_DCP, OP_ISC, OP_SLO, OP_RLA, OP_SRE, OP_RRA,
    // Illegal opcodes - special accumulator operations
    OP_ANC, OP_ASR, OP_ARR, OP_SBX,
    // Illegal opcodes - store with AND operations
    OP_SHA, OP_SHS, OP_SHX, OP_SHY, OP_LAS,
    // Illegal opcodes - special operations
    OP_XAA,
    OP_COUNT
} Operation;

#ifdef CHIPS_IMPL

// Include all function implementations to eliminate forward declarations
#include "fam65xx_addrmodes.h"
#include "fam65xx_ops.h"
#include "fam65xx_ops_part2.h"
#include "fam65xx_ops_part3.h"
#include "fam65xx_ops_rmw.h"
#include "fam65xx_ops_illegal.h"

/* Addressing mode table - function pointers ordered by enum */
static const cycle_fn_t fam65xx_addr_mode_table[AM_COUNT] = {
    NULL,    // AM_NON : No handler needed
    NULL,    // AM_IMM : No handler (handled in operation)
    am_zp,   // AM_ZER
    am_zpx,  // AM_ZPX
    am_zpy,  // AM_ZPY
    am_abs,  // AM_ABS
    am_abx,  // AM_ABX
    am_aby,  // AM_ABY
    am_ind,  // AM_IND
    am_idx,  // AM_INX
    am_idy,  // AM_INY
};

/* Operation table - function pointers ordered by enum */
static const cycle_fn_t fam65xx_op_handlers[OP_COUNT] = {
    op_lda, // OP_LDA
    op_ldx, // OP_LDX
    op_ldy, // OP_LDY,
    op_sta, // OP_STA
    op_stx, // OP_STX
    op_sty, // OP_STY,
    op_adc, // OP_ADC
    op_sbc, // OP_SBC
    op_and, // OP_AND
    op_ora, // OP_ORA
    op_eor, // OP_EOR
    op_cmp, // OP_CMP
    op_cpx, // OP_CPX
    op_cpy, // OP_CPY
    op_asl, // OP_ASL
    op_lsr, // OP_LSR
    op_rol, // OP_ROL
    op_ror, // OP_ROR
    op_inc, // OP_INC
    op_dec, // OP_DEC
    op_inx, // OP_INX
    op_iny, // OP_INY
    op_dex, // OP_DEX
    op_dey, // OP_DEY
    op_tax, // OP_TAX
    op_tay, // OP_TAY
    op_txa, // OP_TXA
    op_tya, // OP_TYA
    op_tsx, // OP_TSX
    op_txs, // OP_TXS
    op_pha, // OP_PHA
    op_php, // OP_PHP
    op_pla, // OP_PLA
    op_plp, // OP_PLP
    op_bcc, // OP_BCC
    op_bcs, // OP_BCS
    op_beq, // OP_BEQ
    op_bne, // OP_BNE
    op_bmi, // OP_BMI
    op_bpl, // OP_BPL
    op_bvc, // OP_BVC
    op_bvs, // OP_BVS
    op_clc, // OP_CLC
    op_sec, // OP_SEC
    op_cli, // OP_CLI
    op_sei, // OP_SEI
    op_cld, // OP_CLD
    op_sed, // OP_SED
    op_clv, // OP_CLV
    op_jmp, // OP_JMP
    op_jsr, // OP_JSR
    op_rts, // OP_RTS
    op_rti, // OP_RTI
    op_brk, // OP_BRK
    op_bit, // OP_BIT
    op_nop, // OP_NOP
    op_jam, // OP_JAM
    // 65C02 enhancements
    op_bra, // OP_BRA
    // Illegal opcodes - combination instructions
    op_lax, // OP_LAX
    op_sax, // OP_SAX
    op_dcp, // OP_DCP
    op_isc, // OP_ISC
    op_slo, // OP_SLO
    op_rla, // OP_RLA
    op_sre, // OP_SRE
    op_rra, // OP_RRA
    // Illegal opcodes - special accumulator operations
    op_anc, // OP_ANC
    op_asr, // OP_ASR
    op_arr, // OP_ARR
    op_sbx, // OP_SBX
    // Illegal opcodes - store with AND operations
    op_sha, // OP_SHA
    op_shs, // OP_SHS
    op_shx, // OP_SHX
    op_shy, // OP_SHY
    op_las, // OP_LAS
    // Illegal opcodes - special operations
    op_xaa  // OP_XAA
};

// Compact macro for opcode_info_t opcode definition - creates properly formatted bitfield entries
// Field order: am_index, illegal_store, _reserved, can_skip_page_cross, rmw, op_index
#define OP(am_index, can_skip_page_cross, op_index, rmw_flag) {(am_index), 0, 0, (can_skip_page_cross), (rmw_flag), (op_index)}
#define OP_ILLEGAL_STORE(am_index, can_skip_page_cross, op_index, rmw_flag) {(am_index), 1, 0, (can_skip_page_cross), (rmw_flag), (op_index)}

// Complete opcode lookup table - 8 entries per line for readability
static const opcode_info_t fam65xx_opcode_table[256] = {
    OP(AM_NON,0,OP_BRK,0), OP(AM_INX,1,OP_ORA,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INX,0,OP_SLO,1), OP(AM_ZER,1,OP_NOP,0), OP(AM_ZER,1,OP_ORA,0), OP(AM_ZER,0,OP_ASL,1), OP(AM_ZER,0,OP_SLO,1),
    OP(AM_NON,0,OP_PHP,0), OP(AM_IMM,1,OP_ORA,0), OP(AM_ACC,0,OP_ASL,0), OP(AM_IMM,1,OP_ANC,0), OP(AM_ABS,1,OP_NOP,0), OP(AM_ABS,1,OP_ORA,0), OP(AM_ABS,0,OP_ASL,1), OP(AM_ABS,0,OP_SLO,1),
    OP(AM_REL,0,OP_BPL,0), OP(AM_INY,1,OP_ORA,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_SLO,1), OP(AM_ZPX,1,OP_NOP,0), OP(AM_ZPX,1,OP_ORA,0), OP(AM_ZPX,0,OP_ASL,1), OP(AM_ZPX,0,OP_SLO,1),
    OP(AM_NON,0,OP_CLC,0), OP(AM_ABY,1,OP_ORA,0), OP(AM_IMM,0,OP_NOP,0), OP(AM_ABY,0,OP_SLO,1), OP(AM_ABX,1,OP_NOP,0), OP(AM_ABX,1,OP_ORA,0), OP(AM_ABX,0,OP_ASL,1), OP(AM_ABX,0,OP_SLO,1),
    OP(AM_NON,0,OP_JSR,0), OP(AM_INX,1,OP_AND,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INX,0,OP_RLA,1), OP(AM_ZER,1,OP_BIT,0), OP(AM_ZER,1,OP_AND,0), OP(AM_ZER,0,OP_ROL,1), OP(AM_ZER,0,OP_RLA,1),
    OP(AM_NON,0,OP_PLP,0), OP(AM_IMM,1,OP_AND,0), OP(AM_ACC,0,OP_ROL,0), OP(AM_IMM,1,OP_ANC,0), OP(AM_ABS,1,OP_BIT,0), OP(AM_ABS,1,OP_AND,0), OP(AM_ABS,0,OP_ROL,1), OP(AM_ABS,0,OP_RLA,1),
    OP(AM_REL,0,OP_BMI,0), OP(AM_INY,1,OP_AND,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_RLA,1), OP(AM_ZPX,1,OP_NOP,0), OP(AM_ZPX,1,OP_AND,0), OP(AM_ZPX,0,OP_ROL,1), OP(AM_ZPX,0,OP_RLA,1),
    OP(AM_NON,0,OP_SEC,0), OP(AM_ABY,1,OP_AND,0), OP(AM_IMM,0,OP_NOP,0), OP(AM_ABY,0,OP_RLA,1), OP(AM_ABX,1,OP_NOP,0), OP(AM_ABX,1,OP_AND,0), OP(AM_ABX,0,OP_ROL,1), OP(AM_ABX,0,OP_RLA,1),
    OP(AM_NON,0,OP_RTI,0), OP(AM_INX,1,OP_EOR,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INX,0,OP_SRE,1), OP(AM_ZER,1,OP_NOP,0), OP(AM_ZER,1,OP_EOR,0), OP(AM_ZER,0,OP_LSR,1), OP(AM_ZER,0,OP_SRE,1),
    OP(AM_NON,0,OP_PHA,0), OP(AM_IMM,1,OP_EOR,0), OP(AM_ACC,0,OP_LSR,0), OP(AM_IMM,1,OP_ASR,0), OP(AM_ABS,1,OP_JMP,0), OP(AM_ABS,1,OP_EOR,0), OP(AM_ABS,0,OP_LSR,1), OP(AM_ABS,0,OP_SRE,1),
    OP(AM_REL,0,OP_BVC,0), OP(AM_INY,1,OP_EOR,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_SRE,1), OP(AM_ZPX,1,OP_NOP,0), OP(AM_ZPX,1,OP_EOR,0), OP(AM_ZPX,0,OP_LSR,1), OP(AM_ZPX,0,OP_SRE,1),
    OP(AM_NON,0,OP_CLI,0), OP(AM_ABY,1,OP_EOR,0), OP(AM_IMM,0,OP_NOP,0), OP(AM_ABY,0,OP_SRE,1), OP(AM_ABX,1,OP_NOP,0), OP(AM_ABX,1,OP_EOR,0), OP(AM_ABX,0,OP_LSR,1), OP(AM_ABX,0,OP_SRE,1),
    OP(AM_NON,0,OP_RTS,0), OP(AM_INX,1,OP_ADC,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INX,0,OP_RRA,1), OP(AM_ZER,1,OP_NOP,0), OP(AM_ZER,1,OP_ADC,0), OP(AM_ZER,0,OP_ROR,1), OP(AM_ZER,0,OP_RRA,1),
    OP(AM_NON,0,OP_PLA,0), OP(AM_IMM,1,OP_ADC,0), OP(AM_ACC,0,OP_ROR,0), OP(AM_IMM,1,OP_ARR,0), OP(AM_IND,1,OP_JMP,0), OP(AM_ABS,1,OP_ADC,0), OP(AM_ABS,0,OP_ROR,1), OP(AM_ABS,0,OP_RRA,1),
    OP(AM_REL,0,OP_BVS,0), OP(AM_INY,1,OP_ADC,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_RRA,1), OP(AM_ZPX,1,OP_NOP,0), OP(AM_ZPX,1,OP_ADC,0), OP(AM_ZPX,0,OP_ROR,1), OP(AM_ZPX,0,OP_RRA,1),
    OP(AM_NON,0,OP_SEI,0), OP(AM_ABY,1,OP_ADC,0), OP(AM_IMM,0,OP_NOP,0), OP(AM_ABY,0,OP_RRA,1), OP(AM_ABX,1,OP_NOP,0), OP(AM_ABX,1,OP_ADC,0), OP(AM_ABX,0,OP_ROR,1), OP(AM_ABX,0,OP_RRA,1),
    OP(AM_IMM,1,OP_NOP,0), OP(AM_INX,0,OP_STA,0), OP(AM_IMM,1,OP_NOP,0), OP(AM_INX,0,OP_SAX,0), OP(AM_ZER,0,OP_STY,0), OP(AM_ZER,0,OP_STA,0), OP(AM_ZER,0,OP_STX,0), OP(AM_ZER,0,OP_SAX,0),
    OP(AM_NON,0,OP_DEY,0), OP(AM_IMM,1,OP_NOP,0), OP(AM_NON,0,OP_TXA,0), OP(AM_IMM,1,OP_XAA,0), OP(AM_ABS,0,OP_STY,0), OP(AM_ABS,0,OP_STA,0), OP(AM_ABS,0,OP_STX,0), OP(AM_ABS,0,OP_SAX,0),
    OP(AM_REL,0,OP_BCC,0), OP(AM_INY,0,OP_STA,0), OP(AM_NON,0,OP_JAM,0), OP_ILLEGAL_STORE(AM_INY,0,OP_SHA,0), OP(AM_ZPX,0,OP_STY,0), OP(AM_ZPX,0,OP_STA,0), OP(AM_ZPY,0,OP_STX,0), OP(AM_ZPY,0,OP_SAX,0),
    OP(AM_NON,0,OP_TYA,0), OP(AM_ABY,0,OP_STA,0), OP(AM_NON,0,OP_TXS,0), OP_ILLEGAL_STORE(AM_ABY,0,OP_SHS,0), OP_ILLEGAL_STORE(AM_ABX,0,OP_SHY,0), OP(AM_ABX,0,OP_STA,0), OP_ILLEGAL_STORE(AM_ABY,0,OP_SHX,0), OP_ILLEGAL_STORE(AM_ABY,0,OP_SHA,0),
    OP(AM_IMM,1,OP_LDY,0), OP(AM_INX,1,OP_LDA,0), OP(AM_IMM,1,OP_LDX,0), OP(AM_INX,1,OP_LAX,0), OP(AM_ZER,1,OP_LDY,0), OP(AM_ZER,1,OP_LDA,0), OP(AM_ZER,1,OP_LDX,0), OP(AM_ZER,1,OP_LAX,0),
    OP(AM_NON,0,OP_TAY,0), OP(AM_IMM,1,OP_LDA,0), OP(AM_NON,0,OP_TAX,0), OP(AM_IMM,0,OP_LAX,0), OP(AM_ABS,1,OP_LDY,0), OP(AM_ABS,1,OP_LDA,0), OP(AM_ABS,1,OP_LDX,0), OP(AM_ABS,1,OP_LAX,0),
    OP(AM_REL,0,OP_BCS,0), OP(AM_INY,1,OP_LDA,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,1,OP_LAX,0), OP(AM_ZPX,1,OP_LDY,0), OP(AM_ZPX,1,OP_LDA,0), OP(AM_ZPY,1,OP_LDX,0), OP(AM_ZPY,1,OP_LAX,0),
    OP(AM_NON,0,OP_CLV,0), OP(AM_ABY,1,OP_LDA,0), OP(AM_NON,0,OP_TSX,0), OP(AM_ABY,1,OP_LAS,0), OP(AM_ABX,1,OP_LDY,0), OP(AM_ABX,1,OP_LDA,0), OP(AM_ABY,1,OP_LDX,0), OP(AM_ABY,1,OP_LAX,0),
    OP(AM_IMM,1,OP_CPY,0), OP(AM_INX,1,OP_CMP,0), OP(AM_IMM,1,OP_NOP,0), OP(AM_INX,0,OP_DCP,1), OP(AM_ZER,1,OP_CPY,0), OP(AM_ZER,1,OP_CMP,0), OP(AM_ZER,0,OP_DEC,1), OP(AM_ZER,0,OP_DCP,1),
    OP(AM_NON,0,OP_INY,0), OP(AM_IMM,1,OP_CMP,0), OP(AM_NON,0,OP_DEX,0), OP(AM_IMM,1,OP_SBX,0), OP(AM_ABS,1,OP_CPY,0), OP(AM_ABS,1,OP_CMP,0), OP(AM_ABS,0,OP_DEC,1), OP(AM_ABS,0,OP_DCP,1),
    OP(AM_REL,0,OP_BNE,0), OP(AM_INY,1,OP_CMP,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_DCP,1), OP(AM_ZPX,1,OP_NOP,0), OP(AM_ZPX,1,OP_CMP,0), OP(AM_ZPX,0,OP_DEC,1), OP(AM_ZPX,0,OP_DCP,1),
    OP(AM_NON,0,OP_CLD,0), OP(AM_ABY,1,OP_CMP,0), OP(AM_IMM,0,OP_NOP,0), OP(AM_ABY,0,OP_DCP,1), OP(AM_ABX,1,OP_NOP,0), OP(AM_ABX,1,OP_CMP,0), OP(AM_ABX,0,OP_DEC,1), OP(AM_ABX,0,OP_DCP,1),
    OP(AM_IMM,1,OP_CPX,0), OP(AM_INX,1,OP_SBC,0), OP(AM_IMM,1,OP_NOP,0), OP(AM_INX,0,OP_ISC,1), OP(AM_ZER,1,OP_CPX,0), OP(AM_ZER,1,OP_SBC,0), OP(AM_ZER,0,OP_INC,1), OP(AM_ZER,0,OP_ISC,1),
    OP(AM_NON,0,OP_INX,0), OP(AM_IMM,1,OP_SBC,0), OP(AM_NON,0,OP_NOP,0), OP(AM_IMM,1,OP_SBC,0), OP(AM_ABS,1,OP_CPX,0), OP(AM_ABS,1,OP_SBC,0), OP(AM_ABS,0,OP_INC,1), OP(AM_ABS,0,OP_ISC,1),
    OP(AM_REL,0,OP_BEQ,0), OP(AM_INY,1,OP_SBC,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_ISC,1), OP(AM_ZPX,1,OP_NOP,0), OP(AM_ZPX,1,OP_SBC,0), OP(AM_ZPX,0,OP_INC,1), OP(AM_ZPX,0,OP_ISC,1),
    OP(AM_NON,0,OP_SED,0), OP(AM_ABY,1,OP_SBC,0), OP(AM_IMM,0,OP_NOP,0), OP(AM_ABY,0,OP_ISC,1), OP(AM_ABX,1,OP_NOP,0), OP(AM_ABX,1,OP_SBC,0), OP(AM_ABX,0,OP_INC,1), OP(AM_ABX,0,OP_ISC,1)
};

// Clean up the compact opcode macros
#undef OP
#undef OP_ILLEGAL_STORE

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif