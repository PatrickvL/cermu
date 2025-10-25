/*
 * opcode_tables.inc - Processor-Specific Opcode Table Specializations
 *
 * This file contains template specializations of the opcode table generation
 * function for each supported processor type. Each processor gets its own
 * compile-time generated opcode table based on its feature set.
 */

#include <array>
#include "../fam65xx_types.h"
#include "../fam65xx_processor_traits.hpp"

// This file is included inside the fam65xx_cpp namespace in fam65xx.hpp
// so all template specializations are automatically in the correct namespace

// ============================================================================
// OPCODE TABLE GENERATION SPECIALIZATIONS
// ============================================================================

// NES 6502 (no BCD, no illegal opcodes) - Base implementation with explicit JAM for undefined opcodes
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<NES6502Tag>() {
    std::array<opcode_info_t, 256> table{};
    
    // All 256 opcodes explicitly defined - valid opcodes and JAM for undefined ones
    table[0x00] = {OP_BRK, AM_NON, OF_NONE};
    table[0x01] = {OP_ORA, AM_INX, OF_NONE};
    table[0x02] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x03] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x04] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x05] = {OP_ORA, AM_ZER, OF_NONE};
    table[0x06] = {OP_ASL, AM_ZER, OF_RMW};
    table[0x07] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x08] = {OP_PHP, AM_NON, OF_NONE};
    table[0x09] = {OP_ORA, AM_IMM, OF_NONE};
    table[0x0A] = {OP_ASL, AM_ACC, OF_NONE};
    table[0x0B] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x0C] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x0D] = {OP_ORA, AM_ABS, OF_NONE};
    table[0x0E] = {OP_ASL, AM_ABS, OF_RMW};
    table[0x0F] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x10] = {OP_BPL, AM_REL, OF_NONE};
    table[0x11] = {OP_ORA, AM_INY, OF_SKIP_PAGE};
    table[0x12] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x13] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x14] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x15] = {OP_ORA, AM_ZPX, OF_NONE};
    table[0x16] = {OP_ASL, AM_ZPX, OF_RMW};
    table[0x17] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x18] = {OP_CLC, AM_NON, OF_NONE};
    table[0x19] = {OP_ORA, AM_ABY, OF_SKIP_PAGE};
    table[0x1A] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x1B] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x1C] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x1D] = {OP_ORA, AM_ABX, OF_SKIP_PAGE};
    table[0x1E] = {OP_ASL, AM_ABX, OF_RMW};
    table[0x1F] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x20] = {OP_JSR, AM_ABS, OF_NONE};
    table[0x21] = {OP_AND, AM_INX, OF_NONE};
    table[0x22] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x23] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x24] = {OP_BIT, AM_ZER, OF_NONE};
    table[0x25] = {OP_AND, AM_ZER, OF_NONE};
    table[0x26] = {OP_ROL, AM_ZER, OF_RMW};
    table[0x27] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x28] = {OP_PLP, AM_NON, OF_NONE};
    table[0x29] = {OP_AND, AM_IMM, OF_NONE};
    table[0x2A] = {OP_ROL, AM_ACC, OF_NONE};
    table[0x2B] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x2C] = {OP_BIT, AM_ABS, OF_NONE};
    table[0x2D] = {OP_AND, AM_ABS, OF_NONE};
    table[0x2E] = {OP_ROL, AM_ABS, OF_RMW};
    table[0x2F] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x30] = {OP_BMI, AM_REL, OF_NONE};
    table[0x31] = {OP_AND, AM_INY, OF_SKIP_PAGE};
    table[0x32] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x33] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x34] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x35] = {OP_AND, AM_ZPX, OF_NONE};
    table[0x36] = {OP_ROL, AM_ZPX, OF_RMW};
    table[0x37] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x38] = {OP_SEC, AM_NON, OF_NONE};
    table[0x39] = {OP_AND, AM_ABY, OF_SKIP_PAGE};
    table[0x3A] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x3B] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x3C] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x3D] = {OP_AND, AM_ABX, OF_SKIP_PAGE};
    table[0x3E] = {OP_ROL, AM_ABX, OF_RMW};
    table[0x3F] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x40] = {OP_RTI, AM_NON, OF_NONE};
    table[0x41] = {OP_EOR, AM_INX, OF_NONE};
    table[0x42] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x43] = {OP_SRE, AM_INX, OF_RMW};  // SRE - Shift Right then EOR (illegal but implemented)
    table[0x44] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x45] = {OP_EOR, AM_ZER, OF_NONE};
    table[0x46] = {OP_LSR, AM_ZER, OF_RMW};
    table[0x47] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x48] = {OP_PHA, AM_NON, OF_NONE};
    table[0x49] = {OP_EOR, AM_IMM, OF_NONE};
    table[0x4A] = {OP_LSR, AM_ACC, OF_NONE};
    table[0x4B] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x4C] = {OP_JMP, AM_ABS, OF_NONE};
    table[0x4D] = {OP_EOR, AM_ABS, OF_NONE};
    table[0x4E] = {OP_LSR, AM_ABS, OF_RMW};
    table[0x4F] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x50] = {OP_BVC, AM_REL, OF_NONE};
    table[0x51] = {OP_EOR, AM_INY, OF_SKIP_PAGE};
    table[0x52] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x53] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x54] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x55] = {OP_EOR, AM_ZPX, OF_NONE};
    table[0x56] = {OP_LSR, AM_ZPX, OF_RMW};
    table[0x57] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x58] = {OP_CLI, AM_NON, OF_NONE};
    table[0x59] = {OP_EOR, AM_ABY, OF_SKIP_PAGE};
    table[0x5A] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x5B] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x5C] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x5D] = {OP_EOR, AM_ABX, OF_SKIP_PAGE};
    table[0x5E] = {OP_LSR, AM_ABX, OF_RMW};
    table[0x5F] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x60] = {OP_RTS, AM_NON, OF_NONE};
    table[0x61] = {OP_ADC, AM_INX, OF_NONE};
    table[0x62] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x63] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x64] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x65] = {OP_ADC, AM_ZER, OF_NONE};
    table[0x66] = {OP_ROR, AM_ZER, OF_RMW};
    table[0x67] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x68] = {OP_PLA, AM_NON, OF_NONE};
    table[0x69] = {OP_ADC, AM_IMM, OF_NONE};
    table[0x6A] = {OP_ROR, AM_ACC, OF_NONE};
    table[0x6B] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x6C] = {OP_JMP, AM_IND, OF_NONE};
    table[0x6D] = {OP_ADC, AM_ABS, OF_NONE};
    table[0x6E] = {OP_ROR, AM_ABS, OF_RMW};
    table[0x6F] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x70] = {OP_BVS, AM_REL, OF_NONE};
    table[0x71] = {OP_ADC, AM_INY, OF_SKIP_PAGE};
    table[0x72] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x73] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x74] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x75] = {OP_ADC, AM_ZPX, OF_NONE};
    table[0x76] = {OP_ROR, AM_ZPX, OF_RMW};
    table[0x77] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x78] = {OP_SEI, AM_NON, OF_NONE};
    table[0x79] = {OP_ADC, AM_ABY, OF_SKIP_PAGE};
    table[0x7A] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x7B] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x7C] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x7D] = {OP_ADC, AM_ABX, OF_SKIP_PAGE};
    table[0x7E] = {OP_ROR, AM_ABX, OF_RMW};
    table[0x7F] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x80] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x81] = {OP_STA, AM_INX, OF_NONE};
    table[0x82] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x83] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x84] = {OP_STY, AM_ZER, OF_NONE};
    table[0x85] = {OP_STA, AM_ZER, OF_NONE};
    table[0x86] = {OP_STX, AM_ZER, OF_NONE};
    table[0x87] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x88] = {OP_DEY, AM_NON, OF_NONE};
    table[0x89] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x8A] = {OP_TXA, AM_NON, OF_NONE};
    table[0x8B] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x8C] = {OP_STY, AM_ABS, OF_NONE};
    table[0x8D] = {OP_STA, AM_ABS, OF_NONE};
    table[0x8E] = {OP_STX, AM_ABS, OF_NONE};
    table[0x8F] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x90] = {OP_BCC, AM_REL, OF_NONE};
    table[0x91] = {OP_STA, AM_INY, OF_NONE};
    table[0x92] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x93] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x94] = {OP_STY, AM_ZPX, OF_NONE};
    table[0x95] = {OP_STA, AM_ZPX, OF_NONE};
    table[0x96] = {OP_STX, AM_ZPY, OF_NONE};
    table[0x97] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x98] = {OP_TYA, AM_NON, OF_NONE};
    table[0x99] = {OP_STA, AM_ABY, OF_NONE};
    table[0x9A] = {OP_TXS, AM_NON, OF_NONE};
    table[0x9B] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x9C] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x9D] = {OP_STA, AM_ABX, OF_NONE};
    table[0x9E] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0x9F] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xA0] = {OP_LDY, AM_IMM, OF_NONE};
    table[0xA1] = {OP_LDA, AM_INX, OF_NONE};
    table[0xA2] = {OP_LDX, AM_IMM, OF_NONE};
    table[0xA3] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xA4] = {OP_LDY, AM_ZER, OF_NONE};
    table[0xA5] = {OP_LDA, AM_ZER, OF_NONE};
    table[0xA6] = {OP_LDX, AM_ZER, OF_NONE};
    table[0xA7] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xA8] = {OP_TAY, AM_NON, OF_NONE};
    table[0xA9] = {OP_LDA, AM_IMM, OF_NONE};
    table[0xAA] = {OP_TAX, AM_NON, OF_NONE};
    table[0xAB] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xAC] = {OP_LDY, AM_ABS, OF_NONE};
    table[0xAD] = {OP_LDA, AM_ABS, OF_NONE};
    table[0xAE] = {OP_LDX, AM_ABS, OF_NONE};
    table[0xAF] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xB0] = {OP_BCS, AM_REL, OF_NONE};
    table[0xB1] = {OP_LDA, AM_INY, OF_SKIP_PAGE};
    table[0xB2] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xB3] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xB4] = {OP_LDY, AM_ZPX, OF_NONE};
    table[0xB5] = {OP_LDA, AM_ZPX, OF_NONE};
    table[0xB6] = {OP_LDX, AM_ZPY, OF_NONE};
    table[0xB7] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xB8] = {OP_CLV, AM_NON, OF_NONE};
    table[0xB9] = {OP_LDA, AM_ABY, OF_SKIP_PAGE};
    table[0xBA] = {OP_TSX, AM_NON, OF_NONE};
    table[0xBB] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xBC] = {OP_LDY, AM_ABX, OF_SKIP_PAGE};
    table[0xBD] = {OP_LDA, AM_ABX, OF_SKIP_PAGE};
    table[0xBE] = {OP_LDX, AM_ABY, OF_SKIP_PAGE};
    table[0xBF] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xC0] = {OP_CPY, AM_IMM, OF_NONE};
    table[0xC1] = {OP_CMP, AM_INX, OF_NONE};
    table[0xC2] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xC3] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xC4] = {OP_CPY, AM_ZER, OF_NONE};
    table[0xC5] = {OP_CMP, AM_ZER, OF_NONE};
    table[0xC6] = {OP_DEC, AM_ZER, OF_RMW};
    table[0xC7] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xC8] = {OP_INY, AM_NON, OF_NONE};
    table[0xC9] = {OP_CMP, AM_IMM, OF_NONE};
    table[0xCA] = {OP_DEX, AM_NON, OF_NONE};
    table[0xCB] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xCC] = {OP_CPY, AM_ABS, OF_NONE};
    table[0xCD] = {OP_CMP, AM_ABS, OF_NONE};
    table[0xCE] = {OP_DEC, AM_ABS, OF_RMW};
    table[0xCF] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xD0] = {OP_BNE, AM_REL, OF_NONE};
    table[0xD1] = {OP_CMP, AM_INY, OF_SKIP_PAGE};
    table[0xD2] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xD3] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xD4] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xD5] = {OP_CMP, AM_ZPX, OF_NONE};
    table[0xD6] = {OP_DEC, AM_ZPX, OF_RMW};
    table[0xD7] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xD8] = {OP_CLD, AM_NON, OF_NONE};
    table[0xD9] = {OP_CMP, AM_ABY, OF_SKIP_PAGE};
    table[0xDA] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xDB] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xDC] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xDD] = {OP_CMP, AM_ABX, OF_SKIP_PAGE};
    table[0xDE] = {OP_DEC, AM_ABX, OF_RMW};
    table[0xDF] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xE0] = {OP_CPX, AM_IMM, OF_NONE};
    table[0xE1] = {OP_SBC, AM_INX, OF_NONE};
    table[0xE2] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xE3] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xE4] = {OP_CPX, AM_ZER, OF_NONE};
    table[0xE5] = {OP_SBC, AM_ZER, OF_NONE};
    table[0xE6] = {OP_INC, AM_ZER, OF_RMW};
    table[0xE7] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xE8] = {OP_INX, AM_NON, OF_NONE};
    table[0xE9] = {OP_SBC, AM_IMM, OF_NONE};
    table[0xEA] = {OP_NOP, AM_NON, OF_NONE};
    table[0xEB] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xEC] = {OP_CPX, AM_ABS, OF_NONE};
    table[0xED] = {OP_SBC, AM_ABS, OF_NONE};
    table[0xEE] = {OP_INC, AM_ABS, OF_RMW};
    table[0xEF] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xF0] = {OP_BEQ, AM_REL, OF_NONE};
    table[0xF1] = {OP_SBC, AM_INY, OF_SKIP_PAGE};
    table[0xF2] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xF3] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xF4] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xF5] = {OP_SBC, AM_ZPX, OF_NONE};
    table[0xF6] = {OP_INC, AM_ZPX, OF_RMW};
    table[0xF7] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xF8] = {OP_SED, AM_NON, OF_NONE};
    table[0xF9] = {OP_SBC, AM_ABY, OF_SKIP_PAGE};
    table[0xFA] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xFB] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xFC] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode
    table[0xFD] = {OP_SBC, AM_ABX, OF_SKIP_PAGE};
    table[0xFE] = {OP_INC, AM_ABX, OF_RMW};
    table[0xFF] = {OP_JAM, AM_NON, OF_NONE};  // KIL/JAM - illegal opcode

    return table;
}

