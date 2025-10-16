#pragma once
#ifndef FAM65XX_TABLES_HPP_INCLUDED
#define FAM65XX_TABLES_HPP_INCLUDED

/*
 * fam65xx_tables.hpp - MOS 65xx Family CPU Lookup Tables and Enums (C++ Version)
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

#include "fam65xx_types.hpp"

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
    // Enhanced addressing modes for all family members
    AM_ZPI,     /* Zero Page Indirect - ($nn) - 65C02 */
    AM_ABI,     /* Absolute Indexed Indirect - ($nnnn,X) - 65C816 */
    AM_SR,      /* Stack Relative - n,S - 65C816 */
    AM_SRI,     /* Stack Relative Indirect Indexed - (n,S),Y - 65C816 */
    AM_ZPR,     /* Zero Page Relative for BBR/BBS - nn,label - Rockwell */
    AM_COUNT
} addressing_mode_t;

/* Aliases for documentation/clarity (all map to AM_NON) */
#define AM_IMP  AM_NON  /* Implied/Implicit - no operand */
#define AM_ACC  AM_NON  /* Accumulator - operate on A register */
#define AM_REL  AM_NON  /* Relative - branch offset */

typedef enum {
    // Core 6502 operations (0-45) - these are used in all processors
    OP_LDA, OP_LDX, OP_LDY,                                    // 0-2: Load operations
    OP_STA, OP_STX, OP_STY,                                    // 3-5: Store operations
    OP_ADC, OP_SBC,                                            // 6-7: Arithmetic
    OP_AND, OP_ORA, OP_EOR,                                    // 8-10: Logic operations
    OP_CMP, OP_CPX, OP_CPY,                                    // 11-13: Compare operations
    OP_ASL, OP_LSR, OP_ROL, OP_ROR,                            // 14-17: Shift/rotate
    OP_INC, OP_DEC,                                            // 18-19: Increment/decrement
    OP_INX, OP_INY, OP_DEX, OP_DEY,                            // 20-23: Register inc/dec
    OP_TAX, OP_TAY, OP_TXA, OP_TYA, OP_TSX, OP_TXS,           // 24-29: Transfer operations
    OP_PHA, OP_PHP, OP_PLA, OP_PLP,                            // 30-33: Stack operations
    OP_BCC, OP_BCS, OP_BEQ, OP_BNE, OP_BMI, OP_BPL, OP_BVC, OP_BVS, // 34-41: Branches
    OP_CLC, OP_SEC, OP_CLI, OP_SEI, OP_CLD, OP_SED, OP_CLV,   // 42-48: Flag operations
    OP_JMP, OP_JSR, OP_RTS, OP_RTI, OP_BRK,                   // 49-53: Control flow
    OP_BIT, OP_NOP, OP_JAM,                                    // 54-56: Test/misc
    
    // Illegal opcodes - most commonly used in 6502/6510 (57-74)
    OP_LAX, OP_SAX, OP_DCP, OP_ISC, OP_SLO, OP_RLA, OP_SRE, OP_RRA, // 57-64: Combo ops
    OP_ANC, OP_ASR, OP_ARR, OP_SBX,                            // 65-68: Special accumulator
    OP_SHA, OP_SHS, OP_SHX, OP_SHY, OP_LAS,                   // 69-73: Store with AND
    OP_XAA,                                                    // 74: Special operation
    
    // 65C02 enhancements (75-84)
    OP_BRA, OP_STZ, OP_TRB, OP_TSB, OP_PHX, OP_PHY, OP_PLX, OP_PLY, OP_WAI, OP_STP, // 75-84

    // Extended operations (85-116) - these will map to OP_NOP in 7-bit tables
    // but can be handled via processor-specific logic
    OP_RMB0, OP_RMB1, OP_RMB2, OP_RMB3, OP_RMB4, OP_RMB5, OP_RMB6, OP_RMB7,
    OP_SMB0, OP_SMB1, OP_SMB2, OP_SMB3, OP_SMB4, OP_SMB5, OP_SMB6, OP_SMB7,
    OP_BBR0, OP_BBR1, OP_BBR2, OP_BBR3, OP_BBR4, OP_BBR5, OP_BBR6, OP_BBR7,
    OP_BBS0, OP_BBS1, OP_BBS2, OP_BBS3, OP_BBS4, OP_BBS5, OP_BBS6, OP_BBS7,
    // 65C816 16-bit operations (117-135)
    OP_REP, OP_SEP, OP_XBA, OP_XCE, OP_COP, OP_WDM,
    OP_PEA, OP_PER, OP_PEI, OP_PHB, OP_PHD, OP_PHK, OP_PLB, OP_PLD,
    OP_RTL, OP_JSL, OP_JML, OP_MVN, OP_MVP,
    
    OP_COUNT
} operation_t;

