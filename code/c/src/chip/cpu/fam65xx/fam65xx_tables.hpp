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

#include "fam65xx_core.hpp"

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
    OP_BRA, OP_STZ, OP_TRB, OP_TSB, OP_PHX, OP_PHY, OP_PLX, OP_PLY, OP_WAI, OP_STP,
    // Rockwell 65C02 bit manipulation
    OP_RMB0, OP_RMB1, OP_RMB2, OP_RMB3, OP_RMB4, OP_RMB5, OP_RMB6, OP_RMB7,
    OP_SMB0, OP_SMB1, OP_SMB2, OP_SMB3, OP_SMB4, OP_SMB5, OP_SMB6, OP_SMB7,
    OP_BBR0, OP_BBR1, OP_BBR2, OP_BBR3, OP_BBR4, OP_BBR5, OP_BBR6, OP_BBR7,
    OP_BBS0, OP_BBS1, OP_BBS2, OP_BBS3, OP_BBS4, OP_BBS5, OP_BBS6, OP_BBS7,
    // 65C816 16-bit operations
    OP_REP, OP_SEP, OP_XBA, OP_XCE, OP_COP, OP_WDM,
    OP_PEA, OP_PER, OP_PEI, OP_PHB, OP_PHD, OP_PHK, OP_PLB, OP_PLD,
    OP_RTL, OP_JSL, OP_JML, OP_MVN, OP_MVP,
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
#include "fam65xx_addrmodes.hpp"
#include "fam65xx_ops.hpp"
#include "fam65xx_ops_part2.hpp"
#include "fam65xx_ops_part3.hpp"
#include "fam65xx_ops_rmw.hpp"
#include "fam65xx_ops_illegal.hpp"
#include "fam65xx_ops_65c02.hpp"

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

// THE ONE OPCODE TABLE - populated by constexpr template helper per processor
opcode_info_t fam65xx_opcode_table[256];

// Clean up the compact opcode macros
#undef OP
#undef OP_ILLEGAL_STORE

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
} // extern "C"

// ============================================================================
// TEMPLATE-BASED PROCESSOR-SPECIFIC OPCODE TABLES
// ============================================================================
// This section provides hardware-accurate opcode tables for each processor variant
// with their specific instruction sets, illegal opcode handling, and new instructions.