// MOS 6502 (original NMOS with illegal opcodes)
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<MOS6502Tag>() {
    auto table = generate_opcode_table<NES6502Tag>();
    
    // Replace specific JAM opcodes with actual illegal opcodes for NMOS 6502
    table[0x83] = {OP_SAX, AM_INX, OF_NONE};
    table[0x87] = {OP_SAX, AM_ZER, OF_NONE};
    table[0x8F] = {OP_SAX, AM_ABS, OF_NONE};
    table[0x97] = {OP_SAX, AM_ZPY, OF_NONE};
    table[0xA3] = {OP_LAX, AM_INX, OF_NONE};
    table[0xA7] = {OP_LAX, AM_ZER, OF_NONE};
    table[0xAF] = {OP_LAX, AM_ABS, OF_NONE};
    table[0xB3] = {OP_LAX, AM_INY, OF_SKIP_PAGE};
    table[0xB7] = {OP_LAX, AM_ZPY, OF_NONE};
    table[0xBF] = {OP_LAX, AM_ABY, OF_SKIP_PAGE};
    
    return table;
}

// MOS 6510 (C64/C128 variant) - Same as 6502 with I/O port
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<MOS6510Tag>() {
    return generate_opcode_table<MOS6502Tag>();
}

// WDC 65C02 (CMOS variant) - Enhanced 6502 without illegal opcodes
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<WDC65C02Tag>() {
    auto table = generate_opcode_table<NES6502Tag>();
    
    // Add 65C02 enhancements incrementally
    table[0x04] = {OP_TSB, AM_ZER, OF_RMW};
    table[0x14] = {OP_TRB, AM_ZER, OF_RMW};
    table[0x5A] = {OP_PHY, AM_NON, OF_NONE};
    table[0x64] = {OP_STZ, AM_ZER, OF_NONE};
    table[0x7A] = {OP_PLY, AM_NON, OF_NONE};
    table[0x80] = {OP_BRA, AM_REL, OF_NONE};
    table[0x9C] = {OP_STZ, AM_ABS, OF_NONE};
    table[0xCB] = {OP_WAI, AM_NON, OF_NONE};
    table[0xDA] = {OP_PHX, AM_NON, OF_NONE};
    table[0xDB] = {OP_STP, AM_NON, OF_NONE};
    table[0xFA] = {OP_PLX, AM_NON, OF_NONE};
    
    return table;
}

