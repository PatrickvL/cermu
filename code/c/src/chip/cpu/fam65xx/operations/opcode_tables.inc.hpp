/*
 * opcode_tables.inc.hpp - CPUTraits-Based Opcode Table Generation
 *
 * This file contains the CPUTraits-based opcode table generation function.
 * Unlike other .inc.hpp files that contain member functions, this file contains
 * a free function that generates opcode tables based on CPUTraits configuration.
 */

// Always include required headers for both standalone analysis and production
#include <array>
#include "../fam65xx_types.h"
#include "../fam65xx_processor_traits.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// CPUTRAITS-BASED OPCODE TABLE GENERATION
// ============================================================================

// CPUTraits-based opcode table generation function
constexpr std::array<opcode_info_t, 256> generate_opcode_table_for_traits(const CPUTraits& traits) {
    // Start with base NES 6502/RICOH 2A03 implementation with explicit illegal opcodes
    std::array<opcode_info_t, 256> table{};
    
    // All 256 opcodes explicitly defined - valid opcodes and proper illegal opcodes for hardware accuracy
    table[0x00] = {OP_BRK, AM_NON, OF_NONE};
    table[0x01] = {OP_ORA, AM_INX, OF_NONE};
    table[0x02] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x03] = {OP_SLO, AM_INX, OF_RMW};   // SLO - Shift Left then OR (illegal)
    table[0x04] = {OP_NOP, AM_ZER, OF_NONE};  // NOP zp - illegal NOP
    table[0x05] = {OP_ORA, AM_ZER, OF_NONE};
    table[0x06] = {OP_ASL, AM_ZER, OF_RMW};
    table[0x07] = {OP_SLO, AM_ZER, OF_RMW};   // SLO - Shift Left then OR (illegal)
    table[0x08] = {OP_PHP, AM_NON, OF_NONE};
    table[0x09] = {OP_ORA, AM_IMM, OF_NONE};
    table[0x0A] = {OP_ASL, AM_ACC, OF_NONE};
    table[0x0B] = {OP_ANC, AM_IMM, OF_NONE};  // ANC - AND with carry (illegal)
    table[0x0C] = {OP_NOP, AM_ABS, OF_NONE};  // NOP abs - illegal NOP
    table[0x0D] = {OP_ORA, AM_ABS, OF_NONE};
    table[0x0E] = {OP_ASL, AM_ABS, OF_RMW};
    table[0x0F] = {OP_SLO, AM_ABS, OF_RMW};   // SLO - Shift Left then OR (illegal)
    table[0x10] = {OP_BPL, AM_REL, OF_NONE};
    table[0x11] = {OP_ORA, AM_INY, OF_SKIP_PAGE};
    table[0x12] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x13] = {OP_SLO, AM_INY, OF_RMW};   // SLO - Shift Left then OR (illegal)
    table[0x14] = {OP_NOP, AM_ZPX, OF_NONE};  // NOP zp,X - illegal NOP
    table[0x15] = {OP_ORA, AM_ZPX, OF_NONE};
    table[0x16] = {OP_ASL, AM_ZPX, OF_RMW};
    table[0x17] = {OP_SLO, AM_ZPX, OF_RMW};   // SLO - Shift Left then OR (illegal)
    table[0x18] = {OP_CLC, AM_NON, OF_NONE};
    table[0x19] = {OP_ORA, AM_ABY, OF_SKIP_PAGE};
    table[0x1A] = {OP_NOP, AM_NON, OF_NONE};  // NOP - illegal NOP
    table[0x1B] = {OP_SLO, AM_ABY, OF_RMW};   // SLO - Shift Left then OR (illegal)
    table[0x1C] = {OP_NOP, AM_ABX, OF_SKIP_PAGE}; // NOP abs,X - illegal NOP
    table[0x1D] = {OP_ORA, AM_ABX, OF_SKIP_PAGE};
    table[0x1E] = {OP_ASL, AM_ABX, OF_RMW};
    table[0x1F] = {OP_SLO, AM_ABX, OF_RMW};   // SLO - Shift Left then OR (illegal)
    table[0x20] = {OP_JSR, AM_NON, OF_NONE};
    table[0x21] = {OP_AND, AM_INX, OF_NONE};
    table[0x22] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x23] = {OP_RLA, AM_INX, OF_RMW};   // RLA - Rotate Left then AND (illegal)
    table[0x24] = {OP_BIT, AM_ZER, OF_NONE};
    table[0x25] = {OP_AND, AM_ZER, OF_NONE};
    table[0x26] = {OP_ROL, AM_ZER, OF_RMW};
    table[0x27] = {OP_RLA, AM_ZER, OF_RMW};   // RLA - Rotate Left then AND (illegal)
    table[0x28] = {OP_PLP, AM_NON, OF_NONE};
    table[0x29] = {OP_AND, AM_IMM, OF_NONE};
    table[0x2A] = {OP_ROL, AM_ACC, OF_NONE};
    table[0x2B] = {OP_ANC, AM_IMM, OF_NONE};  // ANC - AND with carry (illegal)
    table[0x2C] = {OP_BIT, AM_ABS, OF_NONE};
    table[0x2D] = {OP_AND, AM_ABS, OF_NONE};
    table[0x2E] = {OP_ROL, AM_ABS, OF_RMW};
    table[0x2F] = {OP_RLA, AM_ABS, OF_RMW};   // RLA - Rotate Left then AND (illegal)
    table[0x30] = {OP_BMI, AM_REL, OF_NONE};
    table[0x31] = {OP_AND, AM_INY, OF_SKIP_PAGE};
    table[0x32] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x33] = {OP_RLA, AM_INY, OF_RMW};   // RLA - Rotate Left then AND (illegal)
    table[0x34] = {OP_NOP, AM_ZPX, OF_NONE};  // NOP zp,X - illegal NOP
    table[0x35] = {OP_AND, AM_ZPX, OF_NONE};
    table[0x36] = {OP_ROL, AM_ZPX, OF_RMW};
    table[0x37] = {OP_RLA, AM_ZPX, OF_RMW};   // RLA - Rotate Left then AND (illegal)
    table[0x38] = {OP_SEC, AM_NON, OF_NONE};
    table[0x39] = {OP_AND, AM_ABY, OF_SKIP_PAGE};
    table[0x3A] = {OP_NOP, AM_NON, OF_NONE};  // NOP - illegal NOP
    table[0x3B] = {OP_RLA, AM_ABY, OF_RMW};   // RLA - Rotate Left then AND (illegal)
    table[0x3C] = {OP_NOP, AM_ABX, OF_SKIP_PAGE}; // NOP abs,X - illegal NOP
    table[0x3D] = {OP_AND, AM_ABX, OF_SKIP_PAGE};
    table[0x3E] = {OP_ROL, AM_ABX, OF_RMW};
    table[0x3F] = {OP_RLA, AM_ABX, OF_RMW};   // RLA - Rotate Left then AND (illegal)
    table[0x40] = {OP_RTI, AM_NON, OF_NONE};
    table[0x41] = {OP_EOR, AM_INX, OF_NONE};
    table[0x42] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x43] = {OP_SRE, AM_INX, OF_RMW};   // SRE - Shift Right then EOR (illegal)
    table[0x44] = {OP_NOP, AM_ZER, OF_NONE};  // NOP zp - illegal NOP
    table[0x45] = {OP_EOR, AM_ZER, OF_NONE};
    table[0x46] = {OP_LSR, AM_ZER, OF_RMW};
    table[0x47] = {OP_SRE, AM_ZER, OF_RMW};   // SRE - Shift Right then EOR (illegal)
    table[0x48] = {OP_PHA, AM_NON, OF_NONE};
    table[0x49] = {OP_EOR, AM_IMM, OF_NONE};
    table[0x4A] = {OP_LSR, AM_ACC, OF_NONE};
    table[0x4B] = {OP_ASR, AM_IMM, OF_NONE};  // ASR - AND then LSR (illegal) (also called ALR)
    table[0x4C] = {OP_JMP, AM_ABS, OF_NONE};
    table[0x4D] = {OP_EOR, AM_ABS, OF_NONE};
    table[0x4E] = {OP_LSR, AM_ABS, OF_RMW};
    table[0x4F] = {OP_SRE, AM_ABS, OF_RMW};   // SRE - Shift Right then EOR (illegal)
    table[0x50] = {OP_BVC, AM_REL, OF_NONE};
    table[0x51] = {OP_EOR, AM_INY, OF_SKIP_PAGE};
    table[0x52] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x53] = {OP_SRE, AM_INY, OF_RMW};   // SRE - Shift Right then EOR (illegal)
    table[0x54] = {OP_NOP, AM_ZPX, OF_NONE};  // NOP zp,X - illegal NOP
    table[0x55] = {OP_EOR, AM_ZPX, OF_NONE};
    table[0x56] = {OP_LSR, AM_ZPX, OF_RMW};
    table[0x57] = {OP_SRE, AM_ZPX, OF_RMW};   // SRE - Shift Right then EOR (illegal)
    table[0x58] = {OP_CLI, AM_NON, OF_NONE};
    table[0x59] = {OP_EOR, AM_ABY, OF_SKIP_PAGE};
    table[0x5A] = {OP_NOP, AM_NON, OF_NONE};  // NOP - illegal NOP
    table[0x5B] = {OP_SRE, AM_ABY, OF_RMW};   // SRE - Shift Right then EOR (illegal)
    table[0x5C] = {OP_NOP, AM_ABX, OF_SKIP_PAGE}; // NOP abs,X - illegal NOP
    table[0x5D] = {OP_EOR, AM_ABX, OF_SKIP_PAGE};
    table[0x5E] = {OP_LSR, AM_ABX, OF_RMW};
    table[0x5F] = {OP_SRE, AM_ABX, OF_RMW};   // SRE - Shift Right then EOR (illegal)
    table[0x60] = {OP_RTS, AM_NON, OF_NONE};
    table[0x61] = {OP_ADC, AM_INX, OF_NONE};
    table[0x62] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x63] = {OP_RRA, AM_INX, OF_RMW};   // RRA - Rotate Right then ADC (illegal)
    table[0x64] = {OP_NOP, AM_ZER, OF_NONE};  // NOP zp - illegal NOP
    table[0x65] = {OP_ADC, AM_ZER, OF_NONE};
    table[0x66] = {OP_ROR, AM_ZER, OF_RMW};
    table[0x67] = {OP_RRA, AM_ZER, OF_RMW};   // RRA - Rotate Right then ADC (illegal)
    table[0x68] = {OP_PLA, AM_NON, OF_NONE};
    table[0x69] = {OP_ADC, AM_IMM, OF_NONE};
    table[0x6A] = {OP_ROR, AM_ACC, OF_NONE};
    table[0x6B] = {OP_ARR, AM_IMM, OF_NONE};  // ARR - AND then ROR (illegal)
    table[0x6C] = {OP_JMP, AM_IND, OF_NONE};
    table[0x6D] = {OP_ADC, AM_ABS, OF_NONE};
    table[0x6E] = {OP_ROR, AM_ABS, OF_RMW};
    table[0x6F] = {OP_RRA, AM_ABS, OF_RMW};   // RRA - Rotate Right then ADC (illegal)
    table[0x70] = {OP_BVS, AM_REL, OF_NONE};
    table[0x71] = {OP_ADC, AM_INY, OF_SKIP_PAGE};
    table[0x72] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x73] = {OP_RRA, AM_INY, OF_RMW};   // RRA - Rotate Right then ADC (illegal)
    table[0x74] = {OP_NOP, AM_ZPX, OF_NONE};  // NOP zp,X - illegal NOP
    table[0x75] = {OP_ADC, AM_ZPX, OF_NONE};
    table[0x76] = {OP_ROR, AM_ZPX, OF_RMW};
    table[0x77] = {OP_RRA, AM_ZPX, OF_RMW};   // RRA - Rotate Right then ADC (illegal)
    table[0x78] = {OP_SEI, AM_NON, OF_NONE};
    table[0x79] = {OP_ADC, AM_ABY, OF_SKIP_PAGE};
    table[0x7A] = {OP_NOP, AM_NON, OF_NONE};  // NOP - illegal NOP
    table[0x7B] = {OP_RRA, AM_ABY, OF_RMW};   // RRA - Rotate Right then ADC (illegal)
    table[0x7C] = {OP_NOP, AM_ABX, OF_SKIP_PAGE}; // NOP abs,X - illegal NOP
    table[0x7D] = {OP_ADC, AM_ABX, OF_SKIP_PAGE};
    table[0x7E] = {OP_ROR, AM_ABX, OF_RMW};
    table[0x7F] = {OP_RRA, AM_ABX, OF_RMW};   // RRA - Rotate Right then ADC (illegal)
    table[0x80] = {OP_NOP, AM_IMM, OF_NONE};  // NOP #imm - illegal NOP
    table[0x81] = {OP_STA, AM_INX, OF_NONE};
    table[0x82] = {OP_NOP, AM_IMM, OF_NONE};  // NOP #imm - illegal NOP
    table[0x83] = {OP_SAX, AM_INX, OF_NONE};  // SAX - Store A AND X (illegal)
    table[0x84] = {OP_STY, AM_ZER, OF_NONE};
    table[0x85] = {OP_STA, AM_ZER, OF_NONE};
    table[0x86] = {OP_STX, AM_ZER, OF_NONE};
    table[0x87] = {OP_SAX, AM_ZER, OF_NONE};  // SAX - Store A AND X (illegal)
    table[0x88] = {OP_DEY, AM_NON, OF_NONE};
    table[0x89] = {OP_NOP, AM_IMM, OF_NONE};  // NOP #imm - illegal NOP
    table[0x8A] = {OP_TXA, AM_NON, OF_NONE};
    table[0x8B] = {OP_XAA, AM_IMM, OF_NONE};  // XAA - Transfer X AND imm to A (illegal)
    table[0x8C] = {OP_STY, AM_ABS, OF_NONE};
    table[0x8D] = {OP_STA, AM_ABS, OF_NONE};
    table[0x8E] = {OP_STX, AM_ABS, OF_NONE};
    table[0x8F] = {OP_SAX, AM_ABS, OF_NONE};  // SAX - Store A AND X (illegal)
    table[0x90] = {OP_BCC, AM_REL, OF_NONE};
    table[0x91] = {OP_STA, AM_INY, OF_NONE};
    table[0x92] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x93] = {OP_SHA, AM_INY, OF_ILLEGAL_STORE};  // SHA - Store A AND X AND (addr_hi+1) (illegal)
    table[0x94] = {OP_STY, AM_ZPX, OF_NONE};
    table[0x95] = {OP_STA, AM_ZPX, OF_NONE};
    table[0x96] = {OP_STX, AM_ZPY, OF_NONE};
    table[0x97] = {OP_SAX, AM_ZPY, OF_NONE};  // SAX - Store A AND X (illegal)
    table[0x98] = {OP_TYA, AM_NON, OF_NONE};
    table[0x99] = {OP_STA, AM_ABY, OF_NONE};
    table[0x9A] = {OP_TXS, AM_NON, OF_NONE};
    table[0x9B] = {OP_SHS, AM_ABY, OF_ILLEGAL_STORE};  // SHS - Store (A AND X) AND ((addr_hi)+1) to S (illegal)
    table[0x9C] = {OP_SHY, AM_ABX, OF_ILLEGAL_STORE};  // SHY - Store Y AND ((addr_hi)+1) (illegal)
    table[0x9D] = {OP_STA, AM_ABX, OF_NONE};
    table[0x9E] = {OP_SHX, AM_ABY, OF_ILLEGAL_STORE};  // SHX - Store X AND ((addr_hi)+1) (illegal)
    table[0x9F] = {OP_SHA, AM_ABY, OF_ILLEGAL_STORE};  // SHA - Store A AND X AND (addr_hi+1) (illegal)
    table[0xA0] = {OP_LDY, AM_IMM, OF_NONE};
    table[0xA1] = {OP_LDA, AM_INX, OF_NONE};
    table[0xA2] = {OP_LDX, AM_IMM, OF_NONE};
    table[0xA3] = {OP_LAX, AM_INX, OF_NONE};  // LAX - Load A and X (illegal)
    table[0xA4] = {OP_LDY, AM_ZER, OF_NONE};
    table[0xA5] = {OP_LDA, AM_ZER, OF_NONE};
    table[0xA6] = {OP_LDX, AM_ZER, OF_NONE};
    table[0xA7] = {OP_LAX, AM_ZER, OF_NONE};  // LAX - Load A and X (illegal)
    table[0xA8] = {OP_TAY, AM_NON, OF_NONE};
    table[0xA9] = {OP_LDA, AM_IMM, OF_NONE};
    table[0xAA] = {OP_TAX, AM_NON, OF_NONE};
    table[0xAB] = {OP_LAX, AM_IMM, OF_NONE};  // LAX - Load A and X (illegal)
    table[0xAC] = {OP_LDY, AM_ABS, OF_NONE};
    table[0xAD] = {OP_LDA, AM_ABS, OF_NONE};
    table[0xAE] = {OP_LDX, AM_ABS, OF_NONE};
    table[0xAF] = {OP_LAX, AM_ABS, OF_NONE};  // LAX - Load A and X (illegal)
    table[0xB0] = {OP_BCS, AM_REL, OF_NONE};
    table[0xB1] = {OP_LDA, AM_INY, OF_SKIP_PAGE};
    table[0xB2] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xB3] = {OP_LAX, AM_INY, OF_SKIP_PAGE}; // LAX - Load A and X (illegal)
    table[0xB4] = {OP_LDY, AM_ZPX, OF_NONE};
    table[0xB5] = {OP_LDA, AM_ZPX, OF_NONE};
    table[0xB6] = {OP_LDX, AM_ZPY, OF_NONE};
    table[0xB7] = {OP_LAX, AM_ZPY, OF_NONE};  // LAX - Load A and X (illegal)
    table[0xB8] = {OP_CLV, AM_NON, OF_NONE};
    table[0xB9] = {OP_LDA, AM_ABY, OF_SKIP_PAGE};
    table[0xBA] = {OP_TSX, AM_NON, OF_NONE};
    table[0xBB] = {OP_LAS, AM_ABY, OF_SKIP_PAGE}; // LAS - Load A, X, S (illegal)
    table[0xBC] = {OP_LDY, AM_ABX, OF_SKIP_PAGE};
    table[0xBD] = {OP_LDA, AM_ABX, OF_SKIP_PAGE};
    table[0xBE] = {OP_LDX, AM_ABY, OF_SKIP_PAGE};
    table[0xBF] = {OP_LAX, AM_ABY, OF_SKIP_PAGE}; // LAX - Load A and X (illegal)
    table[0xC0] = {OP_CPY, AM_IMM, OF_NONE};
    table[0xC1] = {OP_CMP, AM_INX, OF_NONE};
    table[0xC2] = {OP_NOP, AM_IMM, OF_NONE};  // NOP #imm - illegal NOP
    table[0xC3] = {OP_DCP, AM_INX, OF_RMW};   // DCP - Decrement then Compare (illegal)
    table[0xC4] = {OP_CPY, AM_ZER, OF_NONE};
    table[0xC5] = {OP_CMP, AM_ZER, OF_NONE};
    table[0xC6] = {OP_DEC, AM_ZER, OF_RMW};
    table[0xC7] = {OP_DCP, AM_ZER, OF_RMW};   // DCP - Decrement then Compare (illegal)
    table[0xC8] = {OP_INY, AM_NON, OF_NONE};
    table[0xC9] = {OP_CMP, AM_IMM, OF_NONE};
    table[0xCA] = {OP_DEX, AM_NON, OF_NONE};
    table[0xCB] = {OP_SBX, AM_IMM, OF_NONE};  // SBX - Compare X with A AND imm (illegal) (also called AXS)
    table[0xCC] = {OP_CPY, AM_ABS, OF_NONE};
    table[0xCD] = {OP_CMP, AM_ABS, OF_NONE};
    table[0xCE] = {OP_DEC, AM_ABS, OF_RMW};
    table[0xCF] = {OP_DCP, AM_ABS, OF_RMW};   // DCP - Decrement then Compare (illegal)
    table[0xD0] = {OP_BNE, AM_REL, OF_NONE};
    table[0xD1] = {OP_CMP, AM_INY, OF_SKIP_PAGE};
    table[0xD2] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xD3] = {OP_DCP, AM_INY, OF_RMW};   // DCP - Decrement then Compare (illegal)
    table[0xD4] = {OP_NOP, AM_ZPX, OF_NONE};  // NOP zp,X - illegal NOP
    table[0xD5] = {OP_CMP, AM_ZPX, OF_NONE};
    table[0xD6] = {OP_DEC, AM_ZPX, OF_RMW};
    table[0xD7] = {OP_DCP, AM_ZPX, OF_RMW};   // DCP - Decrement then Compare (illegal)
    table[0xD8] = {OP_CLD, AM_NON, OF_NONE};
    table[0xD9] = {OP_CMP, AM_ABY, OF_SKIP_PAGE};
    table[0xDA] = {OP_NOP, AM_NON, OF_NONE};  // NOP - illegal NOP
    table[0xDB] = {OP_DCP, AM_ABY, OF_RMW};   // DCP - Decrement then Compare (illegal)
    table[0xDC] = {OP_NOP, AM_ABX, OF_SKIP_PAGE}; // NOP abs,X - illegal NOP
    table[0xDD] = {OP_CMP, AM_ABX, OF_SKIP_PAGE};
    table[0xDE] = {OP_DEC, AM_ABX, OF_RMW};
    table[0xDF] = {OP_DCP, AM_ABX, OF_RMW};   // DCP - Decrement then Compare (illegal)
    table[0xE0] = {OP_CPX, AM_IMM, OF_NONE};
    table[0xE1] = {OP_SBC, AM_INX, OF_NONE};
    table[0xE2] = {OP_NOP, AM_IMM, OF_NONE};  // NOP #imm - illegal NOP
    table[0xE3] = {OP_ISC, AM_INX, OF_RMW};   // ISC - Increment then SBC (illegal)
    table[0xE4] = {OP_CPX, AM_ZER, OF_NONE};
    table[0xE5] = {OP_SBC, AM_ZER, OF_NONE};
    table[0xE6] = {OP_INC, AM_ZER, OF_RMW};
    table[0xE7] = {OP_ISC, AM_ZER, OF_RMW};   // ISC - Increment then SBC (illegal)
    table[0xE8] = {OP_INX, AM_NON, OF_NONE};
    table[0xE9] = {OP_SBC, AM_IMM, OF_NONE};
    table[0xEA] = {OP_NOP, AM_NON, OF_NONE};
    table[0xEB] = {OP_SBC, AM_IMM, OF_NONE};  // SBC #imm - illegal SBC
    table[0xEC] = {OP_CPX, AM_ABS, OF_NONE};
    table[0xED] = {OP_SBC, AM_ABS, OF_NONE};
    table[0xEE] = {OP_INC, AM_ABS, OF_RMW};
    table[0xEF] = {OP_ISC, AM_ABS, OF_RMW};   // ISC - Increment then SBC (illegal)
    table[0xF0] = {OP_BEQ, AM_REL, OF_NONE};
    table[0xF1] = {OP_SBC, AM_INY, OF_SKIP_PAGE};
    table[0xF2] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xF3] = {OP_ISC, AM_INY, OF_RMW};   // ISC - Increment then SBC (illegal)
    table[0xF4] = {OP_NOP, AM_ZPX, OF_NONE};  // NOP zp,X - illegal NOP
    table[0xF5] = {OP_SBC, AM_ZPX, OF_NONE};
    table[0xF6] = {OP_INC, AM_ZPX, OF_RMW};
    table[0xF7] = {OP_ISC, AM_ZPX, OF_RMW};   // ISC - Increment then SBC (illegal)
    table[0xF8] = {OP_SED, AM_NON, OF_NONE};
    table[0xF9] = {OP_SBC, AM_ABY, OF_SKIP_PAGE};
    table[0xFA] = {OP_NOP, AM_NON, OF_NONE};  // NOP - illegal NOP
    table[0xFB] = {OP_ISC, AM_ABY, OF_RMW};   // ISC - Increment then SBC (illegal)
    table[0xFC] = {OP_NOP, AM_ABX, OF_SKIP_PAGE}; // NOP abs,X - illegal NOP
    table[0xFD] = {OP_SBC, AM_ABX, OF_SKIP_PAGE};
    table[0xFE] = {OP_INC, AM_ABX, OF_RMW};
    table[0xFF] = {OP_ISC, AM_ABX, OF_RMW};   // ISC - Increment then SBC (illegal)

    // Now apply processor-specific modifications based on CPUTraits
    
    // CMOS processors: Replace illegal opcodes with NOPs
    if (traits.has(CMOS_BASE)) {
        // WDC65C02: Specific illegal opcodes become 2-byte NOPs (AM_IMM)
        // These opcodes: 0x02, 0x22, 0x42, 0x62, 0x82, 0xC2, 0xE2
        table[0x02] = {OP_NOP, AM_IMM, OF_NONE};  // JAM -> 2-byte NOP
        table[0x22] = {OP_NOP, AM_IMM, OF_NONE};  // JAM -> 2-byte NOP
        table[0x42] = {OP_NOP, AM_IMM, OF_NONE};  // JAM -> 2-byte NOP
        table[0x62] = {OP_NOP, AM_IMM, OF_NONE};  // JAM -> 2-byte NOP
        table[0x82] = {OP_NOP, AM_IMM, OF_NONE};  // NOP #imm -> 2-byte NOP
        table[0xC2] = {OP_NOP, AM_IMM, OF_NONE};  // NOP #imm -> 2-byte NOP
        table[0xE2] = {OP_NOP, AM_IMM, OF_NONE};  // NOP #imm -> 2-byte NOP
        
        // Replace ALL other illegal opcodes with simple single-cycle NOPs (WDC65C02 behavior)
        table[0x03] = {OP_NOP, AM_NON, OF_NONE};  // SLO -> NOP
        table[0x07] = {OP_NOP, AM_NON, OF_NONE};  // SLO -> NOP
        table[0x0B] = {OP_NOP, AM_NON, OF_NONE};  // ANC -> NOP
        table[0x0F] = {OP_NOP, AM_NON, OF_NONE};  // SLO -> NOP
        table[0x12] = {OP_NOP, AM_NON, OF_NONE};  // JAM -> NOP
        table[0x13] = {OP_NOP, AM_NON, OF_NONE};  // SLO -> NOP
        table[0x17] = {OP_NOP, AM_NON, OF_NONE};  // SLO -> NOP
        table[0x1B] = {OP_NOP, AM_NON, OF_NONE};  // SLO -> NOP
        table[0x1F] = {OP_NOP, AM_NON, OF_NONE};  // SLO -> NOP
        table[0x23] = {OP_NOP, AM_NON, OF_NONE};  // RLA -> NOP
        table[0x27] = {OP_NOP, AM_NON, OF_NONE};  // RLA -> NOP
        table[0x2B] = {OP_NOP, AM_NON, OF_NONE};  // ANC -> NOP
        table[0x2F] = {OP_NOP, AM_ABS, OF_NONE};  // RLA -> NOP (special case: must match ProcessorTests expectations)
        table[0x32] = {OP_NOP, AM_NON, OF_NONE};  // JAM -> NOP
        table[0x33] = {OP_NOP, AM_NON, OF_NONE};  // RLA -> NOP
        table[0x37] = {OP_NOP, AM_NON, OF_NONE};  // RLA -> NOP
        table[0x3B] = {OP_NOP, AM_NON, OF_NONE};  // RLA -> NOP
        table[0x3F] = {OP_NOP, AM_NON, OF_NONE};  // RLA -> NOP
        table[0x43] = {OP_NOP, AM_NON, OF_NONE};  // SRE -> NOP
        table[0x47] = {OP_NOP, AM_NON, OF_NONE};  // SRE -> NOP
        table[0x4B] = {OP_NOP, AM_NON, OF_NONE};  // ASR -> NOP
        table[0x4F] = {OP_NOP, AM_NON, OF_NONE};  // SRE -> NOP
        table[0x52] = {OP_NOP, AM_NON, OF_NONE};  // JAM -> NOP
        table[0x53] = {OP_NOP, AM_NON, OF_NONE};  // SRE -> NOP
        table[0x57] = {OP_NOP, AM_NON, OF_NONE};  // SRE -> NOP
        table[0x5B] = {OP_NOP, AM_NON, OF_NONE};  // SRE -> NOP
        table[0x5F] = {OP_NOP, AM_NON, OF_NONE};  // SRE -> NOP
        table[0x63] = {OP_NOP, AM_NON, OF_NONE};  // RRA -> NOP
        table[0x67] = {OP_NOP, AM_NON, OF_NONE};  // RRA -> NOP
        table[0x6B] = {OP_NOP, AM_NON, OF_NONE};  // ARR -> NOP
        table[0x6F] = {OP_NOP, AM_NON, OF_NONE};  // RRA -> NOP
        table[0x72] = {OP_NOP, AM_NON, OF_NONE};  // JAM -> NOP
        table[0x73] = {OP_NOP, AM_NON, OF_NONE};  // RRA -> NOP
        table[0x77] = {OP_NOP, AM_NON, OF_NONE};  // RRA -> NOP
        table[0x7B] = {OP_NOP, AM_NON, OF_NONE};  // RRA -> NOP
        table[0x7F] = {OP_NOP, AM_NON, OF_NONE};  // RRA -> NOP
        table[0x83] = {OP_NOP, AM_NON, OF_NONE};  // SAX -> NOP
        table[0x87] = {OP_NOP, AM_NON, OF_NONE};  // SAX -> NOP
        table[0x8B] = {OP_NOP, AM_NON, OF_NONE};  // XAA -> NOP
        table[0x8F] = {OP_NOP, AM_NON, OF_NONE};  // SAX -> NOP
        table[0x92] = {OP_NOP, AM_NON, OF_NONE};  // JAM -> NOP
        table[0x93] = {OP_NOP, AM_NON, OF_NONE};  // SHA -> NOP
        table[0x97] = {OP_NOP, AM_NON, OF_NONE};  // SAX -> NOP
        table[0x9B] = {OP_NOP, AM_NON, OF_NONE};  // SHS -> NOP
        table[0x9C] = {OP_NOP, AM_NON, OF_NONE};  // SHY -> NOP (will be overridden for 65C02)
        table[0x9E] = {OP_NOP, AM_NON, OF_NONE};  // SHX -> NOP
        table[0x9F] = {OP_NOP, AM_NON, OF_NONE};  // SHA -> NOP
        table[0xA3] = {OP_NOP, AM_NON, OF_NONE};  // LAX -> NOP
        table[0xA7] = {OP_NOP, AM_NON, OF_NONE};  // LAX -> NOP
        table[0xAB] = {OP_NOP, AM_NON, OF_NONE};  // LAX -> NOP
        table[0xAF] = {OP_NOP, AM_NON, OF_NONE};  // LAX -> NOP
        table[0xB2] = {OP_NOP, AM_NON, OF_NONE};  // JAM -> NOP
        table[0xB3] = {OP_NOP, AM_NON, OF_NONE};  // LAX -> NOP
        table[0xB7] = {OP_NOP, AM_NON, OF_NONE};  // LAX -> NOP
        table[0xBB] = {OP_NOP, AM_NON, OF_NONE};  // LAS -> NOP
        table[0xBF] = {OP_NOP, AM_NON, OF_NONE};  // LAX -> NOP
        table[0xC3] = {OP_NOP, AM_NON, OF_NONE};  // DCP -> NOP
        table[0xC7] = {OP_NOP, AM_NON, OF_NONE};  // DCP -> NOP
        table[0xCB] = {OP_NOP, AM_NON, OF_NONE};  // SBX -> NOP (will be overridden for 65C02)
        table[0xCF] = {OP_NOP, AM_NON, OF_NONE};  // DCP -> NOP
        table[0xD2] = {OP_NOP, AM_NON, OF_NONE};  // JAM -> NOP
        table[0xD3] = {OP_NOP, AM_NON, OF_NONE};  // DCP -> NOP
        table[0xD7] = {OP_NOP, AM_NON, OF_NONE};  // DCP -> NOP
        table[0xDA] = {OP_NOP, AM_NON, OF_NONE};  // NOP -> NOP (will be overridden for 65C02)
        table[0xDB] = {OP_NOP, AM_NON, OF_NONE};  // DCP -> NOP (will be overridden for 65C02)
        table[0xDF] = {OP_NOP, AM_NON, OF_NONE};  // DCP -> NOP
        table[0xE3] = {OP_NOP, AM_NON, OF_NONE};  // ISC -> NOP
        table[0xE7] = {OP_NOP, AM_NON, OF_NONE};  // ISC -> NOP
        table[0xEB] = {OP_NOP, AM_NON, OF_NONE};  // SBC -> NOP
        table[0xEF] = {OP_NOP, AM_NON, OF_NONE};  // ISC -> NOP
        table[0xF2] = {OP_NOP, AM_NON, OF_NONE};  // JAM -> NOP
        table[0xF3] = {OP_NOP, AM_NON, OF_NONE};  // ISC -> NOP
        table[0xF7] = {OP_NOP, AM_NON, OF_NONE};  // ISC -> NOP
        table[0xFA] = {OP_NOP, AM_NON, OF_NONE};  // NOP -> NOP (will be overridden for 65C02)
        table[0xFB] = {OP_NOP, AM_NON, OF_NONE};  // ISC -> NOP
        table[0xFF] = {OP_NOP, AM_NON, OF_NONE};  // ISC -> NOP
        
        // Add 65C02 enhancements
        table[0x04] = {OP_TSB, AM_ZER, OF_RMW};   // TSB zero page
        table[0x0C] = {OP_TSB, AM_ABS, OF_RMW};   // TSB absolute
        table[0x14] = {OP_TRB, AM_ZER, OF_RMW};   // TRB zero page
        table[0x1C] = {OP_TRB, AM_ABS, OF_RMW};   // TRB absolute
        table[0x5A] = {OP_PHY, AM_NON, OF_NONE};  // PHY
        table[0x64] = {OP_STZ, AM_ZER, OF_NONE};  // STZ zero page
        table[0x7A] = {OP_PLY, AM_NON, OF_NONE};  // PLY
        table[0x80] = {OP_BRA, AM_REL, OF_NONE};  // BRA
        table[0x9C] = {OP_STZ, AM_ABS, OF_NONE};  // STZ absolute
        table[0xCB] = {OP_WAI, AM_NON, OF_NONE};  // WAI
        table[0xDA] = {OP_PHX, AM_NON, OF_NONE};  // PHX
        table[0xDB] = {OP_STP, AM_NON, OF_NONE};  // STP
        table[0xFA] = {OP_PLX, AM_NON, OF_NONE};  // PLX
    }
    
    // Rockwell 65C02 modifications (add RMB/SMB/BBR/BBS instructions)
    if (traits.has(ROCKWELL_BITS)) {
        // Add Rockwell bit manipulation instructions (RMB/SMB)
        table[0x07] = {OP_RMB0, AM_ZER, OF_RMW}; // RMB0
        table[0x17] = {OP_RMB1, AM_ZER, OF_RMW}; // RMB1
        table[0x27] = {OP_RMB2, AM_ZER, OF_RMW}; // RMB2
        table[0x37] = {OP_RMB3, AM_ZER, OF_RMW}; // RMB3
        table[0x47] = {OP_RMB4, AM_ZER, OF_RMW}; // RMB4
        table[0x57] = {OP_RMB5, AM_ZER, OF_RMW}; // RMB5
        table[0x67] = {OP_RMB6, AM_ZER, OF_RMW}; // RMB6
        table[0x77] = {OP_RMB7, AM_ZER, OF_RMW}; // RMB7
        table[0x87] = {OP_SMB0, AM_ZER, OF_RMW}; // SMB0
        table[0x97] = {OP_SMB1, AM_ZER, OF_RMW}; // SMB1
        table[0xA7] = {OP_SMB2, AM_ZER, OF_RMW}; // SMB2
        table[0xB7] = {OP_SMB3, AM_ZER, OF_RMW}; // SMB3
        table[0xC7] = {OP_SMB4, AM_ZER, OF_RMW}; // SMB4
        table[0xD7] = {OP_SMB5, AM_ZER, OF_RMW}; // SMB5
        table[0xE7] = {OP_SMB6, AM_ZER, OF_RMW}; // SMB6
        table[0xF7] = {OP_SMB7, AM_ZER, OF_RMW}; // SMB7
        
        // BBR/BBS instructions (branch on bit reset/set) - use special ZPR addressing mode
        table[0x0F] = {OP_BBR0, AM_ZPR, OF_NONE}; // BBR0
        table[0x1F] = {OP_BBR1, AM_ZPR, OF_NONE}; // BBR1
        table[0x2F] = {OP_BBR2, AM_ZPR, OF_NONE}; // BBR2
        table[0x3F] = {OP_BBR3, AM_ZPR, OF_NONE}; // BBR3
        table[0x4F] = {OP_BBR4, AM_ZPR, OF_NONE}; // BBR4
        table[0x5F] = {OP_BBR5, AM_ZPR, OF_NONE}; // BBR5
        table[0x6F] = {OP_BBR6, AM_ZPR, OF_NONE}; // BBR6
        table[0x7F] = {OP_BBR7, AM_ZPR, OF_NONE}; // BBR7
        table[0x8F] = {OP_BBS0, AM_ZPR, OF_NONE}; // BBS0
        table[0x9F] = {OP_BBS1, AM_ZPR, OF_NONE}; // BBS1
        table[0xAF] = {OP_BBS2, AM_ZPR, OF_NONE}; // BBS2
        table[0xBF] = {OP_BBS3, AM_ZPR, OF_NONE}; // BBS3
        table[0xCF] = {OP_BBS4, AM_ZPR, OF_NONE}; // BBS4
        table[0xDF] = {OP_BBS5, AM_ZPR, OF_NONE}; // BBS5
        table[0xEF] = {OP_BBS6, AM_ZPR, OF_NONE}; // BBS6
        table[0xFF] = {OP_BBS7, AM_ZPR, OF_NONE}; // BBS7
    }
    
    // WDC 65C816 modifications (16-bit enhanced instructions)
    if (traits.has(C816_16BIT)) {
        // Add 65C816 specific opcodes
        table[0x0B] = {OP_PHD, AM_NON, OF_NONE};  // PHD
        table[0x22] = {OP_JSL, AM_ABS, OF_NONE};  // JSL
        table[0x2B] = {OP_PLD, AM_NON, OF_NONE};  // PLD
        table[0x4B] = {OP_PHK, AM_NON, OF_NONE};  // PHK
        table[0x6B] = {OP_RTL, AM_NON, OF_NONE};  // RTL
        table[0x8B] = {OP_PHB, AM_NON, OF_NONE};  // PHB
        table[0xAB] = {OP_PLB, AM_NON, OF_NONE};  // PLB
        table[0xC2] = {OP_REP, AM_IMM, OF_NONE};  // REP
        table[0xE2] = {OP_SEP, AM_IMM, OF_NONE};  // SEP
        table[0xF4] = {OP_PEA, AM_ABS, OF_NONE};  // PEA
        table[0xFB] = {OP_XCE, AM_NON, OF_NONE};  // XCE
        
        // Additional 65C816 instructions
        table[0x42] = {OP_WDM, AM_IMM, OF_NONE};  // WDM
        table[0x44] = {OP_MVP, AM_NON, OF_NONE};  // MVP
        table[0x54] = {OP_MVN, AM_NON, OF_NONE};  // MVN
        table[0x62] = {OP_PER, AM_REL, OF_NONE}; // PER
        table[0xD4] = {OP_PEI, AM_ZPI, OF_NONE}; // PEI
        table[0xF4] = {OP_PEA, AM_ABS, OF_NONE}; // PEA
    }
    
    return table;
}

// ============================================================================
// TEMPLATE SPECIALIZATIONS FOR EACH PROCESSOR TYPE
// ============================================================================

// Template specializations are not needed for CPUTraits reference parameters!
// The template function generate_opcode_table<Traits>() where Traits is a reference
// to a CPUTraits instance will automatically use the generic implementation
// that calls generate_opcode_table_for_traits(Traits) at compile time.
//
// This is because template<const CPUTraits& Traits> means the function is
// instantiated with the actual CPUTraits instance, not a type.
// The compiler will generate the correct implementation automatically.

#endif // FAM65XX_SKIP_IMPLEMENTATION