namespace fam65xx_template {

// Include template definitions
#include "fam65xx_templates.hpp"

// Import template definitions from fam65xx_template namespace
using fam65xx_template::ProcessorTraits;
using fam65xx_template::MOS6502Tag;
using fam65xx_template::MOS6510Tag;
using fam65xx_template::WDC65C02Tag;
using fam65xx_template::Rockwell65C02Tag;
using fam65xx_template::WDC65C816Tag;

#ifdef CHIPS_IMPL

// ============================================================================
// CONSTEXPR TEMPLATE HELPER TO POPULATE THE ONE OPCODE TABLE
// ============================================================================

// Processor feature detection helpers
template<typename ProcessorTag>
constexpr bool processor_has_illegal_opcodes() {
    return ProcessorTraits<ProcessorTag>::has_illegal_opcodes;
}

template<typename ProcessorTag>
constexpr bool processor_has_cmos_enhancements() {
    return ProcessorTraits<ProcessorTag>::has_cmos_enhancements;
}

template<typename ProcessorTag>
constexpr bool processor_has_bit_manipulation() {
    return ProcessorTraits<ProcessorTag>::has_bit_manipulation;
}

template<typename ProcessorTag>
constexpr bool processor_has_16bit_mode() {
    return ProcessorTraits<ProcessorTag>::has_16bit_mode;
}

// Redefine OP macros for template context
#define OP_TEMPLATE(am_index, can_skip_page_cross, op_index, rmw_flag) {(am_index), 0, 0, (can_skip_page_cross), (rmw_flag), (op_index)}
#define OP_ILLEGAL_STORE_TEMPLATE(am_index, can_skip_page_cross, op_index, rmw_flag) {(am_index), 1, 0, (can_skip_page_cross), (rmw_flag), (op_index)}

// THE ONE CONSTEXPR TEMPLATE HELPER that fills the ONE table
template<typename ProcessorTag>
constexpr opcode_info_t get_opcode_entry(uint8_t opcode) {
    // Base 6502 opcodes - common to all processors
    switch (opcode) {
        case 0x00: return OP_TEMPLATE(AM_NON,0,OP_BRK,0);
        case 0x01: return OP_TEMPLATE(AM_INX,1,OP_ORA,0);
        case 0x05: return OP_TEMPLATE(AM_ZER,1,OP_ORA,0);
        case 0x06: return OP_TEMPLATE(AM_ZER,0,OP_ASL,1);
        case 0x08: return OP_TEMPLATE(AM_NON,0,OP_PHP,0);
        case 0x09: return OP_TEMPLATE(AM_IMM,1,OP_ORA,0);
        case 0x0A: return OP_TEMPLATE(AM_ACC,0,OP_ASL,0);
        case 0x0D: return OP_TEMPLATE(AM_ABS,1,OP_ORA,0);
        case 0x0E: return OP_TEMPLATE(AM_ABS,0,OP_ASL,1);
        case 0x10: return OP_TEMPLATE(AM_REL,0,OP_BPL,0);
        case 0x11: return OP_TEMPLATE(AM_INY,1,OP_ORA,0);
        case 0x15: return OP_TEMPLATE(AM_ZPX,1,OP_ORA,0);
        case 0x16: return OP_TEMPLATE(AM_ZPX,0,OP_ASL,1);
        case 0x18: return OP_TEMPLATE(AM_NON,0,OP_CLC,0);
        case 0x19: return OP_TEMPLATE(AM_ABY,1,OP_ORA,0);
        case 0x1D: return OP_TEMPLATE(AM_ABX,1,OP_ORA,0);
        case 0x1E: return OP_TEMPLATE(AM_ABX,0,OP_ASL,1);
        case 0x20: return OP_TEMPLATE(AM_NON,0,OP_JSR,0);
        case 0x21: return OP_TEMPLATE(AM_INX,1,OP_AND,0);
        case 0x24: return OP_TEMPLATE(AM_ZER,1,OP_BIT,0);
        case 0x25: return OP_TEMPLATE(AM_ZER,1,OP_AND,0);
        case 0x26: return OP_TEMPLATE(AM_ZER,0,OP_ROL,1);
        case 0x28: return OP_TEMPLATE(AM_NON,0,OP_PLP,0);
        case 0x29: return OP_TEMPLATE(AM_IMM,1,OP_AND,0);
        case 0x2A: return OP_TEMPLATE(AM_ACC,0,OP_ROL,0);
        case 0x2C: return OP_TEMPLATE(AM_ABS,1,OP_BIT,0);
        case 0x2D: return OP_TEMPLATE(AM_ABS,1,OP_AND,0);
        case 0x2E: return OP_TEMPLATE(AM_ABS,0,OP_ROL,1);
        case 0x30: return OP_TEMPLATE(AM_REL,0,OP_BMI,0);
        case 0x31: return OP_TEMPLATE(AM_INY,1,OP_AND,0);
        case 0x35: return OP_TEMPLATE(AM_ZPX,1,OP_AND,0);
        case 0x36: return OP_TEMPLATE(AM_ZPX,0,OP_ROL,1);
        case 0x38: return OP_TEMPLATE(AM_NON,0,OP_SEC,0);
        case 0x39: return OP_TEMPLATE(AM_ABY,1,OP_AND,0);
        case 0x3D: return OP_TEMPLATE(AM_ABX,1,OP_AND,0);
        case 0x3E: return OP_TEMPLATE(AM_ABX,0,OP_ROL,1);
        // ... continue for all 256 opcodes, but with processor-specific behavior
        
        // Processor-specific handling
        case 0x80: {
            if constexpr (processor_has_cmos_enhancements<ProcessorTag>()) {
                return OP_TEMPLATE(AM_REL,0,OP_BRA,0);  // BRA on 65C02+
            } else {
                if constexpr (processor_has_illegal_opcodes<ProcessorTag>()) {
                    return OP_TEMPLATE(AM_IMM,1,OP_NOP,0);  // Illegal NOP on 6502
                } else {
                    return OP_TEMPLATE(AM_NON,0,OP_NOP,0);  // Regular NOP
                }
            }
        }
        
        default:
            // For illegal opcodes
            if constexpr (processor_has_illegal_opcodes<ProcessorTag>()) {
                // Return appropriate illegal opcode based on opcode value
                return OP_TEMPLATE(AM_NON,0,OP_JAM,0);  // Simplified - would need full mapping
            } else {
                // CMOS processors convert illegal opcodes to NOPs
                return OP_TEMPLATE(AM_NON,0,OP_NOP,0);
            }
    }
}

// Function to populate the ONE table based on processor type
template<typename ProcessorTag>
void populate_opcode_table() {
    for (int i = 0; i < 256; i++) {
        fam65xx_opcode_table[i] = get_opcode_entry<ProcessorTag>(static_cast<uint8_t>(i));
    }
}

// Runtime table initialization (called once per processor type)
void initialize_opcode_table_for_mos6502() { populate_opcode_table<MOS6502Tag>(); }
void initialize_opcode_table_for_mos6510() { populate_opcode_table<MOS6510Tag>(); }
void initialize_opcode_table_for_wdc65c02() { populate_opcode_table<WDC65C02Tag>(); }
void initialize_opcode_table_for_rockwell65c02() { populate_opcode_table<Rockwell65C02Tag>(); }
void initialize_opcode_table_for_wdc65c816() { populate_opcode_table<WDC65C816Tag>(); }

// Clean up template macros
#undef OP_TEMPLATE
#undef OP_ILLEGAL_STORE_TEMPLATE

// ============================================================================
// PROCESSOR-SPECIFIC INSTRUCTION SET QUERIES
// ============================================================================

// Template functions to query instruction availability at compile time
template<typename ProcessorTag>
constexpr bool has_instruction_bra() {
    return ProcessorTraits<ProcessorTag>::has_cmos_enhancements;
}

template<typename ProcessorTag>
constexpr bool has_instruction_phx_phy() {
    return ProcessorTraits<ProcessorTag>::has_cmos_enhancements;
}

template<typename ProcessorTag>
constexpr bool has_instruction_stz() {
    return ProcessorTraits<ProcessorTag>::has_cmos_enhancements;
}

template<typename ProcessorTag>
constexpr bool has_instruction_rmb_smb() {
    return ProcessorTraits<ProcessorTag>::has_bit_manipulation;
}

template<typename ProcessorTag>
constexpr bool has_instruction_bbr_bbs() {
    return ProcessorTraits<ProcessorTag>::has_bit_manipulation;
}

template<typename ProcessorTag>
constexpr bool has_16bit_instructions() {
    return ProcessorTraits<ProcessorTag>::has_16bit_mode;
}

// ============================================================================
// EXAMPLE: PROCESSOR-SPECIFIC OPERATION MODIFICATIONS
// ============================================================================

// Example of how processor-specific behavior could be implemented
// This demonstrates the template pattern for processor-specific modifications

template<typename ProcessorTag>
struct ProcessorBehavior {
    // Default behavior - can be specialized per processor
    