// Helper to safely convert operation to 7-bit index
#define OP_TO_7BIT(op) ((op) < OP_COUNT_7BIT ? (op) : OP_NOP)

#ifdef AIEMUC_IMPL

// Include all function implementations to eliminate forward declarations
#include "fam65xx_addrmodes.hpp"
#include "fam65xx_ops.hpp"
#include "fam65xx_ops_part2.hpp"
#include "fam65xx_ops_part3.hpp"
#include "fam65xx_ops_rmw.hpp"
#include "fam65xx_ops_illegal.hpp"
#include "fam65xx_ops_65c02.hpp"

/* Operation table - function pointers ordered by enum */
const cycle_fn_t fam65xx_op_handlers[OP_COUNT] = {
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
    op_stz, // OP_STZ
    op_trb, // OP_TRB
    op_tsb, // OP_TSB
    op_phx, // OP_PHX
    op_phy, // OP_PHY
    op_plx, // OP_PLX
    op_ply, // OP_PLY
    op_wai, // OP_WAI
    op_stp, // OP_STP
    // Rockwell 65C02 bit manipulation - TODO: implement all
    op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, // OP_RMB0-7
    op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, // OP_SMB0-7
    op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, // OP_BBR0-7
    op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, // OP_BBS0-7
    // 65C816 16-bit operations - TODO: implement all
    op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, // OP_REP-OP_WDM
    op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, op_nop, // OP_PEA-OP_PLD
    op_nop, op_nop, op_nop, op_nop, op_nop, // OP_RTL-OP_MVP
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

// Single shared addressing mode table (same across all processors)
const cycle_fn_t fam65xx_addr_mode_table[AM_COUNT] = {
    nullptr,  // AM_NON - No handler needed
    nullptr,  // AM_IMM - No handler (handled in operation)
    am_zp,    // AM_ZER
    am_zpx,   // AM_ZPX
    am_zpy,   // AM_ZPY
    am_abs,   // AM_ABS
    am_abx,   // AM_ABX
    am_aby,   // AM_ABY
    am_ind,   // AM_IND
    am_idx,   // AM_INX
    am_idy,   // AM_INY
    am_zpi,   // AM_ZPI - Zero Page Indirect (65C02+)
    am_abi,   // AM_ABI - Absolute Indexed Indirect (65C816)
    am_sr,    // AM_SR - Stack Relative (65C816)
    am_sri,   // AM_SRI - Stack Relative Indirect Indexed (65C816)
    am_zpr    // AM_ZPR - Zero Page Relative for BBR/BBS (Rockwell)
};

// Compact macro for opcode_info_t opcode definition - creates properly formatted bitfield entries
// Field order: am_index, illegal_store, _reserved, can_skip_page_cross, rmw, op_index
#define OP(am_index, can_skip_page_cross, op_index, rmw_flag) {(am_index), 0, 0, (can_skip_page_cross), (rmw_flag), OP_TO_7BIT(op_index)}
#define OP_ILLEGAL_STORE(am_index, can_skip_page_cross, op_index, rmw_flag) {(am_index), 1, 0, (can_skip_page_cross), (rmw_flag), OP_TO_7BIT(op_index)}

#endif /* AIEMUC_IMPL */

#ifdef __cplusplus
} // extern "C"

// C++ includes outside namespace to avoid namespace pollution
#include <array>

// ============================================================================
// COMPLETE PROCESSOR-SPECIFIC OPCODE TABLES
// ============================================================================
// This section provides hardware-accurate opcode tables for each processor variant
// with complete 256-opcode mapping integrated from fam65xx_unified_opcodes.hpp