// WDC 65C816 (16-bit enhanced) - 65C02 base + 16-bit extensions
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<WDC65C816Tag>() {
    auto table = generate_opcode_table<WDC65C02Tag>();
    
    // Add 65C816 specific opcodes incrementally (placeholders for now)
    table[0x0B] = {OP_PHD, AM_NON, OF_NONE};
    table[0x22] = {OP_JSL, AM_ABS, OF_NONE};
    table[0x2B] = {OP_PLD, AM_NON, OF_NONE};
    table[0x4B] = {OP_PHK, AM_NON, OF_NONE};
    table[0x6B] = {OP_RTL, AM_NON, OF_NONE};
    table[0x8B] = {OP_PHB, AM_NON, OF_NONE};
    table[0xAB] = {OP_PLB, AM_NON, OF_NONE};
    table[0xC2] = {OP_REP, AM_IMM, OF_NONE};
    table[0xE2] = {OP_SEP, AM_IMM, OF_NONE};
    table[0xF4] = {OP_PEA, AM_ABS, OF_NONE};
    table[0xFB] = {OP_XCE, AM_NON, OF_NONE};

    return table;
}

// Rockwell 65C02 (CMOS with RMB/SMB/BBR/BBS) - 65C02 + bit manipulation
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<Rockwell65C02Tag>() {
    auto table = generate_opcode_table<WDC65C02Tag>();
    
    // Add Rockwell bit manipulation instructions incrementally (RMB/SMB)
    table[0x07] = {OP_RMB0, AM_ZER, OF_RMW}; // RMB0 (placeholder)
    table[0x17] = {OP_RMB1, AM_ZER, OF_RMW}; // RMB1 (placeholder)
    table[0x27] = {OP_RMB2, AM_ZER, OF_RMW}; // RMB2 (placeholder)
    table[0x37] = {OP_RMB3, AM_ZER, OF_RMW}; // RMB3 (placeholder)
    table[0x47] = {OP_RMB4, AM_ZER, OF_RMW}; // RMB4 (placeholder)
    table[0x57] = {OP_RMB5, AM_ZER, OF_RMW}; // RMB5 (placeholder)
    table[0x67] = {OP_RMB6, AM_ZER, OF_RMW}; // RMB6 (placeholder)
    table[0x77] = {OP_RMB7, AM_ZER, OF_RMW}; // RMB7 (placeholder)
    table[0x87] = {OP_SMB0, AM_ZER, OF_RMW}; // SMB0 (placeholder)
    table[0x97] = {OP_SMB1, AM_ZER, OF_RMW}; // SMB1 (placeholder)
    table[0xA7] = {OP_SMB2, AM_ZER, OF_RMW}; // SMB2 (placeholder)
    table[0xB7] = {OP_SMB3, AM_ZER, OF_RMW}; // SMB3 (placeholder)
    table[0xC7] = {OP_SMB4, AM_ZER, OF_RMW}; // SMB4 (placeholder)
    table[0xD7] = {OP_SMB5, AM_ZER, OF_RMW}; // SMB5 (placeholder)
    table[0xE7] = {OP_SMB6, AM_ZER, OF_RMW}; // SMB6 (placeholder)
    table[0xF7] = {OP_SMB7, AM_ZER, OF_RMW}; // SMB7 (placeholder)

    return table;
}