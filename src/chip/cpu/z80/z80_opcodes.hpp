#pragma once
/*
 * z80_opcodes.h — Z80 Opcode Descriptor Types
 *
 * Opcode info structure and operation/addressing-mode enums for the Z80
 * family.  Used by the opcode table, decoder/disassembler, and the main
 * CPU template.
 *
 * Follows the fam65xx opcode_info_t pattern.
 */

#include <cstdint>

namespace z80 {

// ============================================================================
// Z80 Addressing Modes
// ============================================================================
// The Z80 has far fewer orthogonal addressing modes than the 6502 — most
// instructions encode the operand type in the opcode bits directly.
// This enum classifies the *operand format* for the disassembler and
// instruction-length calculation.

enum class AddrMode : uint8_t {
    IMP = 0,  // Implicit / register-only (NOP, HALT, EX AF,AF', etc.)
    IMM8,     // 8-bit immediate:  LD A,n / ADD A,n
    IMM16,    // 16-bit immediate: LD BC,nn / LD SP,nn
    REL8,     // Relative branch:  JR cc,e  (signed 8-bit offset)
    ABS16,    // Absolute address:  JP nn / CALL nn / LD (nn),A
    IND_HL,   // (HL) implied in opcode (no extra bytes)
    IND_BC,   // (BC) implied
    IND_DE,   // (DE) implied
    IND_SP,   // (SP) implied (EX (SP),HL)
    IND_C,    // (C) I/O port: IN r,(C) / OUT (C),r
    IND_IMM8, // (n) I/O port 8-bit: IN A,(n) / OUT (n),A
    IND_ABS,  // (nn) indirect absolute: LD A,(nn) / LD (nn),HL
    IX_D,     // (IX+d) indexed with signed displacement
    IY_D,     // (IY+d) indexed with signed displacement
    RST,      // RST target embedded in opcode (no extra bytes)
    BIT_OP,   // CB-prefix bit operations (bit number in opcode)
    PREFIX,   // Prefix byte (CB/DD/ED/FD) — not a real instruction
    COUNT
};

// ============================================================================
// Z80 Operations
// ============================================================================
// Mnemonic indices for all Z80 instructions (standard + undocumented).

enum class Op : uint8_t {
    // ---- 8-bit load ----
    LD = 0,         // General LD (covers register, immediate, indirect, etc.)

    // ---- 8-bit arithmetic/logic ----
    ADD,  ADC,  SUB,  SBC,  AND,  XOR,  OR,   CP,
    INC,  DEC,

    // ---- 16-bit arithmetic ----
    ADD16, ADC16, SBC16, INC16, DEC16,

    // ---- Rotate/shift (accumulator) ----
    RLCA, RRCA, RLA,  RRA,

    // ---- CB-prefix rotate/shift ----
    RLC,  RRC,  RL,   RR,   SLA,  SRA,  SLL,  SRL,

    // ---- CB-prefix bit operations ----
    BIT,  RES,  SET,

    // ---- Branch/jump ----
    JP,   JR,   DJNZ, CALL, RET,  RETI, RETN, RST,

    // ---- Stack ----
    PUSH, POP,

    // ---- Exchange ----
    EX,   EXX,

    // ---- Block transfer/search ----
    LDI,  LDIR, LDD,  LDDR,
    CPI,  CPIR, CPD,  CPDR,

    // ---- Block I/O ----
    INI,  INIR, IND_OP, INDR,
    OUTI, OTIR, OUTD, OTDR,

    // ---- I/O ----
    IN,   OUT,

    // ---- Interrupt/CPU control ----
    DI,   EI,   IM,   HALT, NOP,

    // ---- Special ----
    DAA,  CPL,  NEG,  CCF,  SCF,
    LD_I_A, LD_R_A, LD_A_I, LD_A_R,
    RLD,  RRD,

    // ---- Undocumented ----
    // SLL is already above (CB 0x30-0x37)
    // IX/IY half-register ops use standard LD/ADD/etc. with special reg encoding

    // Marker
    INVALID,
    COUNT
};

// ============================================================================
// Opcode Info — compact descriptor for one Z80 opcode
// ============================================================================

struct OpcodeInfo {
    uint8_t op;        // Operation enum (Op)
    uint8_t am;        // Addressing mode (AddrMode)
    uint8_t cycles;    // Base T-state count (excluding wait/contention)
    uint8_t length;    // Instruction length in bytes (1-4)

    constexpr OpcodeInfo()
        : op(static_cast<uint8_t>(Op::NOP))
        , am(static_cast<uint8_t>(AddrMode::IMP))
        , cycles(4)
        , length(1) {}

    constexpr OpcodeInfo(Op o, AddrMode a, uint8_t c, uint8_t len)
        : op(static_cast<uint8_t>(o))
        , am(static_cast<uint8_t>(a))
        , cycles(c)
        , length(len) {}
};

} // namespace z80