namespace fam65xx_variants {

// Forward declarations for processor tags (from fam65xx_variants.hpp)
struct MOS6502Tag;
struct MOS6510Tag;
struct WDC65C02Tag;
struct Rockwell65C02Tag;
struct WDC65C816Tag;

#ifdef AIEMUC_IMPL

// ============================================================================
// COMPLETE MOS 6502 OPCODE TABLE (256 entries, hardware-accurate)
// ============================================================================
const opcode_info_t mos6502_opcode_table[256] = {
    // 0x00-0x0F
    OP(AM_NON, 0, OP_BRK, 0), OP(AM_INX, 1, OP_ORA, 0), OP(AM_NON, 0, OP_JAM, 0), OP(AM_INX, 0, OP_SLO, 0),
    OP(AM_ZER, 0, OP_NOP, 0), OP(AM_ZER, 1, OP_ORA, 0), OP(AM_ZER, 0, OP_ASL, 1), OP(AM_ZER, 0, OP_SLO, 0),
    OP(AM_NON, 0, OP_PHP, 0), OP(AM_IMM, 1, OP_ORA, 0), OP(AM_NON, 0, OP_ASL, 1), OP(AM_IMM, 0, OP_ANC, 0),
    OP(AM_ABS, 0, OP_NOP, 0), OP(AM_ABS, 1, OP_ORA, 0), OP(AM_ABS, 0, OP_ASL, 1), OP(AM_ABS, 0, OP_SLO, 0),
    
    // 0x10-0x1F
    OP(AM_NON, 0, OP_BPL, 0), OP(AM_INY, 1, OP_ORA, 0), OP(AM_NON, 0, OP_JAM, 0), OP_ILLEGAL_STORE(AM_INY, 0, OP_SLO, 0),
    OP(AM_ZPX, 0, OP_NOP, 0), OP(AM_ZPX, 1, OP_ORA, 0), OP(AM_ZPX, 0, OP_ASL, 1), OP(AM_ZPX, 0, OP_SLO, 0),
    OP(AM_NON, 0, OP_CLC, 0), OP(AM_ABY, 1, OP_ORA, 0), OP(AM_NON, 0, OP_NOP, 0), OP_ILLEGAL_STORE(AM_ABY, 0, OP_SLO, 0),
    OP(AM_ABX, 0, OP_NOP, 0), OP(AM_ABX, 1, OP_ORA, 0), OP_ILLEGAL_STORE(AM_ABX, 0, OP_ASL, 1), OP_ILLEGAL_STORE(AM_ABX, 0, OP_SLO, 0),
    
    // 0x20-0x2F
    OP(AM_ABS, 0, OP_JSR, 0), OP(AM_INX, 1, OP_AND, 0), OP(AM_NON, 0, OP_JAM, 0), OP(AM_INX, 0, OP_RLA, 0),
    OP(AM_ZER, 1, OP_BIT, 0), OP(AM_ZER, 1, OP_AND, 0), OP(AM_ZER, 0, OP_ROL, 1), OP(AM_ZER, 0, OP_RLA, 0),
    OP(AM_NON, 0, OP_PLP, 0), OP(AM_IMM, 1, OP_AND, 0), OP(AM_NON, 0, OP_ROL, 1), OP(AM_IMM, 0, OP_ANC, 0),
    OP(AM_ABS, 1, OP_BIT, 0), OP(AM_ABS, 1, OP_AND, 0), OP(AM_ABS, 0, OP_ROL, 1), OP(AM_ABS, 0, OP_RLA, 0),
    
    // 0x30-0x3F
    OP(AM_NON, 0, OP_BMI, 0), OP(AM_INY, 1, OP_AND, 0), OP(AM_NON, 0, OP_JAM, 0), OP_ILLEGAL_STORE(AM_INY, 0, OP_RLA, 0),
    OP(AM_ZPX, 0, OP_NOP, 0), OP(AM_ZPX, 1, OP_AND, 0), OP(AM_ZPX, 0, OP_ROL, 1), OP(AM_ZPX, 0, OP_RLA, 0),
    OP(AM_NON, 0, OP_SEC, 0), OP(AM_ABY, 1, OP_AND, 0), OP(AM_NON, 0, OP_NOP, 0), OP_ILLEGAL_STORE(AM_ABY, 0, OP_RLA, 0),
    OP(AM_ABX, 0, OP_NOP, 0), OP(AM_ABX, 1, OP_AND, 0), OP_ILLEGAL_STORE(AM_ABX, 0, OP_ROL, 1), OP_ILLEGAL_STORE(AM_ABX, 0, OP_RLA, 0),
    
    // 0x40-0x4F
    OP(AM_NON, 0, OP_RTI, 0), OP(AM_INX, 1, OP_EOR, 0), OP(AM_NON, 0, OP_JAM, 0), OP(AM_INX, 0, OP_SRE, 0),
    OP(AM_ZER, 0, OP_NOP, 0), OP(AM_ZER, 1, OP_EOR, 0), OP(AM_ZER, 0, OP_LSR, 1), OP(AM_ZER, 0, OP_SRE, 0),
    OP(AM_NON, 0, OP_PHA, 0), OP(AM_IMM, 1, OP_EOR, 0), OP(AM_NON, 0, OP_LSR, 1), OP(AM_IMM, 0, OP_ASR, 0),
    OP(AM_ABS, 0, OP_JMP, 0), OP(AM_ABS, 1, OP_EOR, 0), OP(AM_ABS, 0, OP_LSR, 1), OP(AM_ABS, 0, OP_SRE, 0),
    
    // 0x50-0x5F
    OP(AM_NON, 0, OP_BVC, 0), OP(AM_INY, 1, OP_EOR, 0), OP(AM_NON, 0, OP_JAM, 0), OP_ILLEGAL_STORE(AM_INY, 0, OP_SRE, 0),
    OP(AM_ZPX, 0, OP_NOP, 0), OP(AM_ZPX, 1, OP_EOR, 0), OP(AM_ZPX, 0, OP_LSR, 1), OP(AM_ZPX, 0, OP_SRE, 0),
    OP(AM_NON, 0, OP_CLI, 0), OP(AM_ABY, 1, OP_EOR, 0), OP(AM_NON, 0, OP_NOP, 0), OP_ILLEGAL_STORE(AM_ABY, 0, OP_SRE, 0),
    OP(AM_ABX, 0, OP_NOP, 0), OP(AM_ABX, 1, OP_EOR, 0), OP_ILLEGAL_STORE(AM_ABX, 0, OP_LSR, 1), OP_ILLEGAL_STORE(AM_ABX, 0, OP_SRE, 0),
    
    // 0x60-0x6F
    OP(AM_NON, 0, OP_RTS, 0), OP(AM_INX, 1, OP_ADC, 0), OP(AM_NON, 0, OP_JAM, 0), OP(AM_INX, 0, OP_RRA, 0),
    OP(AM_ZER, 0, OP_NOP, 0), OP(AM_ZER, 1, OP_ADC, 0), OP(AM_ZER, 0, OP_ROR, 1), OP(AM_ZER, 0, OP_RRA, 0),
    OP(AM_NON, 0, OP_PLA, 0), OP(AM_IMM, 1, OP_ADC, 0), OP(AM_NON, 0, OP_ROR, 1), OP(AM_IMM, 0, OP_ARR, 0),
    OP(AM_IND, 0, OP_JMP, 0), OP(AM_ABS, 1, OP_ADC, 0), OP(AM_ABS, 0, OP_ROR, 1), OP(AM_ABS, 0, OP_RRA, 0),
    
    // 0x70-0x7F
    OP(AM_NON, 0, OP_BVS, 0), OP(AM_INY, 1, OP_ADC, 0), OP(AM_NON, 0, OP_JAM, 0), OP_ILLEGAL_STORE(AM_INY, 0, OP_RRA, 0),
    OP(AM_ZPX, 0, OP_NOP, 0), OP(AM_ZPX, 1, OP_ADC, 0), OP(AM_ZPX, 0, OP_ROR, 1), OP(AM_ZPX, 0, OP_RRA, 0),
    OP(AM_NON, 0, OP_SEI, 0), OP(AM_ABY, 1, OP_ADC, 0), OP(AM_NON, 0, OP_NOP, 0), OP_ILLEGAL_STORE(AM_ABY, 0, OP_RRA, 0),
    OP(AM_ABX, 0, OP_NOP, 0), OP(AM_ABX, 1, OP_ADC, 0), OP_ILLEGAL_STORE(AM_ABX, 0, OP_ROR, 1), OP_ILLEGAL_STORE(AM_ABX, 0, OP_RRA, 0),
    
    // 0x80-0x8F
    OP(AM_IMM, 0, OP_NOP, 0), OP(AM_INX, 0, OP_STA, 0), OP(AM_IMM, 0, OP_NOP, 0), OP(AM_INX, 0, OP_SAX, 0),
    OP(AM_ZER, 0, OP_STY, 0), OP(AM_ZER, 0, OP_STA, 0), OP(AM_ZER, 0, OP_STX, 0), OP(AM_ZER, 0, OP_SAX, 0),
    OP(AM_NON, 0, OP_DEY, 0), OP(AM_IMM, 0, OP_NOP, 0), OP(AM_NON, 0, OP_TXA, 0), OP(AM_IMM, 0, OP_XAA, 0),
    OP(AM_ABS, 0, OP_STY, 0), OP(AM_ABS, 0, OP_STA, 0), OP(AM_ABS, 0, OP_STX, 0), OP(AM_ABS, 0, OP_SAX, 0),
    
    // 0x90-0x9F
    OP(AM_NON, 0, OP_BCC, 0), OP_ILLEGAL_STORE(AM_INY, 0, OP_STA, 0), OP(AM_NON, 0, OP_JAM, 0), OP_ILLEGAL_STORE(AM_INY, 0, OP_SHA, 0),
    OP(AM_ZPX, 0, OP_STY, 0), OP(AM_ZPX, 0, OP_STA, 0), OP(AM_ZPY, 0, OP_STX, 0), OP(AM_ZPY, 0, OP_SAX, 0),
    OP(AM_NON, 0, OP_TYA, 0), OP_ILLEGAL_STORE(AM_ABY, 0, OP_STA, 0), OP(AM_NON, 0, OP_TXS, 0), OP_ILLEGAL_STORE(AM_ABY, 0, OP_SHS, 0),
    OP_ILLEGAL_STORE(AM_ABX, 0, OP_SHY, 0), OP(AM_ABX, 0, OP_STA, 0), OP_ILLEGAL_STORE(AM_ABY, 0, OP_SHX, 0), OP_ILLEGAL_STORE(AM_ABY, 0, OP_SHA, 0),
    
    // 0xA0-0xAF
    OP(AM_IMM, 1, OP_LDY, 0), OP(AM_INX, 1, OP_LDA, 0), OP(AM_IMM, 1, OP_LDX, 0), OP(AM_INX, 1, OP_LAX, 0),
    OP(AM_ZER, 1, OP_LDY, 0), OP(AM_ZER, 1, OP_LDA, 0), OP(AM_ZER, 1, OP_LDX, 0), OP(AM_ZER, 1, OP_LAX, 0),
    OP(AM_NON, 0, OP_TAY, 0), OP(AM_IMM, 1, OP_LDA, 0), OP(AM_NON, 0, OP_TAX, 0), OP(AM_IMM, 1, OP_LAX, 0),
    OP(AM_ABS, 1, OP_LDY, 0), OP(AM_ABS, 1, OP_LDA, 0), OP(AM_ABS, 1, OP_LDX, 0), OP(AM_ABS, 1, OP_LAX, 0),
    
    // 0xB0-0xBF
    OP(AM_NON, 0, OP_BCS, 0), OP(AM_INY, 1, OP_LDA, 0), OP(AM_NON, 0, OP_JAM, 0), OP(AM_INY, 1, OP_LAX, 0),
    OP(AM_ZPX, 1, OP_LDY, 0), OP(AM_ZPX, 1, OP_LDA, 0), OP(AM_ZPY, 1, OP_LDX, 0), OP(AM_ZPY, 1, OP_LAX, 0),
    OP(AM_NON, 0, OP_CLV, 0), OP(AM_ABY, 1, OP_LDA, 0), OP(AM_NON, 0, OP_TSX, 0), OP(AM_ABY, 1, OP_LAS, 0),
    OP(AM_ABX, 1, OP_LDY, 0), OP(AM_ABX, 1, OP_LDA, 0), OP(AM_ABY, 1, OP_LDX, 0), OP(AM_ABY, 1, OP_LAX, 0),
    
    // 0xC0-0xCF
    OP(AM_IMM, 1, OP_CPY, 0), OP(AM_INX, 1, OP_CMP, 0), OP(AM_IMM, 0, OP_NOP, 0), OP(AM_INX, 0, OP_DCP, 0),
    OP(AM_ZER, 1, OP_CPY, 0), OP(AM_ZER, 1, OP_CMP, 0), OP(AM_ZER, 0, OP_DEC, 1), OP(AM_ZER, 0, OP_DCP, 0),
    OP(AM_NON, 0, OP_INY, 0), OP(AM_IMM, 1, OP_CMP, 0), OP(AM_NON, 0, OP_DEX, 0), OP(AM_IMM, 0, OP_SBX, 0),
    OP(AM_ABS, 1, OP_CPY, 0), OP(AM_ABS, 1, OP_CMP, 0), OP(AM_ABS, 0, OP_DEC, 1), OP(AM_ABS, 0, OP_DCP, 0),
    
    // 0xD0-0xDF
    OP(AM_NON, 0, OP_BNE, 0), OP(AM_INY, 1, OP_CMP, 0), OP(AM_NON, 0, OP_JAM, 0), OP_ILLEGAL_STORE(AM_INY, 0, OP_DCP, 0),
    OP(AM_ZPX, 0, OP_NOP, 0), OP(AM_ZPX, 1, OP_CMP, 0), OP(AM_ZPX, 0, OP_DEC, 1), OP(AM_ZPX, 0, OP_DCP, 0),
    OP(AM_NON, 0, OP_CLD, 0), OP(AM_ABY, 1, OP_CMP, 0), OP(AM_NON, 0, OP_NOP, 0), OP_ILLEGAL_STORE(AM_ABY, 0, OP_DCP, 0),
    OP(AM_ABX, 0, OP_NOP, 0), OP(AM_ABX, 1, OP_CMP, 0), OP_ILLEGAL_STORE(AM_ABX, 0, OP_DEC, 1), OP_ILLEGAL_STORE(AM_ABX, 0, OP_DCP, 0),
    
    // 0xE0-0xEF
    OP(AM_IMM, 1, OP_CPX, 0), OP(AM_INX, 1, OP_SBC, 0), OP(AM_IMM, 0, OP_NOP, 0), OP(AM_INX, 0, OP_ISC, 0),
    OP(AM_ZER, 1, OP_CPX, 0), OP(AM_ZER, 1, OP_SBC, 0), OP(AM_ZER, 0, OP_INC, 1), OP(AM_ZER, 0, OP_ISC, 0),
    OP(AM_NON, 0, OP_INX, 0), OP(AM_IMM, 1, OP_SBC, 0), OP(AM_NON, 0, OP_NOP, 0), OP(AM_IMM, 1, OP_SBC, 0),
    OP(AM_ABS, 1, OP_CPX, 0), OP(AM_ABS, 1, OP_SBC, 0), OP(AM_ABS, 0, OP_INC, 1), OP(AM_ABS, 0, OP_ISC, 0),
    
    // 0xF0-0xFF
    OP(AM_NON, 0, OP_BEQ, 0), OP(AM_INY, 1, OP_SBC, 0), OP(AM_NON, 0, OP_JAM, 0), OP_ILLEGAL_STORE(AM_INY, 0, OP_ISC, 0),
    OP(AM_ZPX, 0, OP_NOP, 0), OP(AM_ZPX, 1, OP_SBC, 0), OP(AM_ZPX, 0, OP_INC, 1), OP(AM_ZPX, 0, OP_ISC, 0),
    OP(AM_NON, 0, OP_SED, 0), OP(AM_ABY, 1, OP_SBC, 0), OP(AM_NON, 0, OP_NOP, 0), OP_ILLEGAL_STORE(AM_ABY, 0, OP_ISC, 0),
    OP(AM_ABX, 0, OP_NOP, 0), OP(AM_ABX, 1, OP_SBC, 0), OP_ILLEGAL_STORE(AM_ABX, 0, OP_INC, 1), OP_ILLEGAL_STORE(AM_ABX, 0, OP_ISC, 0)
};

#endif // AIEMUC_IMPL

} // namespace fam65xx_variants

// ============================================================================
// PROCESSOR-AGNOSTIC ACCESS FUNCTIONS (Global scope)
// ============================================================================

#ifdef AIEMUC_IMPL

// Default processor-agnostic accessor functions - clean static const implementation
opcode_info_t fam65xx_get_opcode_entry(uint8_t opcode) {
    // Use MOS 6502 table directly - no macro needed!
    return fam65xx_variants::mos6502_opcode_table[opcode];
}

cycle_fn_t fam65xx_get_addr_mode_handler(int am_index) {
    // Use the single shared addressing mode table
    if (am_index >= 0 && am_index < AM_COUNT) {
        return fam65xx_addr_mode_table[am_index];
    }
    return nullptr;
}

#endif

#endif /* __cplusplus */

#endif /* FAM65XX_TABLES_HPP_INCLUDED */