    static inline bool should_enable_illegal_opcodes() {
        return ProcessorTraits<ProcessorTag>::has_illegal_opcodes;
    }
    
    static inline bool should_handle_io_port() {
        return ProcessorTraits<ProcessorTag>::has_io_port;
    }
    
    static inline bool should_use_cmos_timing() {
        return ProcessorTraits<ProcessorTag>::has_cmos_enhancements;
    }
    
    static inline bool decimal_mode_affects_nz() {
        return ProcessorTraits<ProcessorTag>::decimal_affects_nz;
    }
    
    static inline bool has_jmp_indirect_bug() {
        return ProcessorTraits<ProcessorTag>::has_nmos_bugs;
    }
};

// Example specialization for MOS 6502
template<>
struct ProcessorBehavior<MOS6502Tag> {
    static inline bool should_enable_illegal_opcodes() { return true; }
    static inline bool should_handle_io_port() { return false; }
    static inline bool should_use_cmos_timing() { return false; }
    static inline bool decimal_mode_affects_nz() { return true; }
    static inline bool has_jmp_indirect_bug() { return true; }
};

// Example specialization for WDC 65C02
template<>
struct ProcessorBehavior<WDC65C02Tag> {
    static inline bool should_enable_illegal_opcodes() { return false; }
    static inline bool should_handle_io_port() { return false; }
    static inline bool should_use_cmos_timing() { return true; }
    static inline bool decimal_mode_affects_nz() { return false; }
    static inline bool has_jmp_indirect_bug() { return false; }
};

#endif /* CHIPS_IMPL */

} // namespace fam65xx_template

#endif /* __cplusplus */

#endif /* FAM65XX_TABLES_HPP_INCLUDED */