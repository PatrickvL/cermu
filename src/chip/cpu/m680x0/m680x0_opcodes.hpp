#pragma once
/*
 * m680x0_opcodes.hpp — Opcode instruction group definitions for M680x0
 *
 * The 68000 instruction word is 16 bits.  Bits 15-12 select one of 16
 * instruction groups.  Further decoding is group-specific.
 *
 * This header defines the instruction group enum, the opcode mnemonic
 * enum, and addressing mode descriptors for disassembly support.
 */

#include <cstdint>

namespace m680x0 {

// ============================================================================
// Instruction Groups (bits 15-12 of the first instruction word)
// ============================================================================

enum class InstrGroup : uint8_t {
    GROUP_0  = 0x0,  // Bit manipulation / MOVEP / Immediate (ORI, ANDI, SUBI, ADDI, EORI, CMPI, BTST, BCHG, BCLR, BSET)
    GROUP_1  = 0x1,  // MOVE.B
    GROUP_2  = 0x2,  // MOVE.L / MOVEA.L
    GROUP_3  = 0x3,  // MOVE.W / MOVEA.W
    GROUP_4  = 0x4,  // Miscellaneous (LEA, PEA, CHK, EXT, SWAP, TAS, TST, TRAP, LINK, UNLK, MOVEM, JSR, JMP, MOVE USP, etc.)
    GROUP_5  = 0x5,  // ADDQ / SUBQ / Scc / DBcc
    GROUP_6  = 0x6,  // Bcc / BSR / BRA
    GROUP_7  = 0x7,  // MOVEQ
    GROUP_8  = 0x8,  // OR / DIVU / DIVS / SBCD
    GROUP_9  = 0x9,  // SUB / SUBA / SUBX
    GROUP_A  = 0xA,  // A-line (unimplemented — traps to Line-A vector)
    GROUP_B  = 0xB,  // CMP / CMPA / CMPM / EOR
    GROUP_C  = 0xC,  // AND / MULU / MULS / ABCD / EXG
    GROUP_D  = 0xD,  // ADD / ADDA / ADDX
    GROUP_E  = 0xE,  // Shift / Rotate (ASL, ASR, LSL, LSR, ROL, ROR, ROXL, ROXR)
    GROUP_F  = 0xF,  // F-line (coprocessor / unimplemented — traps to Line-F vector)
};

// ============================================================================
// Instruction Mnemonic Enumeration (for disassembler and opcode metadata)
// ============================================================================

enum class Op : uint8_t {
    // --- Data Movement ---
    MOVE, MOVEA, MOVEQ, MOVEM, MOVEP, MOVE_USP, MOVE_CCR, MOVE_SR,
    LEA, PEA, EXG, SWAP, LINK, UNLK,

    // --- Arithmetic ---
    ADD, ADDA, ADDI, ADDQ, ADDX,
    SUB, SUBA, SUBI, SUBQ, SUBX,
    MULU, MULS, DIVU, DIVS,
    NEG, NEGX, CLR, CMP, CMPA, CMPI, CMPM,
    EXT, TST,

    // --- Logic ---
    AND, ANDI, OR, ORI, EOR, EORI, NOT,

    // --- Shift/Rotate ---
    ASL, ASR, LSL, LSR, ROL, ROR, ROXL, ROXR,

    // --- Bit Manipulation ---
    BTST, BSET, BCLR, BCHG,

    // --- BCD ---
    ABCD, SBCD, NBCD,

    // --- Program Control ---
    BRA, BSR, BCC,      // Branch always, subroutine, conditional
    DBCC,               // Decrement and branch
    SCC,                // Set on condition
    JMP, JSR,           // Jump, jump to subroutine
    RTS, RTE, RTR,      // Return from subroutine/exception/restore CCR
    NOP, RESET, STOP, TRAP, TRAPV,
    CHK,

    // --- System Control ---
    ANDI_SR, EORI_SR, ORI_SR,
    ANDI_CCR, EORI_CCR, ORI_CCR,
    TAS,

    // --- Privileged ---
    MOVE_FROM_SR,

    // --- Special ---
    ILLEGAL,
    LINE_A,
    LINE_F,

    // --- Pseudo ---
    UNKNOWN,    // Unrecognized opcode (for disassembler)
};

// ============================================================================
// Opcode Info — metadata for disassembly
// ============================================================================

struct OpcodeInfo {
    Op          op;         // Mnemonic
    uint8_t     size;       // Instruction size in bytes (minimum, before extension words)
    uint8_t     base_cycles;// Base clock cycles (register-to-register case)
};

// ============================================================================
// Instruction word field extraction helpers
// ============================================================================

inline constexpr uint8_t instr_group(uint16_t opcode) {
    return static_cast<uint8_t>((opcode >> 12) & 0xF);
}

inline constexpr uint8_t instr_ea_mode(uint16_t opcode) {
    return static_cast<uint8_t>((opcode >> 3) & 0x7);
}

inline constexpr uint8_t instr_ea_reg(uint16_t opcode) {
    return static_cast<uint8_t>(opcode & 0x7);
}

inline constexpr uint8_t instr_reg(uint16_t opcode) {
    return static_cast<uint8_t>((opcode >> 9) & 0x7);
}

inline constexpr uint8_t instr_opmode(uint16_t opcode) {
    return static_cast<uint8_t>((opcode >> 6) & 0x7);
}

inline constexpr uint8_t instr_size_field(uint16_t opcode) {
    return static_cast<uint8_t>((opcode >> 6) & 0x3);
}

inline constexpr uint8_t instr_condition(uint16_t opcode) {
    return static_cast<uint8_t>((opcode >> 8) & 0xF);
}

} // namespace m680x0
