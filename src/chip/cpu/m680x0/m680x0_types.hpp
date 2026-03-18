#pragma once
/*
 * m680x0_types.hpp — Shared types for Motorola 680x0 CPU family
 *
 * Enums, flag definitions, and utility types used across the M680x0
 * implementation.  Separate from the main template to keep compile times low.
 */

#include <cstdint>

namespace m680x0 {

// ============================================================================
// Condition Code Register (CCR) — lower 8 bits of the Status Register
// ============================================================================
// The 68000 CCR is 5 bits: C, V, Z, N, X
// Bits 5-7 are always zero.

namespace Flags {
    constexpr uint8_t C = 0x01;   // Carry
    constexpr uint8_t V = 0x02;   // Overflow
    constexpr uint8_t Z = 0x04;   // Zero
    constexpr uint8_t N = 0x08;   // Negative
    constexpr uint8_t X = 0x10;   // Extend (operand carry for multi-precision)
    constexpr uint8_t CCR_MASK = 0x1F;
} // namespace Flags

// ============================================================================
// Status Register — upper byte (supervisor portion)
// ============================================================================

namespace SRBits {
    constexpr uint16_t I0 = 1 << 8;     // Interrupt Priority Mask bit 0
    constexpr uint16_t I1 = 1 << 9;     // Interrupt Priority Mask bit 1
    constexpr uint16_t I2 = 1 << 10;    // Interrupt Priority Mask bit 2
    constexpr uint16_t IPM_MASK = I0 | I1 | I2;
    constexpr uint16_t IPM_SHIFT = 8;

    constexpr uint16_t S  = 1 << 13;    // Supervisor/User state (1=supervisor)
    constexpr uint16_t T0 = 1 << 14;    // Trace enable bit 0 (68020+)
    constexpr uint16_t T1 = 1 << 15;    // Trace enable bit 1

    constexpr uint16_t M  = 1 << 12;    // Master/Interrupt state (68020+)

    constexpr uint16_t SR_MASK = 0xA71F; // Valid SR bits for 68000 (T1, S, I2-I0, CCR)

    inline constexpr uint8_t get_ipm(uint16_t sr) {
        return static_cast<uint8_t>((sr & IPM_MASK) >> IPM_SHIFT);
    }
} // namespace SRBits

// ============================================================================
// Condition Codes (for Bcc, Scc, DBcc)
// ============================================================================
// Encoded in bits 11-8 of the instruction word

enum class Condition : uint8_t {
    T  = 0,   // True (always)
    F  = 1,   // False (never)
    HI = 2,   // High           (!C && !Z)
    LS = 3,   // Low or Same    (C || Z)
    CC = 4,   // Carry Clear    (!C)   — also "HS"
    CS = 5,   // Carry Set      (C)    — also "LO"
    NE = 6,   // Not Equal      (!Z)
    EQ = 7,   // Equal          (Z)
    VC = 8,   // Overflow Clear (!V)
    VS = 9,   // Overflow Set   (V)
    PL = 10,  // Plus           (!N)
    MI = 11,  // Minus          (N)
    GE = 12,  // Greater/Equal  (N && V) || (!N && !V)
    LT = 13,  // Less Than      (N && !V) || (!N && V)
    GT = 14,  // Greater Than   (N && V && !Z) || (!N && !V && !Z)
    LE = 15,  // Less/Equal     (Z) || (N && !V) || (!N && V)
};

// ============================================================================
// Operation Size
// ============================================================================

enum class OpSize : uint8_t {
    Byte = 0,   // .B — 8-bit
    Word = 1,   // .W — 16-bit
    Long = 2,   // .L — 32-bit
};

// ============================================================================
// Effective Address Modes (encoded in bits 5-0 of instruction word)
// ============================================================================
// Mode field = bits 5-3, Register field = bits 2-0

enum class EAMode : uint8_t {
    DataRegDirect    = 0,  // Dn
    AddrRegDirect    = 1,  // An
    AddrRegIndirect  = 2,  // (An)
    AddrRegPostInc   = 3,  // (An)+
    AddrRegPreDec    = 4,  // -(An)
    AddrRegDisp      = 5,  // (d16,An)
    AddrRegIndex     = 6,  // (d8,An,Xn)
    Special          = 7,  // Determined by register field:
    //  reg=0: Abs.W   ($xxxx)
    //  reg=1: Abs.L   ($xxxxxxxx)
    //  reg=2: (d16,PC)
    //  reg=3: (d8,PC,Xn)
    //  reg=4: #imm
};

// ============================================================================
// Exception Vector Numbers
// ============================================================================

namespace Vector {
    constexpr uint8_t RESET_SSP          = 0;   // Initial SSP (vector 0)
    constexpr uint8_t RESET_PC           = 1;   // Initial PC (vector 1)
    constexpr uint8_t BUS_ERROR          = 2;
    constexpr uint8_t ADDRESS_ERROR      = 3;
    constexpr uint8_t ILLEGAL_INSTR      = 4;
    constexpr uint8_t ZERO_DIVIDE        = 5;
    constexpr uint8_t CHK_INSTR          = 6;
    constexpr uint8_t TRAPV_INSTR        = 7;
    constexpr uint8_t PRIVILEGE_VIOLATION = 8;
    constexpr uint8_t TRACE              = 9;
    constexpr uint8_t LINE_A             = 10;  // Unimplemented A-line
    constexpr uint8_t LINE_F             = 11;  // Unimplemented F-line
    // 12-14: reserved
    constexpr uint8_t UNINIT_INT         = 15;  // Uninitialized interrupt
    // 16-23: reserved
    constexpr uint8_t SPURIOUS_INT       = 24;  // Spurious interrupt
    constexpr uint8_t AUTO_VECTOR_1      = 25;  // Level 1 autovector
    constexpr uint8_t AUTO_VECTOR_2      = 26;
    constexpr uint8_t AUTO_VECTOR_3      = 27;
    constexpr uint8_t AUTO_VECTOR_4      = 28;
    constexpr uint8_t AUTO_VECTOR_5      = 29;
    constexpr uint8_t AUTO_VECTOR_6      = 30;
    constexpr uint8_t AUTO_VECTOR_7      = 31;  // Level 7 autovector (NMI)
    constexpr uint8_t TRAP_BASE          = 32;  // TRAP #0 .. TRAP #15 → vectors 32-47
    // 48-63: reserved
    // 64-255: user interrupt vectors
} // namespace Vector

} // namespace m680x0
