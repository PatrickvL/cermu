/*
 * opcode_tables.inc.hpp - CPUTraits-Based Opcode Table Generation
 *
 * This file contains the CPUTraits-based opcode table generation function.
 * Unlike other .inc.hpp files that contain member functions, this file contains
 * a free function that generates opcode tables based on CPUTraits
 * configuration.
 */

// Always include required headers for both standalone analysis and production
#include "../fam65xx_processor_traits.hpp"
#include "../fam65xx_types.h"
#include <array>

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// CPUTRAITS-BASED OPCODE TABLE GENERATION
// ============================================================================

// CPUTraits-based opcode table generation function
constexpr std::array<opcode_info_t, 256>
generate_opcode_table_for_traits(const fam65xx::CPUTraits &traits) {
  // Start with base NES 6502/RICOH 2A03 implementation with explicit illegal
  // opcodes
  std::array<opcode_info_t, 256> table{};

  // All 256 opcodes explicitly defined - valid opcodes and proper illegal
  // opcodes for hardware accuracy
  table[0x00] = {OP::BRK, AM::NON, OF::NONE};
  table[0x01] = {OP::ORA, AM::INX, OF::NONE};
  table[0x02] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0x03] = {OP::SLO, AM::INX,
                 OF::RMW}; // SLO - Shift Left then OR (illegal)
  table[0x04] = {OP::NOP, AM::ZER, OF::NONE}; // NOP zp - illegal NOP
  table[0x05] = {OP::ORA, AM::ZER, OF::NONE};
  table[0x06] = {OP::ASL, AM::ZER, OF::RMW};
  table[0x07] = {OP::SLO, AM::ZER,
                 OF::RMW}; // SLO - Shift Left then OR (illegal)
  table[0x08] = {OP::PHP, AM::NON, OF::NONE};
  table[0x09] = {OP::ORA, AM::IMM, OF::NONE};
  table[0x0A] = {OP::ASL, AM::ACC, OF::NONE};
  table[0x0B] = {OP::ANC, AM::IMM, OF::NONE}; // ANC - AND with carry (illegal)
  table[0x0C] = {OP::NOP, AM::ABS, OF::NONE}; // NOP abs - illegal NOP
  table[0x0D] = {OP::ORA, AM::ABS, OF::NONE};
  table[0x0E] = {OP::ASL, AM::ABS, OF::RMW};
  table[0x0F] = {OP::SLO, AM::ABS,
                 OF::RMW}; // SLO - Shift Left then OR (illegal)
  table[0x10] = {OP::BPL, AM::REL, OF::NONE};
  table[0x11] = {OP::ORA, AM::INY, OF::SKIP_PAGE};
  table[0x12] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0x13] = {OP::SLO, AM::INY,
                 OF::RMW}; // SLO - Shift Left then OR (illegal)
  table[0x14] = {OP::NOP, AM::ZPX, OF::NONE}; // NOP zp,X - illegal NOP
  table[0x15] = {OP::ORA, AM::ZPX, OF::NONE};
  table[0x16] = {OP::ASL, AM::ZPX, OF::RMW};
  table[0x17] = {OP::SLO, AM::ZPX,
                 OF::RMW}; // SLO - Shift Left then OR (illegal)
  table[0x18] = {OP::CLC, AM::NON, OF::NONE};
  table[0x19] = {OP::ORA, AM::ABY, OF::SKIP_PAGE};
  table[0x1A] = {OP::NOP, AM::NON, OF::NONE}; // INC A - illegal NOP (1-byte)
  table[0x1B] = {OP::SLO, AM::ABY,
                 OF::RMW}; // SLO - Shift Left then OR (illegal)
  table[0x1C] = {OP::NOP, AM::ABX, OF::SKIP_PAGE}; // NOP abs,X - illegal NOP
  table[0x1D] = {OP::ORA, AM::ABX, OF::SKIP_PAGE};
  table[0x1E] = {OP::ASL, AM::ABX, OF::RMW};
  table[0x1F] = {OP::SLO, AM::ABX,
                 OF::RMW}; // SLO - Shift Left then OR (illegal)
  table[0x20] = {OP::JSR, AM::NON, OF::NONE};
  table[0x21] = {OP::AND, AM::INX, OF::NONE};
  table[0x22] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0x23] = {OP::RLA, AM::INX,
                 OF::RMW}; // RLA - Rotate Left then AND (illegal)
  table[0x24] = {OP::BIT, AM::ZER, OF::NONE};
  table[0x25] = {OP::AND, AM::ZER, OF::NONE};
  table[0x26] = {OP::ROL, AM::ZER, OF::RMW};
  table[0x27] = {OP::RLA, AM::ZER,
                 OF::RMW}; // RLA - Rotate Left then AND (illegal)
  table[0x28] = {OP::PLP, AM::NON, OF::NONE};
  table[0x29] = {OP::AND, AM::IMM, OF::NONE};
  table[0x2A] = {OP::ROL, AM::ACC, OF::NONE};
  table[0x2B] = {OP::ANC, AM::IMM, OF::NONE}; // ANC - AND with carry (illegal)
  table[0x2C] = {OP::BIT, AM::ABS, OF::NONE};
  table[0x2D] = {OP::AND, AM::ABS, OF::NONE};
  table[0x2E] = {OP::ROL, AM::ABS, OF::RMW};
  table[0x2F] = {OP::RLA, AM::ABS,
                 OF::RMW}; // RLA - Rotate Left then AND (illegal)
  table[0x30] = {OP::BMI, AM::REL, OF::NONE};
  table[0x31] = {OP::AND, AM::INY, OF::SKIP_PAGE};
  table[0x32] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0x33] = {OP::RLA, AM::INY,
                 OF::RMW}; // RLA - Rotate Left then AND (illegal)
  table[0x34] = {OP::NOP, AM::ZPX, OF::NONE}; // NOP zp,X - illegal NOP
  table[0x35] = {OP::AND, AM::ZPX, OF::NONE};
  table[0x36] = {OP::ROL, AM::ZPX, OF::RMW};
  table[0x37] = {OP::RLA, AM::ZPX,
                 OF::RMW}; // RLA - Rotate Left then AND (illegal)
  table[0x38] = {OP::SEC, AM::NON, OF::NONE};
  table[0x39] = {OP::AND, AM::ABY, OF::SKIP_PAGE};
  table[0x3A] = {OP::NOP, AM::NON, OF::NONE}; // DEC A - illegal NOP (1-byte)
  table[0x3B] = {OP::RLA, AM::ABY,
                 OF::RMW}; // RLA - Rotate Left then AND (illegal)
  table[0x3C] = {
      OP::NOP, AM::ABX,
      OF::SKIP_PAGE}; // NOP abs,X - illegal NOP (will be overridden for 65C02)
  table[0x3D] = {OP::AND, AM::ABX, OF::SKIP_PAGE};
  table[0x3E] = {OP::ROL, AM::ABX, OF::RMW};
  table[0x3F] = {OP::RLA, AM::ABX,
                 OF::RMW}; // RLA - Rotate Left then AND (illegal)
  table[0x40] = {OP::RTI, AM::NON, OF::NONE};
  table[0x41] = {OP::EOR, AM::INX, OF::NONE};
  table[0x42] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0x43] = {OP::SRE, AM::INX,
                 OF::RMW}; // SRE - Shift Right then EOR (illegal)
  table[0x44] = {OP::NOP, AM::ZER, OF::NONE}; // NOP zp - illegal NOP
  table[0x45] = {OP::EOR, AM::ZER, OF::NONE};
  table[0x46] = {OP::LSR, AM::ZER, OF::RMW};
  table[0x47] = {OP::SRE, AM::ZER,
                 OF::RMW}; // SRE - Shift Right then EOR (illegal)
  table[0x48] = {OP::PHA, AM::NON, OF::NONE};
  table[0x49] = {OP::EOR, AM::IMM, OF::NONE};
  table[0x4A] = {OP::LSR, AM::ACC, OF::NONE};
  table[0x4B] = {OP::ASR, AM::IMM,
                 OF::NONE}; // ASR - AND then LSR (illegal) (also called ALR)
  table[0x4C] = {OP::JMP, AM::ABS, OF::NONE};
  table[0x4D] = {OP::EOR, AM::ABS, OF::NONE};
  table[0x4E] = {OP::LSR, AM::ABS, OF::RMW};
  table[0x4F] = {OP::SRE, AM::ABS,
                 OF::RMW}; // SRE - Shift Right then EOR (illegal)
  table[0x50] = {OP::BVC, AM::REL, OF::NONE};
  table[0x51] = {OP::EOR, AM::INY, OF::SKIP_PAGE};
  table[0x52] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0x53] = {OP::SRE, AM::INY,
                 OF::RMW}; // SRE - Shift Right then EOR (illegal)
  table[0x54] = {OP::NOP, AM::ZPX, OF::NONE}; // NOP zp,X - illegal NOP
  table[0x55] = {OP::EOR, AM::ZPX, OF::NONE};
  table[0x56] = {OP::LSR, AM::ZPX, OF::RMW};
  table[0x57] = {OP::SRE, AM::ZPX,
                 OF::RMW}; // SRE - Shift Right then EOR (illegal)
  table[0x58] = {OP::CLI, AM::NON, OF::NONE};
  table[0x59] = {OP::EOR, AM::ABY, OF::SKIP_PAGE};
  table[0x5A] = {OP::NOP, AM::NON, OF::NONE}; // NOP - illegal NOP
  table[0x5B] = {OP::SRE, AM::ABY,
                 OF::RMW}; // SRE - Shift Right then EOR (illegal)
  table[0x5C] = {OP::NOP, AM::ABX, OF::SKIP_PAGE}; // NOP abs,X - illegal NOP
  table[0x5D] = {OP::EOR, AM::ABX, OF::SKIP_PAGE};
  table[0x5E] = {OP::LSR, AM::ABX, OF::RMW};
  table[0x5F] = {OP::SRE, AM::ABX,
                 OF::RMW}; // SRE - Shift Right then EOR (illegal)
  table[0x60] = {OP::RTS, AM::NON, OF::NONE};
  table[0x61] = {OP::ADC, AM::INX, OF::NONE};
  table[0x62] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0x63] = {OP::RRA, AM::INX,
                 OF::RMW}; // RRA - Rotate Right then ADC (illegal)
  table[0x64] = {
      OP::NOP, AM::ZER,
      OF::NONE}; // NOP zp - illegal NOP (2-byte) - will be overridden for 65C02
  table[0x65] = {OP::ADC, AM::ZER, OF::NONE};
  table[0x66] = {OP::ROR, AM::ZER, OF::RMW};
  table[0x67] = {OP::RRA, AM::ZER,
                 OF::RMW}; // RRA - Rotate Right then ADC (illegal)
  table[0x68] = {OP::PLA, AM::NON, OF::NONE};
  table[0x69] = {OP::ADC, AM::IMM, OF::NONE};
  table[0x6A] = {OP::ROR, AM::ACC, OF::NONE};
  table[0x6B] = {OP::ARR, AM::IMM, OF::NONE}; // ARR - AND then ROR (illegal)
  table[0x6C] = {OP::JMP, AM::IND, OF::NONE};
  table[0x6D] = {OP::ADC, AM::ABS, OF::NONE};
  table[0x6E] = {OP::ROR, AM::ABS, OF::RMW};
  table[0x6F] = {OP::RRA, AM::ABS,
                 OF::RMW}; // RRA - Rotate Right then ADC (illegal)
  table[0x70] = {OP::BVS, AM::REL, OF::NONE};
  table[0x71] = {OP::ADC, AM::INY, OF::SKIP_PAGE};
  table[0x72] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0x73] = {OP::RRA, AM::INY,
                 OF::RMW}; // RRA - Rotate Right then ADC (illegal)
  table[0x74] = {OP::NOP, AM::ZPX, OF::NONE}; // NOP zp,X - illegal NOP
  table[0x75] = {OP::ADC, AM::ZPX, OF::NONE};
  table[0x76] = {OP::ROR, AM::ZPX, OF::RMW};
  table[0x77] = {OP::RRA, AM::ZPX,
                 OF::RMW}; // RRA - Rotate Right then ADC (illegal)
  table[0x78] = {OP::SEI, AM::NON, OF::NONE};
  table[0x79] = {OP::ADC, AM::ABY, OF::SKIP_PAGE};
  table[0x7A] = {OP::NOP, AM::NON, OF::NONE}; // NOP - illegal NOP
  table[0x7B] = {OP::RRA, AM::ABY,
                 OF::RMW}; // RRA - Rotate Right then ADC (illegal)
  table[0x7C] = {OP::NOP, AM::ABX,
                 OF::SKIP_PAGE}; // JMP (abs,X) - illegal NOP (3-byte) - will be
                                 // overridden for 65C02
  table[0x7D] = {OP::ADC, AM::ABX, OF::SKIP_PAGE};
  table[0x7E] = {OP::ROR, AM::ABX, OF::RMW};
  table[0x7F] = {OP::RRA, AM::ABX,
                 OF::RMW}; // RRA - Rotate Right then ADC (illegal)
  table[0x80] = {OP::NOP, AM::IMM, OF::NONE}; // NOP #imm - illegal NOP
  table[0x81] = {OP::STA, AM::INX, OF::NONE};
  table[0x82] = {OP::NOP, AM::IMM, OF::NONE}; // NOP #imm - illegal NOP
  table[0x83] = {OP::SAX, AM::INX, OF::NONE}; // SAX - Store A AND X (illegal)
  table[0x84] = {OP::STY, AM::ZER, OF::NONE};
  table[0x85] = {OP::STA, AM::ZER, OF::NONE};
  table[0x86] = {OP::STX, AM::ZER, OF::NONE};
  table[0x87] = {OP::SAX, AM::ZER, OF::NONE}; // SAX - Store A AND X (illegal)
  table[0x88] = {OP::DEY, AM::NON, OF::NONE};
  table[0x89] = {OP::NOP, AM::IMM, OF::NONE}; // NOP #imm - illegal NOP
  table[0x8A] = {OP::TXA, AM::NON, OF::NONE};
  table[0x8B] = {OP::XAA, AM::IMM,
                 OF::NONE}; // XAA - Transfer X AND imm to A (illegal)
  table[0x8C] = {OP::STY, AM::ABS, OF::NONE};
  table[0x8D] = {OP::STA, AM::ABS, OF::NONE};
  table[0x8E] = {OP::STX, AM::ABS, OF::NONE};
  table[0x8F] = {
      OP::SAX, AM::ABS,
      OF::NONE}; // SAX - Store A AND X (illegal) - will be overridden for CMOS
  table[0x90] = {OP::BCC, AM::REL, OF::NONE};
  table[0x91] = {OP::STA, AM::INY, OF::NONE};
  table[0x92] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0x93] = {
      OP::SHA, AM::INY,
      OF::ILLEGAL_STORE}; // SHA - Store A AND X AND (addr_hi+1) (illegal)
  table[0x94] = {OP::STY, AM::ZPX, OF::NONE};
  table[0x95] = {OP::STA, AM::ZPX, OF::NONE};
  table[0x96] = {OP::STX, AM::ZPY, OF::NONE};
  table[0x97] = {OP::SAX, AM::ZPY, OF::NONE}; // SAX - Store A AND X (illegal)
  table[0x98] = {OP::TYA, AM::NON, OF::NONE};
  table[0x99] = {OP::STA, AM::ABY, OF::NONE};
  table[0x9A] = {OP::TXS, AM::NON, OF::NONE};
  table[0x9B] = {OP::SHS, AM::ABY,
                 OF::ILLEGAL_STORE}; // SHS - Store (A AND X) AND ((addr_hi)+1)
                                     // to S (illegal)
  table[0x9C] = {OP::SHY, AM::ABX,
                 OF::ILLEGAL_STORE}; // SHY - Store Y AND ((addr_hi)+1)
                                     // (illegal) - will be overridden for 65C02
  table[0x9D] = {OP::STA, AM::ABX, OF::NONE};
  table[0x9E] = {OP::SHX, AM::ABY,
                 OF::ILLEGAL_STORE}; // SHX - Store X AND ((addr_hi)+1)
                                     // (illegal) - will be overridden for CMOS
  table[0x9F] = {OP::SHA, AM::ABY,
                 OF::ILLEGAL_STORE}; // SHA - Store A AND X AND (addr_hi+1)
                                     // (illegal) - will be overridden for CMOS
  table[0xA0] = {OP::LDY, AM::IMM, OF::NONE};
  table[0xA1] = {OP::LDA, AM::INX, OF::NONE};
  table[0xA2] = {OP::LDX, AM::IMM, OF::NONE};
  table[0xA3] = {OP::LAX, AM::INX, OF::NONE}; // LAX - Load A and X (illegal)
  table[0xA4] = {OP::LDY, AM::ZER, OF::NONE};
  table[0xA5] = {OP::LDA, AM::ZER, OF::NONE};
  table[0xA6] = {OP::LDX, AM::ZER, OF::NONE};
  table[0xA7] = {OP::LAX, AM::ZER, OF::NONE}; // LAX - Load A and X (illegal)
  table[0xA8] = {OP::TAY, AM::NON, OF::NONE};
  table[0xA9] = {OP::LDA, AM::IMM, OF::NONE};
  table[0xAA] = {OP::TAX, AM::NON, OF::NONE};
  table[0xAB] = {OP::LAX, AM::IMM, OF::NONE}; // LAX - Load A and X (illegal)
  table[0xAC] = {OP::LDY, AM::ABS, OF::NONE};
  table[0xAD] = {OP::LDA, AM::ABS, OF::NONE};
  table[0xAE] = {OP::LDX, AM::ABS, OF::NONE};
  table[0xAF] = {OP::LAX, AM::ABS, OF::NONE}; // LAX - Load A and X (illegal)
  table[0xB0] = {OP::BCS, AM::REL, OF::NONE};
  table[0xB1] = {OP::LDA, AM::INY, OF::SKIP_PAGE};
  table[0xB2] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0xB3] = {OP::LAX, AM::INY,
                 OF::SKIP_PAGE}; // LAX - Load A and X (illegal)
  table[0xB4] = {OP::LDY, AM::ZPX, OF::NONE};
  table[0xB5] = {OP::LDA, AM::ZPX, OF::NONE};
  table[0xB6] = {OP::LDX, AM::ZPY, OF::NONE};
  table[0xB7] = {OP::LAX, AM::ZPY, OF::NONE}; // LAX - Load A and X (illegal)
  table[0xB8] = {OP::CLV, AM::NON, OF::NONE};
  table[0xB9] = {OP::LDA, AM::ABY, OF::SKIP_PAGE};
  table[0xBA] = {OP::TSX, AM::NON, OF::NONE};
  table[0xBB] = {OP::LAS, AM::ABY,
                 OF::SKIP_PAGE}; // LAS - Load A, X, S (illegal)
  table[0xBC] = {OP::LDY, AM::ABX, OF::SKIP_PAGE};
  table[0xBD] = {OP::LDA, AM::ABX, OF::SKIP_PAGE};
  table[0xBE] = {OP::LDX, AM::ABY, OF::SKIP_PAGE};
  table[0xBF] = {OP::LAX, AM::ABY,
                 OF::SKIP_PAGE}; // LAX - Load A and X (illegal)
  table[0xC0] = {OP::CPY, AM::IMM, OF::NONE};
  table[0xC1] = {OP::CMP, AM::INX, OF::NONE};
  table[0xC2] = {OP::NOP, AM::IMM, OF::NONE}; // NOP #imm - illegal NOP
  table[0xC3] = {OP::DCP, AM::INX,
                 OF::RMW}; // DCP - Decrement then Compare (illegal)
  table[0xC4] = {OP::CPY, AM::ZER, OF::NONE};
  table[0xC5] = {OP::CMP, AM::ZER, OF::NONE};
  table[0xC6] = {OP::DEC, AM::ZER, OF::RMW};
  table[0xC7] = {OP::DCP, AM::ZER,
                 OF::RMW}; // DCP - Decrement then Compare (illegal)
  table[0xC8] = {OP::INY, AM::NON, OF::NONE};
  table[0xC9] = {OP::CMP, AM::IMM, OF::NONE};
  table[0xCA] = {OP::DEX, AM::NON, OF::NONE};
  table[0xCB] = {
      OP::SBX, AM::IMM,
      OF::NONE}; // SBX - Compare X with A AND imm (illegal) (also called AXS)
  table[0xCC] = {OP::CPY, AM::ABS, OF::NONE};
  table[0xCD] = {OP::CMP, AM::ABS, OF::NONE};
  table[0xCE] = {OP::DEC, AM::ABS, OF::RMW};
  table[0xCF] = {OP::DCP, AM::ABS,
                 OF::RMW}; // DCP - Decrement then Compare (illegal)
  table[0xD0] = {OP::BNE, AM::REL, OF::NONE};
  table[0xD1] = {OP::CMP, AM::INY, OF::SKIP_PAGE};
  table[0xD2] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0xD3] = {OP::DCP, AM::INY,
                 OF::RMW}; // DCP - Decrement then Compare (illegal)
  table[0xD4] = {OP::NOP, AM::ZPX, OF::NONE}; // NOP zp,X - illegal NOP
  table[0xD5] = {OP::CMP, AM::ZPX, OF::NONE};
  table[0xD6] = {OP::DEC, AM::ZPX, OF::RMW};
  table[0xD7] = {OP::DCP, AM::ZPX,
                 OF::RMW}; // DCP - Decrement then Compare (illegal)
  table[0xD8] = {OP::CLD, AM::NON, OF::NONE};
  table[0xD9] = {OP::CMP, AM::ABY, OF::SKIP_PAGE};
  table[0xDA] = {OP::NOP, AM::NON, OF::NONE}; // NOP - illegal NOP
  table[0xDB] = {OP::DCP, AM::ABY,
                 OF::RMW}; // DCP - Decrement then Compare (illegal)
  table[0xDC] = {OP::NOP, AM::ABX, OF::SKIP_PAGE}; // NOP abs,X - illegal NOP
  table[0xDD] = {OP::CMP, AM::ABX, OF::SKIP_PAGE};
  table[0xDE] = {OP::DEC, AM::ABX, OF::RMW};
  table[0xDF] = {OP::DCP, AM::ABX,
                 OF::RMW}; // DCP - Decrement then Compare (illegal)
  table[0xE0] = {OP::CPX, AM::IMM, OF::NONE};
  table[0xE1] = {OP::SBC, AM::INX, OF::NONE};
  table[0xE2] = {OP::NOP, AM::IMM, OF::NONE}; // NOP #imm - illegal NOP
  table[0xE3] = {OP::ISC, AM::INX,
                 OF::RMW}; // ISC - Increment then SBC (illegal)
  table[0xE4] = {OP::CPX, AM::ZER, OF::NONE};
  table[0xE5] = {OP::SBC, AM::ZER, OF::NONE};
  table[0xE6] = {OP::INC, AM::ZER, OF::RMW};
  table[0xE7] = {OP::ISC, AM::ZER,
                 OF::RMW}; // ISC - Increment then SBC (illegal)
  table[0xE8] = {OP::INX, AM::NON, OF::NONE};
  table[0xE9] = {OP::SBC, AM::IMM, OF::NONE};
  table[0xEA] = {OP::NOP, AM::NON, OF::NONE};
  table[0xEB] = {OP::SBC, AM::IMM, OF::NONE}; // SBC #imm - illegal SBC
  table[0xEC] = {OP::CPX, AM::ABS, OF::NONE};
  table[0xED] = {OP::SBC, AM::ABS, OF::NONE};
  table[0xEE] = {OP::INC, AM::ABS, OF::RMW};
  table[0xEF] = {OP::ISC, AM::ABS,
                 OF::RMW}; // ISC - Increment then SBC (illegal)
  table[0xF0] = {OP::BEQ, AM::REL, OF::NONE};
  table[0xF1] = {OP::SBC, AM::INY, OF::SKIP_PAGE};
  table[0xF2] = {OP::JAM, AM::NON, OF::NONE}; // KIL/JAM - illegal opcode
  table[0xF3] = {OP::ISC, AM::INY,
                 OF::RMW}; // ISC - Increment then SBC (illegal)
  table[0xF4] = {OP::NOP, AM::ZPX, OF::NONE}; // NOP zp,X - illegal NOP
  table[0xF5] = {OP::SBC, AM::ZPX, OF::NONE};
  table[0xF6] = {OP::INC, AM::ZPX, OF::RMW};
  table[0xF7] = {OP::ISC, AM::ZPX,
                 OF::RMW}; // ISC - Increment then SBC (illegal)
  table[0xF8] = {OP::SED, AM::NON, OF::NONE};
  table[0xF9] = {OP::SBC, AM::ABY, OF::SKIP_PAGE};
  table[0xFA] = {OP::NOP, AM::NON, OF::NONE}; // NOP - illegal NOP
  table[0xFB] = {OP::ISC, AM::ABY,
                 OF::RMW}; // ISC - Increment then SBC (illegal)
  table[0xFC] = {OP::NOP, AM::ABX, OF::SKIP_PAGE}; // NOP abs,X - illegal NOP
  table[0xFD] = {OP::SBC, AM::ABX, OF::SKIP_PAGE};
  table[0xFE] = {OP::INC, AM::ABX, OF::RMW};
  table[0xFF] = {OP::ISC, AM::ABX,
                 OF::RMW}; // ISC - Increment then SBC (illegal)

  // Now apply processor-specific modifications based on CPUTraits

  // Synertek 65C02 specific overrides - limited CMOS processor (MUST come
  // before general CMOS)
  if (traits.has(fam65xx::CPUCoreFlags::CMOS_BASE) &&
      !traits.has(fam65xx::CPUCoreFlags::WAI_STP) &&
      !traits.has(fam65xx::CPUCoreFlags::ROCKWELL_BITS)) {
    // Synertek 65C02 supports basic accumulator increment/decrement but not
    // advanced CMOS instructions
    table[0x1A] = {OP::INC, AM::ACC, OF::NONE}; // INC A - Increment Accumulator
                                                // (supported on Synertek 65C02)
    table[0x3A] = {OP::DEC, AM::ACC, OF::NONE}; // DEC A - Decrement Accumulator
                                                // (supported on Synertek 65C02)
    table[0x64] = {
        OP::NOP, AM::ZER,
        OF::NONE}; // STZ zp -> 2-byte NOP (zero page) - not supported
    // Note: 0x7C (JMP abs,X) is supported on Synertek 65C02 - handled by CMOS section
    table[0x9C] = {
        OP::NOP, AM::ABS,
        OF::NONE}; // STZ abs -> 3-byte NOP (absolute) - not supported
    // WAI/STP will be handled by post-CMOS override section to avoid conflicts
  }

  // CMOS processors: Replace illegal opcodes with NOPs
  if (traits.has(fam65xx::CPUCoreFlags::CMOS_BASE)) {
    // WDC65C02: Specific illegal opcodes become 2-byte NOPs (AM::IMM)
    table[0x02] = {OP::NOP, AM::IMM, OF::NONE}; // JAM -> 2-byte NOP
    table[0x22] = {OP::NOP, AM::IMM, OF::NONE}; // JAM -> 2-byte NOP
    table[0x42] = {OP::NOP, AM::IMM, OF::NONE}; // JAM -> 2-byte NOP
    table[0x62] = {OP::NOP, AM::IMM, OF::NONE}; // JAM -> 2-byte NOP
    table[0x82] = {OP::NOP, AM::IMM, OF::NONE}; // NOP #imm -> 2-byte NOP
    table[0xC2] = {OP::NOP, AM::IMM, OF::NONE}; // NOP #imm -> 2-byte NOP
    table[0xE2] = {OP::NOP, AM::IMM, OF::NONE}; // NOP #imm -> 2-byte NOP

    // Replace ALL other illegal opcodes with appropriate NOPs for Synertek
    // 65C02 compatibility Based on test failures, many need to be 2-byte NOPs
    // (AM::IMM) to match expected PC advancement
    table[0x03] = {OP::NOP, AM::NON, OF::NONE}; // SLO -> NOP
    table[0x07] = {OP::NOP, AM::IMM, OF::NONE}; // SLO -> 2-byte NOP
    table[0x0B] = {OP::NOP, AM::NON, OF::NONE}; // ANC -> NOP
    table[0x0F] = {OP::NOP, AM::ABS, OF::NONE}; // SLO -> 3-byte NOP (absolute) - will be overridden by Rockwell section
    table[0x12] = {OP::ORA, AM::ZPI,
                   OF::NONE}; // ORA ($nn) - ORA zero page indirect (65C02)
    table[0x13] = {OP::NOP, AM::NON, OF::NONE}; // SLO -> NOP
    table[0x17] = {OP::NOP, AM::IMM, OF::NONE}; // SLO -> 2-byte NOP
    table[0x1B] = {OP::NOP, AM::NON, OF::NONE}; // SLO -> NOP
    table[0x1F] = {OP::NOP, AM::ABS,
                   OF::NONE}; // SLO -> 3-byte NOP (absolute,X)
    table[0x23] = {OP::NOP, AM::NON, OF::NONE}; // RLA -> NOP
    table[0x27] = {OP::NOP, AM::IMM, OF::NONE}; // RLA -> 2-byte NOP
    table[0x2B] = {OP::NOP, AM::NON, OF::NONE}; // ANC -> NOP
    table[0x2F] = {OP::NOP, AM::ABS,
                   OF::NONE}; // RLA -> NOP (special case: must match
                              // ProcessorTests expectations)
    table[0x32] = {OP::AND, AM::ZPI,
                   OF::NONE}; // AND ($nn) - AND zero page indirect (65C02)
    table[0x33] = {OP::NOP, AM::NON, OF::NONE}; // RLA -> NOP
    table[0x34] = {OP::BIT, AM::ZPX,
                   OF::NONE}; // BIT zp,X - BIT zero page,X (65C02)
    table[0x3C] = {OP::BIT, AM::ABX,
                   OF::SKIP_PAGE}; // BIT abs,X - BIT absolute,X (65C02)
    table[0x37] = {OP::NOP, AM::IMM, OF::NONE}; // RLA -> 2-byte NOP
    table[0x3B] = {OP::NOP, AM::NON, OF::NONE}; // RLA -> NOP
    table[0x3F] = {OP::NOP, AM::ABS,
                   OF::NONE}; // RLA -> 3-byte NOP (absolute,X)
    table[0x43] = {OP::NOP, AM::NON, OF::NONE}; // SRE -> NOP
    table[0x47] = {OP::NOP, AM::IMM, OF::NONE}; // SRE -> 2-byte NOP
    table[0x4B] = {OP::NOP, AM::NON, OF::NONE}; // ASR -> NOP
    table[0x4F] = {OP::NOP, AM::ABS, OF::NONE}; // SRE -> 3-byte NOP (absolute)
    table[0x52] = {OP::EOR, AM::ZPI,
                   OF::NONE}; // EOR ($nn) - EOR zero page indirect (65C02)
    table[0x53] = {OP::NOP, AM::NON, OF::NONE}; // SRE -> NOP
    table[0x57] = {OP::NOP, AM::IMM, OF::NONE}; // SRE -> 2-byte NOP
    table[0x5B] = {OP::NOP, AM::NON, OF::NONE}; // SRE -> NOP
    table[0x5F] = {OP::NOP, AM::ABS,
                   OF::NONE}; // SRE -> 3-byte NOP (absolute,X)
    table[0x63] = {OP::NOP, AM::NON, OF::NONE}; // RRA -> NOP
    table[0x67] = {OP::NOP, AM::IMM, OF::NONE}; // RRA -> 2-byte NOP
    table[0x6B] = {OP::NOP, AM::NON, OF::NONE}; // ARR -> NOP
    table[0x6F] = {OP::NOP, AM::ABS, OF::NONE}; // RRA -> 3-byte NOP (absolute)
    table[0x72] = {
        OP::ADC, AM::ZPI,
        OF::NONE}; // ADC ($nn) - Add with Carry zero page indirect (65C02)
    table[0x73] = {OP::NOP, AM::NON, OF::NONE}; // RRA -> NOP
    table[0x77] = {OP::NOP, AM::IMM, OF::NONE}; // RRA -> 2-byte NOP
    table[0x7B] = {OP::NOP, AM::NON, OF::NONE}; // RRA -> NOP
    table[0x7C] = {
        OP::JMP, AM::ABI,
        OF::NONE}; // JMP (abs,X) - JMP absolute indexed indirect (ALL 65C02)
    table[0x7F] = {OP::NOP, AM::ABS,
                   OF::NONE}; // RRA -> 3-byte NOP (absolute,X)
    table[0x83] = {OP::NOP, AM::NON, OF::NONE}; // SAX -> NOP
    table[0x87] = {OP::NOP, AM::IMM, OF::NONE}; // SAX -> 2-byte NOP
    table[0x89] = {OP::BIT, AM::IMM,
                   OF::NONE}; // BIT #imm - BIT immediate (ALL 65C02)
    table[0x8B] = {OP::NOP, AM::NON, OF::NONE}; // XAA -> NOP
    table[0x8F] = {OP::NOP, AM::ABS,
                   OF::NONE}; // SAX -> 3-byte NOP (absolute addressing)
    table[0x92] = {OP::STA, AM::ZPI,
                   OF::NONE}; // STA ($nn) - Store A zero page indirect (65C02)
    table[0x93] = {OP::NOP, AM::NON, OF::NONE}; // SHA -> NOP
    table[0x97] = {OP::NOP, AM::IMM, OF::NONE}; // SAX -> 2-byte NOP
    table[0x9B] = {OP::NOP, AM::NON, OF::NONE}; // SHS -> NOP
    table[0x9C] = {
        OP::NOP, AM::ABS,
        OF::NONE}; // SHY -> 3-byte NOP (will be overridden for 65C02)
    table[0x9E] = {OP::NOP, AM::ABX,
                   OF::NONE}; // SHX -> 3-byte NOP (absolute,X addressing)
    table[0x9F] = {OP::NOP, AM::ABY,
                   OF::NONE}; // SHA -> 3-byte NOP (absolute,Y addressing)
    table[0xA3] = {OP::NOP, AM::NON, OF::NONE}; // LAX -> NOP
    table[0xA7] = {OP::NOP, AM::IMM, OF::NONE}; // LAX -> 2-byte NOP
    table[0xAB] = {OP::NOP, AM::NON, OF::NONE}; // LAX -> NOP
    table[0xAF] = {OP::NOP, AM::ABS,
                   OF::NONE}; // LAX -> 3-byte NOP (absolute addressing)
    table[0xB2] = {OP::LDA, AM::ZPI,
                   OF::NONE}; // LDA ($nn) - Load A zero page indirect (65C02)
    table[0xB3] = {OP::NOP, AM::NON, OF::NONE}; // LAX -> NOP
    table[0xB7] = {OP::NOP, AM::IMM, OF::NONE}; // LAX -> 2-byte NOP
    table[0xBB] = {OP::NOP, AM::NON, OF::NONE}; // LAS -> NOP
    table[0xBF] = {OP::NOP, AM::ABY,
                   OF::NONE}; // LAX -> 3-byte NOP (absolute,Y addressing)
    table[0xC3] = {OP::NOP, AM::NON, OF::NONE}; // DCP -> NOP
    table[0xC7] = {OP::NOP, AM::IMM, OF::NONE}; // DCP -> 2-byte NOP
    table[0xCB] = {OP::NOP, AM::NON,
                   OF::NONE}; // SBX -> NOP (will be overridden for 65C02)
    table[0xCF] = {OP::NOP, AM::ABS,
                   OF::NONE}; // DCP -> 3-byte NOP (absolute addressing)
    table[0xD2] = {
        OP::CMP, AM::ZPI,
        OF::NONE}; // CMP ($nn) - Compare A zero page indirect (65C02)
    table[0xD3] = {OP::NOP, AM::NON, OF::NONE}; // DCP -> NOP
    table[0xD7] = {OP::NOP, AM::IMM, OF::NONE}; // DCP -> 2-byte NOP
    table[0xDA] = {OP::NOP, AM::NON,
                   OF::NONE}; // NOP -> NOP (will be overridden for 65C02)
    table[0xDB] = {OP::NOP, AM::ABY,
                   OF::NONE}; // DCP -> 3-byte NOP (absolute,Y addressing) (will
                              // be overridden for 65C02)
    table[0xDF] = {OP::NOP, AM::ABX,
                   OF::NONE}; // DCP -> 3-byte NOP (absolute,X addressing)
    table[0xE3] = {OP::NOP, AM::NON, OF::NONE}; // ISC -> NOP
    table[0xE7] = {OP::NOP, AM::IMM, OF::NONE}; // ISC -> 2-byte NOP
    table[0xEB] = {OP::NOP, AM::NON, OF::NONE}; // SBC -> NOP
    table[0xEF] = {OP::NOP, AM::ABS,
                   OF::NONE}; // ISC -> 3-byte NOP (absolute addressing)
    table[0xF2] = {
        OP::SBC, AM::ZPI,
        OF::NONE}; // SBC ($nn) - Subtract with Carry zero page indirect (65C02)
    table[0xF3] = {OP::NOP, AM::NON, OF::NONE}; // ISC -> NOP
    table[0xF7] = {OP::NOP, AM::IMM, OF::NONE}; // ISC -> 2-byte NOP
    table[0xFA] = {OP::NOP, AM::NON,
                   OF::NONE}; // NOP -> NOP (will be overridden for 65C02)
    table[0xFB] = {OP::NOP, AM::NON, OF::NONE}; // ISC -> NOP
    table[0xFF] = {OP::NOP, AM::ABX,
                   OF::NONE}; // ISC -> 3-byte NOP (absolute,X addressing)

    // Add 65C02 enhancements - ALL CMOS processors support these accumulator
    // instructions
    table[0x1A] = {
        OP::INC, AM::ACC,
        OF::NONE}; // INC A - Increment Accumulator (all 65C02 variants)
    table[0x3A] = {
        OP::DEC, AM::ACC,
        OF::NONE}; // DEC A - Decrement Accumulator (all 65C02 variants)
    table[0x04] = {OP::TSB, AM::ZER, OF::RMW};  // TSB zero page
    table[0x0C] = {OP::TSB, AM::ABS, OF::RMW};  // TSB absolute
    table[0x14] = {OP::TRB, AM::ZER, OF::RMW};  // TRB zero page
    table[0x1C] = {OP::TRB, AM::ABS, OF::RMW};  // TRB absolute
    table[0x5A] = {OP::PHY, AM::NON, OF::NONE}; // PHY
    table[0x64] = {OP::STZ, AM::ZER, OF::NONE}; // STZ zero page
    table[0x74] = {OP::STZ, AM::ZPX, OF::NONE}; // STZ zero page,X
    table[0x7A] = {OP::PLY, AM::NON, OF::NONE}; // PLY
    table[0x80] = {OP::BRA, AM::REL, OF::NONE}; // BRA
    table[0x9C] = {OP::STZ, AM::ABS, OF::NONE}; // STZ absolute
    table[0x9E] = {OP::STZ, AM::ABX, OF::NONE}; // STZ absolute,X
    table[0xCB] = {OP::WAI, AM::NON, OF::NONE}; // WAI
    table[0xDA] = {OP::PHX, AM::NON, OF::NONE}; // PHX
    table[0xDB] = {OP::STP, AM::IMM, OF::NONE}; // STP (2-byte instruction)
    table[0xFA] = {OP::PLX, AM::NON, OF::NONE}; // PLX

    // Synertek 65C02 post-CMOS overrides - must come after general CMOS
    // settings
    if (traits.has(fam65xx::CPUCoreFlags::CMOS_BASE) &&
        !traits.has(fam65xx::CPUCoreFlags::WAI_STP) &&
        !traits.has(fam65xx::CPUCoreFlags::ROCKWELL_BITS)) {
      // Synertek 65C02 doesn't support WAI/STP - override with proper NOPs
      table[0xCB] = {OP::NOP, AM::NON,
                     OF::NONE}; // WAI -> 1-byte NOP (implied) - halt
                                // immediately on Synertek 65C02
      table[0xDB] = {OP::NOP, AM::IMM,
                     OF::NONE}; // STP -> 2-byte NOP (immediate) - not supported
                                // on Synertek 65C02
    }
  }

  // Rockwell 65C02 modifications (add RMB/SMB/BBR/BBS instructions)
  if (traits.has(fam65xx::CPUCoreFlags::ROCKWELL_BITS)) {
    // Add Rockwell bit manipulation instructions (RMB/SMB)
    table[0x07] = {OP::RMB0, AM::ZER, OF::RMW}; // RMB0
    table[0x17] = {OP::RMB1, AM::ZER, OF::RMW}; // RMB1
    table[0x27] = {OP::RMB2, AM::ZER, OF::RMW}; // RMB2
    table[0x37] = {OP::RMB3, AM::ZER, OF::RMW}; // RMB3
    table[0x47] = {OP::RMB4, AM::ZER, OF::RMW}; // RMB4
    table[0x57] = {OP::RMB5, AM::ZER, OF::RMW}; // RMB5
    table[0x67] = {OP::RMB6, AM::ZER, OF::RMW}; // RMB6
    table[0x77] = {OP::RMB7, AM::ZER, OF::RMW}; // RMB7
    table[0x87] = {OP::SMB0, AM::ZER, OF::RMW}; // SMB0
    table[0x97] = {OP::SMB1, AM::ZER, OF::RMW}; // SMB1
    table[0xA7] = {OP::SMB2, AM::ZER, OF::RMW}; // SMB2
    table[0xB7] = {OP::SMB3, AM::ZER, OF::RMW}; // SMB3
    table[0xC7] = {OP::SMB4, AM::ZER, OF::RMW}; // SMB4
    table[0xD7] = {OP::SMB5, AM::ZER, OF::RMW}; // SMB5
    table[0xE7] = {OP::SMB6, AM::ZER, OF::RMW}; // SMB6
    table[0xF7] = {OP::SMB7, AM::ZER, OF::RMW}; // SMB7

    // BBR/BBS instructions (branch on bit reset/set) - use special ZPR
    // addressing mode
    table[0x0F] = {OP::BBR0, AM::ZPR, OF::NONE}; // BBR0
    table[0x1F] = {OP::BBR1, AM::ZPR, OF::NONE}; // BBR1
    table[0x2F] = {OP::BBR2, AM::ZPR, OF::NONE}; // BBR2
    table[0x3F] = {OP::BBR3, AM::ZPR, OF::NONE}; // BBR3
    table[0x4F] = {OP::BBR4, AM::ZPR, OF::NONE}; // BBR4
    table[0x5F] = {OP::BBR5, AM::ZPR, OF::NONE}; // BBR5
    table[0x6F] = {OP::BBR6, AM::ZPR, OF::NONE}; // BBR6
    table[0x7F] = {OP::BBR7, AM::ZPR, OF::NONE}; // BBR7
    table[0x8F] = {OP::BBS0, AM::ZPR, OF::NONE}; // BBS0
    table[0x9F] = {OP::BBS1, AM::ZPR, OF::NONE}; // BBS1
    table[0xAF] = {OP::BBS2, AM::ZPR, OF::NONE}; // BBS2
    table[0xBF] = {OP::BBS3, AM::ZPR, OF::NONE}; // BBS3
    table[0xCF] = {OP::BBS4, AM::ZPR, OF::NONE}; // BBS4
    table[0xDF] = {OP::BBS5, AM::ZPR, OF::NONE}; // BBS5
    table[0xEF] = {OP::BBS6, AM::ZPR, OF::NONE}; // BBS6
    table[0xFF] = {OP::BBS7, AM::ZPR, OF::NONE}; // BBS7
  }

  // WDC 65C816 modifications (16-bit enhanced instructions)
  if (traits.has(fam65xx::CPUCoreFlags::C816_16BIT)) {
    // Mode Control Instructions
    table[0xC2] = {OP::REP, AM::IMM,
                   OF::NONE}; // REP - Reset Processor Status Bits
    table[0xE2] = {OP::SEP, AM::IMM,
                   OF::NONE}; // SEP - Set Processor Status Bits
    table[0xFB] = {OP::XCE, AM::NON,
                   OF::NONE}; // XCE - Exchange Carry and Emulation

    // Enhanced Stack Operations
    table[0x0B] = {OP::PHD, AM::NON,
                   OF::NONE}; // PHD - Push Direct Page Register
    table[0x2B] = {OP::PLD, AM::NON,
                   OF::NONE}; // PLD - Pull Direct Page Register
    table[0x4B] = {OP::PHK, AM::NON,
                   OF::NONE}; // PHK - Push Program Bank Register
    table[0x62] = {OP::PER, AM::REL,
                   OF::NONE}; // PER - Push Effective Relative Address
    table[0x8B] = {OP::PHB, AM::NON, OF::NONE}; // PHB - Push Data Bank Register
    table[0xAB] = {OP::PLB, AM::NON, OF::NONE}; // PLB - Pull Data Bank Register
    table[0xD4] = {OP::PEI, AM::DPI,
                   OF::NONE}; // PEI - Push Effective Indirect Address (Direct
                              // Page Indirect)
    table[0xF4] = {OP::PEA, AM::ABS,
                   OF::NONE}; // PEA - Push Effective Absolute Address

    // Long Addressing Operations
    table[0x22] = {OP::JSL, AM::ABL,
                   OF::NONE}; // JSL - Jump to Subroutine Long (24-bit)
    table[0x5C] = {OP::JML, AM::ABL,
                   OF::NONE}; // JML - Jump Long (24-bit absolute)
    table[0x6B] = {OP::RTL, AM::NON,
                   OF::NONE}; // RTL - Return from Subroutine Long
    table[0xDC] = {OP::JML, AM::ABI,
                   OF::NONE}; // JML - Jump Long (absolute indexed indirect)

    // Data Transfer Operations
    table[0x44] = {OP::MVN, AM::IMM,
                   OF::NONE}; // MVN - Move Negative (uses 2 immediate bytes for
                              // src/dst banks)
    table[0x54] = {OP::MVP, AM::IMM,
                   OF::NONE}; // MVP - Move Positive (uses 2 immediate bytes for
                              // src/dst banks)
    table[0xEB] = {OP::XBA, AM::NON, OF::NONE}; // XBA - Exchange B and A

    // System Operations
    table[0x02] = {OP::COP, AM::IMM,
                   OF::NONE}; // COP - Co-processor Instruction
    table[0x42] = {OP::WDM, AM::IMM,
                   OF::NONE}; // WDM - WDM Reserved Instruction

    // Note: 65C816-specific addressing modes (ABL, ABLX, DPIL, DPILY, SR, SRIY)
    // will be used by existing operations (LDA, STA, etc.) based on opcode
    // mapping These are handled by the standard operation table entries with
    // different addressing modes
  }

  return table;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION
