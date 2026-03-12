#pragma once
/*
 * z80_opcode_tables.inc.hpp — Z80 Opcode Table Generation
 *
 * Constexpr opcode table generation for all Z80 opcode pages:
 *   - Base (unprefixed, 256 entries)
 *   - CB prefix (bit/rotate/shift, 256 entries)
 *   - ED prefix (extended instructions, 256 entries)
 *   - DD/FD prefix (IX/IY variants — reuse base table with displacement)
 *
 * Included by z80.hpp inside the z80 namespace.
 * Uses the OpcodeInfo struct from z80_opcodes.h.
 */

#include "chip/cpu/z80/z80_opcodes.h"
#include <array>

namespace z80 {

// Shorthand aliases for table construction
using O  = Op;
using AM = AddrMode;

// ============================================================================
// Base (unprefixed) opcode table — 256 entries
// ============================================================================

inline constexpr std::array<OpcodeInfo, 256> generate_base_opcode_table() {
    std::array<OpcodeInfo, 256> t{};

    // Row 0x00
    t[0x00] = {O::NOP,    AM::IMP,     4, 1}; // NOP
    t[0x01] = {O::LD,     AM::IMM16,  10, 3}; // LD BC,nn
    t[0x02] = {O::LD,     AM::IND_BC,  7, 1}; // LD (BC),A
    t[0x03] = {O::INC16,  AM::IMP,     6, 1}; // INC BC
    t[0x04] = {O::INC,    AM::IMP,     4, 1}; // INC B
    t[0x05] = {O::DEC,    AM::IMP,     4, 1}; // DEC B
    t[0x06] = {O::LD,     AM::IMM8,    7, 2}; // LD B,n
    t[0x07] = {O::RLCA,   AM::IMP,     4, 1}; // RLCA
    t[0x08] = {O::EX,     AM::IMP,     4, 1}; // EX AF,AF'
    t[0x09] = {O::ADD16,  AM::IMP,    11, 1}; // ADD HL,BC
    t[0x0A] = {O::LD,     AM::IND_BC,  7, 1}; // LD A,(BC)
    t[0x0B] = {O::DEC16,  AM::IMP,     6, 1}; // DEC BC
    t[0x0C] = {O::INC,    AM::IMP,     4, 1}; // INC C
    t[0x0D] = {O::DEC,    AM::IMP,     4, 1}; // DEC C
    t[0x0E] = {O::LD,     AM::IMM8,    7, 2}; // LD C,n
    t[0x0F] = {O::RRCA,   AM::IMP,     4, 1}; // RRCA

    // Row 0x10
    t[0x10] = {O::DJNZ,   AM::REL8,   13, 2}; // DJNZ e (13/8)
    t[0x11] = {O::LD,     AM::IMM16,  10, 3}; // LD DE,nn
    t[0x12] = {O::LD,     AM::IND_DE,  7, 1}; // LD (DE),A
    t[0x13] = {O::INC16,  AM::IMP,     6, 1}; // INC DE
    t[0x14] = {O::INC,    AM::IMP,     4, 1}; // INC D
    t[0x15] = {O::DEC,    AM::IMP,     4, 1}; // DEC D
    t[0x16] = {O::LD,     AM::IMM8,    7, 2}; // LD D,n
    t[0x17] = {O::RLA,    AM::IMP,     4, 1}; // RLA
    t[0x18] = {O::JR,     AM::REL8,   12, 2}; // JR e
    t[0x19] = {O::ADD16,  AM::IMP,    11, 1}; // ADD HL,DE
    t[0x1A] = {O::LD,     AM::IND_DE,  7, 1}; // LD A,(DE)
    t[0x1B] = {O::DEC16,  AM::IMP,     6, 1}; // DEC DE
    t[0x1C] = {O::INC,    AM::IMP,     4, 1}; // INC E
    t[0x1D] = {O::DEC,    AM::IMP,     4, 1}; // DEC E
    t[0x1E] = {O::LD,     AM::IMM8,    7, 2}; // LD E,n
    t[0x1F] = {O::RRA,    AM::IMP,     4, 1}; // RRA

    // Row 0x20
    t[0x20] = {O::JR,     AM::REL8,   12, 2}; // JR NZ,e (12/7)
    t[0x21] = {O::LD,     AM::IMM16,  10, 3}; // LD HL,nn
    t[0x22] = {O::LD,     AM::IND_ABS,16, 3}; // LD (nn),HL
    t[0x23] = {O::INC16,  AM::IMP,     6, 1}; // INC HL
    t[0x24] = {O::INC,    AM::IMP,     4, 1}; // INC H
    t[0x25] = {O::DEC,    AM::IMP,     4, 1}; // DEC H
    t[0x26] = {O::LD,     AM::IMM8,    7, 2}; // LD H,n
    t[0x27] = {O::DAA,    AM::IMP,     4, 1}; // DAA
    t[0x28] = {O::JR,     AM::REL8,   12, 2}; // JR Z,e (12/7)
    t[0x29] = {O::ADD16,  AM::IMP,    11, 1}; // ADD HL,HL
    t[0x2A] = {O::LD,     AM::IND_ABS,16, 3}; // LD HL,(nn)
    t[0x2B] = {O::DEC16,  AM::IMP,     6, 1}; // DEC HL
    t[0x2C] = {O::INC,    AM::IMP,     4, 1}; // INC L
    t[0x2D] = {O::DEC,    AM::IMP,     4, 1}; // DEC L
    t[0x2E] = {O::LD,     AM::IMM8,    7, 2}; // LD L,n
    t[0x2F] = {O::CPL,    AM::IMP,     4, 1}; // CPL

    // Row 0x30
    t[0x30] = {O::JR,     AM::REL8,   12, 2}; // JR NC,e (12/7)
    t[0x31] = {O::LD,     AM::IMM16,  10, 3}; // LD SP,nn
    t[0x32] = {O::LD,     AM::IND_ABS,13, 3}; // LD (nn),A
    t[0x33] = {O::INC16,  AM::IMP,     6, 1}; // INC SP
    t[0x34] = {O::INC,    AM::IND_HL, 11, 1}; // INC (HL)
    t[0x35] = {O::DEC,    AM::IND_HL, 11, 1}; // DEC (HL)
    t[0x36] = {O::LD,     AM::IMM8,   10, 2}; // LD (HL),n
    t[0x37] = {O::SCF,    AM::IMP,     4, 1}; // SCF
    t[0x38] = {O::JR,     AM::REL8,   12, 2}; // JR C,e (12/7)
    t[0x39] = {O::ADD16,  AM::IMP,    11, 1}; // ADD HL,SP
    t[0x3A] = {O::LD,     AM::IND_ABS,13, 3}; // LD A,(nn)
    t[0x3B] = {O::DEC16,  AM::IMP,     6, 1}; // DEC SP
    t[0x3C] = {O::INC,    AM::IMP,     4, 1}; // INC A
    t[0x3D] = {O::DEC,    AM::IMP,     4, 1}; // DEC A
    t[0x3E] = {O::LD,     AM::IMM8,    7, 2}; // LD A,n
    t[0x3F] = {O::CCF,    AM::IMP,     4, 1}; // CCF

    // Rows 0x40-0x7F: LD r,r' and HALT
    // 0x40-0x47: LD B,r
    t[0x40] = {O::LD,     AM::IMP,     4, 1}; // LD B,B
    t[0x41] = {O::LD,     AM::IMP,     4, 1}; // LD B,C
    t[0x42] = {O::LD,     AM::IMP,     4, 1}; // LD B,D
    t[0x43] = {O::LD,     AM::IMP,     4, 1}; // LD B,E
    t[0x44] = {O::LD,     AM::IMP,     4, 1}; // LD B,H
    t[0x45] = {O::LD,     AM::IMP,     4, 1}; // LD B,L
    t[0x46] = {O::LD,     AM::IND_HL,  7, 1}; // LD B,(HL)
    t[0x47] = {O::LD,     AM::IMP,     4, 1}; // LD B,A
    // 0x48-0x4F: LD C,r
    t[0x48] = {O::LD,     AM::IMP,     4, 1}; // LD C,B
    t[0x49] = {O::LD,     AM::IMP,     4, 1}; // LD C,C
    t[0x4A] = {O::LD,     AM::IMP,     4, 1}; // LD C,D
    t[0x4B] = {O::LD,     AM::IMP,     4, 1}; // LD C,E
    t[0x4C] = {O::LD,     AM::IMP,     4, 1}; // LD C,H
    t[0x4D] = {O::LD,     AM::IMP,     4, 1}; // LD C,L
    t[0x4E] = {O::LD,     AM::IND_HL,  7, 1}; // LD C,(HL)
    t[0x4F] = {O::LD,     AM::IMP,     4, 1}; // LD C,A
    // 0x50-0x57: LD D,r
    t[0x50] = {O::LD,     AM::IMP,     4, 1}; // LD D,B
    t[0x51] = {O::LD,     AM::IMP,     4, 1}; // LD D,C
    t[0x52] = {O::LD,     AM::IMP,     4, 1}; // LD D,D
    t[0x53] = {O::LD,     AM::IMP,     4, 1}; // LD D,E
    t[0x54] = {O::LD,     AM::IMP,     4, 1}; // LD D,H
    t[0x55] = {O::LD,     AM::IMP,     4, 1}; // LD D,L
    t[0x56] = {O::LD,     AM::IND_HL,  7, 1}; // LD D,(HL)
    t[0x57] = {O::LD,     AM::IMP,     4, 1}; // LD D,A
    // 0x58-0x5F: LD E,r
    t[0x58] = {O::LD,     AM::IMP,     4, 1}; // LD E,B
    t[0x59] = {O::LD,     AM::IMP,     4, 1}; // LD E,C
    t[0x5A] = {O::LD,     AM::IMP,     4, 1}; // LD E,D
    t[0x5B] = {O::LD,     AM::IMP,     4, 1}; // LD E,E
    t[0x5C] = {O::LD,     AM::IMP,     4, 1}; // LD E,H
    t[0x5D] = {O::LD,     AM::IMP,     4, 1}; // LD E,L
    t[0x5E] = {O::LD,     AM::IND_HL,  7, 1}; // LD E,(HL)
    t[0x5F] = {O::LD,     AM::IMP,     4, 1}; // LD E,A
    // 0x60-0x67: LD H,r
    t[0x60] = {O::LD,     AM::IMP,     4, 1}; // LD H,B
    t[0x61] = {O::LD,     AM::IMP,     4, 1}; // LD H,C
    t[0x62] = {O::LD,     AM::IMP,     4, 1}; // LD H,D
    t[0x63] = {O::LD,     AM::IMP,     4, 1}; // LD H,E
    t[0x64] = {O::LD,     AM::IMP,     4, 1}; // LD H,H
    t[0x65] = {O::LD,     AM::IMP,     4, 1}; // LD H,L
    t[0x66] = {O::LD,     AM::IND_HL,  7, 1}; // LD H,(HL)
    t[0x67] = {O::LD,     AM::IMP,     4, 1}; // LD H,A
    // 0x68-0x6F: LD L,r
    t[0x68] = {O::LD,     AM::IMP,     4, 1}; // LD L,B
    t[0x69] = {O::LD,     AM::IMP,     4, 1}; // LD L,C
    t[0x6A] = {O::LD,     AM::IMP,     4, 1}; // LD L,D
    t[0x6B] = {O::LD,     AM::IMP,     4, 1}; // LD L,E
    t[0x6C] = {O::LD,     AM::IMP,     4, 1}; // LD L,H
    t[0x6D] = {O::LD,     AM::IMP,     4, 1}; // LD L,L
    t[0x6E] = {O::LD,     AM::IND_HL,  7, 1}; // LD L,(HL)
    t[0x6F] = {O::LD,     AM::IMP,     4, 1}; // LD L,A
    // 0x70-0x77: LD (HL),r and HALT
    t[0x70] = {O::LD,     AM::IND_HL,  7, 1}; // LD (HL),B
    t[0x71] = {O::LD,     AM::IND_HL,  7, 1}; // LD (HL),C
    t[0x72] = {O::LD,     AM::IND_HL,  7, 1}; // LD (HL),D
    t[0x73] = {O::LD,     AM::IND_HL,  7, 1}; // LD (HL),E
    t[0x74] = {O::LD,     AM::IND_HL,  7, 1}; // LD (HL),H
    t[0x75] = {O::LD,     AM::IND_HL,  7, 1}; // LD (HL),L
    t[0x76] = {O::HALT,   AM::IMP,     4, 1}; // HALT
    t[0x77] = {O::LD,     AM::IND_HL,  7, 1}; // LD (HL),A
    // 0x78-0x7F: LD A,r
    t[0x78] = {O::LD,     AM::IMP,     4, 1}; // LD A,B
    t[0x79] = {O::LD,     AM::IMP,     4, 1}; // LD A,C
    t[0x7A] = {O::LD,     AM::IMP,     4, 1}; // LD A,D
    t[0x7B] = {O::LD,     AM::IMP,     4, 1}; // LD A,E
    t[0x7C] = {O::LD,     AM::IMP,     4, 1}; // LD A,H
    t[0x7D] = {O::LD,     AM::IMP,     4, 1}; // LD A,L
    t[0x7E] = {O::LD,     AM::IND_HL,  7, 1}; // LD A,(HL)
    t[0x7F] = {O::LD,     AM::IMP,     4, 1}; // LD A,A

    // Rows 0x80-0xBF: ALU A,r operations
    // 0x80-0x87: ADD A,r
    t[0x80] = {O::ADD,    AM::IMP,     4, 1};
    t[0x81] = {O::ADD,    AM::IMP,     4, 1};
    t[0x82] = {O::ADD,    AM::IMP,     4, 1};
    t[0x83] = {O::ADD,    AM::IMP,     4, 1};
    t[0x84] = {O::ADD,    AM::IMP,     4, 1};
    t[0x85] = {O::ADD,    AM::IMP,     4, 1};
    t[0x86] = {O::ADD,    AM::IND_HL,  7, 1};
    t[0x87] = {O::ADD,    AM::IMP,     4, 1};
    // 0x88-0x8F: ADC A,r
    t[0x88] = {O::ADC,    AM::IMP,     4, 1};
    t[0x89] = {O::ADC,    AM::IMP,     4, 1};
    t[0x8A] = {O::ADC,    AM::IMP,     4, 1};
    t[0x8B] = {O::ADC,    AM::IMP,     4, 1};
    t[0x8C] = {O::ADC,    AM::IMP,     4, 1};
    t[0x8D] = {O::ADC,    AM::IMP,     4, 1};
    t[0x8E] = {O::ADC,    AM::IND_HL,  7, 1};
    t[0x8F] = {O::ADC,    AM::IMP,     4, 1};
    // 0x90-0x97: SUB r
    t[0x90] = {O::SUB,    AM::IMP,     4, 1};
    t[0x91] = {O::SUB,    AM::IMP,     4, 1};
    t[0x92] = {O::SUB,    AM::IMP,     4, 1};
    t[0x93] = {O::SUB,    AM::IMP,     4, 1};
    t[0x94] = {O::SUB,    AM::IMP,     4, 1};
    t[0x95] = {O::SUB,    AM::IMP,     4, 1};
    t[0x96] = {O::SUB,    AM::IND_HL,  7, 1};
    t[0x97] = {O::SUB,    AM::IMP,     4, 1};
    // 0x98-0x9F: SBC A,r
    t[0x98] = {O::SBC,    AM::IMP,     4, 1};
    t[0x99] = {O::SBC,    AM::IMP,     4, 1};
    t[0x9A] = {O::SBC,    AM::IMP,     4, 1};
    t[0x9B] = {O::SBC,    AM::IMP,     4, 1};
    t[0x9C] = {O::SBC,    AM::IMP,     4, 1};
    t[0x9D] = {O::SBC,    AM::IMP,     4, 1};
    t[0x9E] = {O::SBC,    AM::IND_HL,  7, 1};
    t[0x9F] = {O::SBC,    AM::IMP,     4, 1};
    // 0xA0-0xA7: AND r
    t[0xA0] = {O::AND,    AM::IMP,     4, 1};
    t[0xA1] = {O::AND,    AM::IMP,     4, 1};
    t[0xA2] = {O::AND,    AM::IMP,     4, 1};
    t[0xA3] = {O::AND,    AM::IMP,     4, 1};
    t[0xA4] = {O::AND,    AM::IMP,     4, 1};
    t[0xA5] = {O::AND,    AM::IMP,     4, 1};
    t[0xA6] = {O::AND,    AM::IND_HL,  7, 1};
    t[0xA7] = {O::AND,    AM::IMP,     4, 1};
    // 0xA8-0xAF: XOR r
    t[0xA8] = {O::XOR,    AM::IMP,     4, 1};
    t[0xA9] = {O::XOR,    AM::IMP,     4, 1};
    t[0xAA] = {O::XOR,    AM::IMP,     4, 1};
    t[0xAB] = {O::XOR,    AM::IMP,     4, 1};
    t[0xAC] = {O::XOR,    AM::IMP,     4, 1};
    t[0xAD] = {O::XOR,    AM::IMP,     4, 1};
    t[0xAE] = {O::XOR,    AM::IND_HL,  7, 1};
    t[0xAF] = {O::XOR,    AM::IMP,     4, 1};
    // 0xB0-0xB7: OR r
    t[0xB0] = {O::OR,     AM::IMP,     4, 1};
    t[0xB1] = {O::OR,     AM::IMP,     4, 1};
    t[0xB2] = {O::OR,     AM::IMP,     4, 1};
    t[0xB3] = {O::OR,     AM::IMP,     4, 1};
    t[0xB4] = {O::OR,     AM::IMP,     4, 1};
    t[0xB5] = {O::OR,     AM::IMP,     4, 1};
    t[0xB6] = {O::OR,     AM::IND_HL,  7, 1};
    t[0xB7] = {O::OR,     AM::IMP,     4, 1};
    // 0xB8-0xBF: CP r
    t[0xB8] = {O::CP,     AM::IMP,     4, 1};
    t[0xB9] = {O::CP,     AM::IMP,     4, 1};
    t[0xBA] = {O::CP,     AM::IMP,     4, 1};
    t[0xBB] = {O::CP,     AM::IMP,     4, 1};
    t[0xBC] = {O::CP,     AM::IMP,     4, 1};
    t[0xBD] = {O::CP,     AM::IMP,     4, 1};
    t[0xBE] = {O::CP,     AM::IND_HL,  7, 1};
    t[0xBF] = {O::CP,     AM::IMP,     4, 1};

    // Row 0xC0
    t[0xC0] = {O::RET,    AM::IMP,    11, 1}; // RET NZ (11/5)
    t[0xC1] = {O::POP,    AM::IMP,    10, 1}; // POP BC
    t[0xC2] = {O::JP,     AM::ABS16,  10, 3}; // JP NZ,nn
    t[0xC3] = {O::JP,     AM::ABS16,  10, 3}; // JP nn
    t[0xC4] = {O::CALL,   AM::ABS16,  17, 3}; // CALL NZ,nn (17/10)
    t[0xC5] = {O::PUSH,   AM::IMP,    11, 1}; // PUSH BC
    t[0xC6] = {O::ADD,    AM::IMM8,    7, 2}; // ADD A,n
    t[0xC7] = {O::RST,    AM::RST,    11, 1}; // RST 00h
    t[0xC8] = {O::RET,    AM::IMP,    11, 1}; // RET Z (11/5)
    t[0xC9] = {O::RET,    AM::IMP,    10, 1}; // RET
    t[0xCA] = {O::JP,     AM::ABS16,  10, 3}; // JP Z,nn
    t[0xCB] = {O::NOP,    AM::PREFIX,  4, 1}; // CB prefix
    t[0xCC] = {O::CALL,   AM::ABS16,  17, 3}; // CALL Z,nn (17/10)
    t[0xCD] = {O::CALL,   AM::ABS16,  17, 3}; // CALL nn
    t[0xCE] = {O::ADC,    AM::IMM8,    7, 2}; // ADC A,n
    t[0xCF] = {O::RST,    AM::RST,    11, 1}; // RST 08h

    // Row 0xD0
    t[0xD0] = {O::RET,    AM::IMP,    11, 1}; // RET NC (11/5)
    t[0xD1] = {O::POP,    AM::IMP,    10, 1}; // POP DE
    t[0xD2] = {O::JP,     AM::ABS16,  10, 3}; // JP NC,nn
    t[0xD3] = {O::OUT,    AM::IND_IMM8,11,2}; // OUT (n),A
    t[0xD4] = {O::CALL,   AM::ABS16,  17, 3}; // CALL NC,nn (17/10)
    t[0xD5] = {O::PUSH,   AM::IMP,    11, 1}; // PUSH DE
    t[0xD6] = {O::SUB,    AM::IMM8,    7, 2}; // SUB n
    t[0xD7] = {O::RST,    AM::RST,    11, 1}; // RST 10h
    t[0xD8] = {O::RET,    AM::IMP,    11, 1}; // RET C (11/5)
    t[0xD9] = {O::EXX,    AM::IMP,     4, 1}; // EXX
    t[0xDA] = {O::JP,     AM::ABS16,  10, 3}; // JP C,nn
    t[0xDB] = {O::IN,     AM::IND_IMM8,11,2}; // IN A,(n)
    t[0xDC] = {O::CALL,   AM::ABS16,  17, 3}; // CALL C,nn (17/10)
    t[0xDD] = {O::NOP,    AM::PREFIX,  4, 1}; // DD prefix (IX)
    t[0xDE] = {O::SBC,    AM::IMM8,    7, 2}; // SBC A,n
    t[0xDF] = {O::RST,    AM::RST,    11, 1}; // RST 18h

    // Row 0xE0
    t[0xE0] = {O::RET,    AM::IMP,    11, 1}; // RET PO (11/5)
    t[0xE1] = {O::POP,    AM::IMP,    10, 1}; // POP HL
    t[0xE2] = {O::JP,     AM::ABS16,  10, 3}; // JP PO,nn
    t[0xE3] = {O::EX,     AM::IND_SP, 19, 1}; // EX (SP),HL
    t[0xE4] = {O::CALL,   AM::ABS16,  17, 3}; // CALL PO,nn (17/10)
    t[0xE5] = {O::PUSH,   AM::IMP,    11, 1}; // PUSH HL
    t[0xE6] = {O::AND,    AM::IMM8,    7, 2}; // AND n
    t[0xE7] = {O::RST,    AM::RST,    11, 1}; // RST 20h
    t[0xE8] = {O::RET,    AM::IMP,    11, 1}; // RET PE (11/5)
    t[0xE9] = {O::JP,     AM::IND_HL,  4, 1}; // JP (HL) — really JP HL
    t[0xEA] = {O::JP,     AM::ABS16,  10, 3}; // JP PE,nn
    t[0xEB] = {O::EX,     AM::IMP,     4, 1}; // EX DE,HL
    t[0xEC] = {O::CALL,   AM::ABS16,  17, 3}; // CALL PE,nn (17/10)
    t[0xED] = {O::NOP,    AM::PREFIX,  4, 1}; // ED prefix
    t[0xEE] = {O::XOR,    AM::IMM8,    7, 2}; // XOR n
    t[0xEF] = {O::RST,    AM::RST,    11, 1}; // RST 28h

    // Row 0xF0
    t[0xF0] = {O::RET,    AM::IMP,    11, 1}; // RET P (11/5)
    t[0xF1] = {O::POP,    AM::IMP,    10, 1}; // POP AF
    t[0xF2] = {O::JP,     AM::ABS16,  10, 3}; // JP P,nn
    t[0xF3] = {O::DI,     AM::IMP,     4, 1}; // DI
    t[0xF4] = {O::CALL,   AM::ABS16,  17, 3}; // CALL P,nn (17/10)
    t[0xF5] = {O::PUSH,   AM::IMP,    11, 1}; // PUSH AF
    t[0xF6] = {O::OR,     AM::IMM8,    7, 2}; // OR n
    t[0xF7] = {O::RST,    AM::RST,    11, 1}; // RST 30h
    t[0xF8] = {O::RET,    AM::IMP,    11, 1}; // RET M (11/5)
    t[0xF9] = {O::LD,     AM::IMP,     6, 1}; // LD SP,HL
    t[0xFA] = {O::JP,     AM::ABS16,  10, 3}; // JP M,nn
    t[0xFB] = {O::EI,     AM::IMP,     4, 1}; // EI
    t[0xFC] = {O::CALL,   AM::ABS16,  17, 3}; // CALL M,nn (17/10)
    t[0xFD] = {O::NOP,    AM::PREFIX,  4, 1}; // FD prefix (IY)
    t[0xFE] = {O::CP,     AM::IMM8,    7, 2}; // CP n
    t[0xFF] = {O::RST,    AM::RST,    11, 1}; // RST 38h

    return t;
}

// ============================================================================
// CB-prefix opcode table — 256 entries (bit/rotate/shift operations)
// ============================================================================
// Format: CB xx where xx encodes operation (bits 7-6), bit/reg (bits 5-3, 2-0)
//   00-07: RLC r    08-0F: RRC r    10-17: RL r     18-1F: RR r
//   20-27: SLA r    28-2F: SRA r    30-37: SLL r*   38-3F: SRL r
//   40-7F: BIT b,r  80-BF: RES b,r  C0-FF: SET b,r
//   * SLL is undocumented (shifts 1 into bit 0)

inline constexpr std::array<OpcodeInfo, 256> generate_cb_opcode_table() {
    std::array<OpcodeInfo, 256> t{};

    for (int i = 0; i < 256; i++) {
        uint8_t r = i & 0x07;
        uint8_t cycles = (r == 6) ? 15 : 8;  // (HL) variants take 15, register takes 8
        uint8_t group = (i >> 6) & 3;
        Op op;

        if (group == 0) {
            // Rotate/shift group: bits 5-3 select operation
            uint8_t shift_op = (i >> 3) & 7;
            switch (shift_op) {
                case 0: op = O::RLC; break;
                case 1: op = O::RRC; break;
                case 2: op = O::RL;  break;
                case 3: op = O::RR;  break;
                case 4: op = O::SLA; break;
                case 5: op = O::SRA; break;
                case 6: op = O::SLL; break;  // Undocumented
                case 7: op = O::SRL; break;
                default: op = O::NOP; break;
            }
            // BIT test on (HL) is 12 cycles, not 15
        } else if (group == 1) {
            op = O::BIT;
            if (r == 6) cycles = 12;  // BIT b,(HL) is 12 T-states
        } else if (group == 2) {
            op = O::RES;
        } else {
            op = O::SET;
        }

        AddrMode am = (r == 6) ? AM::IND_HL : AM::IMP;
        t[i] = {op, am, cycles, 2};  // All CB-prefix instructions are 2 bytes
    }

    return t;
}

// ============================================================================
// ED-prefix opcode table — 256 entries (extended instructions)
// ============================================================================
// Most ED-prefix opcodes in the 00-3F and 80+ (except block ops) range are
// NOPs or undefined.  The "good" ones are in 0x40-0x7F and block ops at
// 0xA0-0xBB.

inline constexpr std::array<OpcodeInfo, 256> generate_ed_opcode_table() {
    std::array<OpcodeInfo, 256> t{};

    // Default: all are 2-byte NOPs (8 T-states for the two fetches)
    for (int i = 0; i < 256; i++) {
        t[i] = {O::NOP, AM::IMP, 8, 2};
    }

    // 0x40-0x7F: I/O, 16-bit load/arithmetic, special registers
    // IN r,(C) / OUT (C),r
    t[0x40] = {O::IN,     AM::IND_C,  12, 2}; // IN B,(C)
    t[0x41] = {O::OUT,    AM::IND_C,  12, 2}; // OUT (C),B
    t[0x42] = {O::SBC16,  AM::IMP,    15, 2}; // SBC HL,BC
    t[0x43] = {O::LD,     AM::IND_ABS,20, 4}; // LD (nn),BC
    t[0x44] = {O::NEG,    AM::IMP,     8, 2}; // NEG
    t[0x45] = {O::RETN,   AM::IMP,    14, 2}; // RETN
    t[0x46] = {O::IM,     AM::IMP,     8, 2}; // IM 0
    t[0x47] = {O::LD_I_A, AM::IMP,     9, 2}; // LD I,A
    t[0x48] = {O::IN,     AM::IND_C,  12, 2}; // IN C,(C)
    t[0x49] = {O::OUT,    AM::IND_C,  12, 2}; // OUT (C),C
    t[0x4A] = {O::ADC16,  AM::IMP,    15, 2}; // ADC HL,BC
    t[0x4B] = {O::LD,     AM::IND_ABS,20, 4}; // LD BC,(nn)
    t[0x4C] = {O::NEG,    AM::IMP,     8, 2}; // NEG* (undocumented mirror)
    t[0x4D] = {O::RETI,   AM::IMP,    14, 2}; // RETI
    t[0x4E] = {O::IM,     AM::IMP,     8, 2}; // IM 0* (undocumented mirror)
    t[0x4F] = {O::LD_R_A, AM::IMP,     9, 2}; // LD R,A

    t[0x50] = {O::IN,     AM::IND_C,  12, 2}; // IN D,(C)
    t[0x51] = {O::OUT,    AM::IND_C,  12, 2}; // OUT (C),D
    t[0x52] = {O::SBC16,  AM::IMP,    15, 2}; // SBC HL,DE
    t[0x53] = {O::LD,     AM::IND_ABS,20, 4}; // LD (nn),DE
    t[0x54] = {O::NEG,    AM::IMP,     8, 2}; // NEG* (undocumented)
    t[0x55] = {O::RETN,   AM::IMP,    14, 2}; // RETN* (undocumented)
    t[0x56] = {O::IM,     AM::IMP,     8, 2}; // IM 1
    t[0x57] = {O::LD_A_I, AM::IMP,     9, 2}; // LD A,I
    t[0x58] = {O::IN,     AM::IND_C,  12, 2}; // IN E,(C)
    t[0x59] = {O::OUT,    AM::IND_C,  12, 2}; // OUT (C),E
    t[0x5A] = {O::ADC16,  AM::IMP,    15, 2}; // ADC HL,DE
    t[0x5B] = {O::LD,     AM::IND_ABS,20, 4}; // LD DE,(nn)
    t[0x5C] = {O::NEG,    AM::IMP,     8, 2}; // NEG* (undocumented)
    t[0x5D] = {O::RETN,   AM::IMP,    14, 2}; // RETN* (undocumented)
    t[0x5E] = {O::IM,     AM::IMP,     8, 2}; // IM 2
    t[0x5F] = {O::LD_A_R, AM::IMP,     9, 2}; // LD A,R

    t[0x60] = {O::IN,     AM::IND_C,  12, 2}; // IN H,(C)
    t[0x61] = {O::OUT,    AM::IND_C,  12, 2}; // OUT (C),H
    t[0x62] = {O::SBC16,  AM::IMP,    15, 2}; // SBC HL,HL
    t[0x63] = {O::LD,     AM::IND_ABS,20, 4}; // LD (nn),HL  (ED variant)
    t[0x64] = {O::NEG,    AM::IMP,     8, 2}; // NEG* (undocumented)
    t[0x65] = {O::RETN,   AM::IMP,    14, 2}; // RETN* (undocumented)
    t[0x66] = {O::IM,     AM::IMP,     8, 2}; // IM 0* (undocumented)
    t[0x67] = {O::RRD,    AM::IMP,    18, 2}; // RRD
    t[0x68] = {O::IN,     AM::IND_C,  12, 2}; // IN L,(C)
    t[0x69] = {O::OUT,    AM::IND_C,  12, 2}; // OUT (C),L
    t[0x6A] = {O::ADC16,  AM::IMP,    15, 2}; // ADC HL,HL
    t[0x6B] = {O::LD,     AM::IND_ABS,20, 4}; // LD HL,(nn)  (ED variant)
    t[0x6C] = {O::NEG,    AM::IMP,     8, 2}; // NEG* (undocumented)
    t[0x6D] = {O::RETN,   AM::IMP,    14, 2}; // RETN* (undocumented)
    t[0x6E] = {O::IM,     AM::IMP,     8, 2}; // IM 0* (undocumented)
    t[0x6F] = {O::RLD,    AM::IMP,    18, 2}; // RLD

    t[0x70] = {O::IN,     AM::IND_C,  12, 2}; // IN (C) / IN F,(C) (undoc, flags only)
    t[0x71] = {O::OUT,    AM::IND_C,  12, 2}; // OUT (C),0 (undocumented)
    t[0x72] = {O::SBC16,  AM::IMP,    15, 2}; // SBC HL,SP
    t[0x73] = {O::LD,     AM::IND_ABS,20, 4}; // LD (nn),SP
    t[0x74] = {O::NEG,    AM::IMP,     8, 2}; // NEG* (undocumented)
    t[0x75] = {O::RETN,   AM::IMP,    14, 2}; // RETN* (undocumented)
    t[0x76] = {O::IM,     AM::IMP,     8, 2}; // IM 1* (undocumented)
    // 0x77: NOP (default)
    t[0x78] = {O::IN,     AM::IND_C,  12, 2}; // IN A,(C)
    t[0x79] = {O::OUT,    AM::IND_C,  12, 2}; // OUT (C),A
    t[0x7A] = {O::ADC16,  AM::IMP,    15, 2}; // ADC HL,SP
    t[0x7B] = {O::LD,     AM::IND_ABS,20, 4}; // LD SP,(nn)
    t[0x7C] = {O::NEG,    AM::IMP,     8, 2}; // NEG* (undocumented)
    t[0x7D] = {O::RETN,   AM::IMP,    14, 2}; // RETN* (undocumented)
    t[0x7E] = {O::IM,     AM::IMP,     8, 2}; // IM 2* (undocumented)
    // 0x7F: NOP (default)

    // Block transfer/search instructions (0xA0-0xBB)
    t[0xA0] = {O::LDI,    AM::IMP,    16, 2}; // LDI
    t[0xA1] = {O::CPI,    AM::IMP,    16, 2}; // CPI
    t[0xA2] = {O::INI,    AM::IMP,    16, 2}; // INI
    t[0xA3] = {O::OUTI,   AM::IMP,    16, 2}; // OUTI
    // 0xA4-0xA7: NOP (default)
    t[0xA8] = {O::LDD,    AM::IMP,    16, 2}; // LDD
    t[0xA9] = {O::CPD,    AM::IMP,    16, 2}; // CPD
    t[0xAA] = {O::IND_OP, AM::IMP,    16, 2}; // IND
    t[0xAB] = {O::OUTD,   AM::IMP,    16, 2}; // OUTD
    // 0xAC-0xAF: NOP (default)
    t[0xB0] = {O::LDIR,   AM::IMP,    21, 2}; // LDIR (21/16)
    t[0xB1] = {O::CPIR,   AM::IMP,    21, 2}; // CPIR (21/16)
    t[0xB2] = {O::INIR,   AM::IMP,    21, 2}; // INIR (21/16)
    t[0xB3] = {O::OTIR,   AM::IMP,    21, 2}; // OTIR (21/16)
    // 0xB4-0xB7: NOP (default)
    t[0xB8] = {O::LDDR,   AM::IMP,    21, 2}; // LDDR (21/16)
    t[0xB9] = {O::CPDR,   AM::IMP,    21, 2}; // CPDR (21/16)
    t[0xBA] = {O::INDR,   AM::IMP,    21, 2}; // INDR (21/16)
    t[0xBB] = {O::OTDR,   AM::IMP,    21, 2}; // OTDR (21/16)

    return t;
}

// ============================================================================
// Static table instances (inline so they're shared across TUs)
// ============================================================================

inline constexpr auto base_opcode_table = generate_base_opcode_table();
inline constexpr auto cb_opcode_table   = generate_cb_opcode_table();
inline constexpr auto ed_opcode_table   = generate_ed_opcode_table();

} // namespace z80
