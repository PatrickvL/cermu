#pragma once
/*
 * z80_types.h — Shared types for Z80 CPU family
 *
 * Enums, opcode descriptors, and utility types used across the Z80
 * implementation. Separate from the main template to keep compile times low.
 */

#include <cstdint>

namespace z80 {

// ============================================================================
// Z80 Flag Bits (F register)
// ============================================================================

namespace Flags {
    constexpr uint8_t C  = 0x01;  // Carry
    constexpr uint8_t N  = 0x02;  // Add/Subtract
    constexpr uint8_t PV = 0x04;  // Parity/Overflow
    constexpr uint8_t X  = 0x08;  // Undocumented (copy of bit 3)
    constexpr uint8_t H  = 0x10;  // Half carry
    constexpr uint8_t Y  = 0x20;  // Undocumented (copy of bit 5)
    constexpr uint8_t Z  = 0x40;  // Zero
    constexpr uint8_t S  = 0x80;  // Sign
} // namespace Flags

// ============================================================================
// Flag Layout Structs — compile-time flag bit positions per CPU variant
// ============================================================================
// Used via `using Fl = std::conditional_t<is_sm83(), SM83FlagLayout, Z80FlagLayout>`
// inside z80_t. ALU code uses Fl::C, Fl::Z, etc. — flag-position-agnostic.
// Absent flags (S, PV, X, Y on SM83) are zero, making OR-expressions no-ops.

struct Z80FlagLayout {
    static constexpr uint8_t C  = 0x01;  // Carry
    static constexpr uint8_t N  = 0x02;  // Add/Subtract
    static constexpr uint8_t PV = 0x04;  // Parity/Overflow
    static constexpr uint8_t X  = 0x08;  // Undocumented (copy of bit 3)
    static constexpr uint8_t H  = 0x10;  // Half carry
    static constexpr uint8_t Y  = 0x20;  // Undocumented (copy of bit 5)
    static constexpr uint8_t Z  = 0x40;  // Zero
    static constexpr uint8_t S  = 0x80;  // Sign
    static constexpr uint8_t VALID = 0xFF;
};

struct SM83FlagLayout {
    static constexpr uint8_t C  = 0x10;  // Carry (bit 4)
    static constexpr uint8_t H  = 0x20;  // Half carry (bit 5)
    static constexpr uint8_t N  = 0x40;  // Add/Subtract (bit 6)
    static constexpr uint8_t Z  = 0x80;  // Zero (bit 7)
    // SM83 lacks these — zero constants make OR expressions no-ops
    static constexpr uint8_t S  = 0;
    static constexpr uint8_t PV = 0;
    static constexpr uint8_t X  = 0;
    static constexpr uint8_t Y  = 0;
    static constexpr uint8_t VALID = 0xF0;
};

// ============================================================================
// Z80 Condition Codes (for JR/JP/CALL/RET cc)
// ============================================================================

enum class Condition : uint8_t {
    NZ = 0,  // Not Zero
    Z  = 1,  // Zero
    NC = 2,  // No Carry
    C  = 3,  // Carry
    PO = 4,  // Parity Odd (P/V clear)
    PE = 5,  // Parity Even (P/V set)
    P  = 6,  // Sign Positive (S clear)
    M  = 7,  // Sign Minus (S set)
};

// ============================================================================
// Z80 Register Encoding (for r and rp fields in opcodes)
// ============================================================================

enum class Reg8 : uint8_t {
    B = 0, C = 1, D = 2, E = 3, H = 4, L = 5, HL_IND = 6, A = 7
};

enum class Reg16 : uint8_t {
    BC = 0, DE = 1, HL = 2, SP = 3
};

enum class Reg16AF : uint8_t {
    BC = 0, DE = 1, HL = 2, AF = 3
};

// ============================================================================
// Interrupt Mode
// ============================================================================

enum class IntMode : uint8_t {
    IM0 = 0,   // Execute instruction on data bus (8080 compatible)
    IM1 = 1,   // RST 38h
    IM2 = 2,   // Vectored (I register × 256 + data bus byte)
};

} // namespace z80
