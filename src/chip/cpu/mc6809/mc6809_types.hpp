#pragma once
/*
 * mc6809_types.hpp — MC6809 Register Encoding, Flags, Condition Codes
 *
 * Type definitions shared across the MC6809 family template and
 * supporting files (decoder, test harness, etc.).
 */

#include <cstdint>

namespace mc6809 {

// ============================================================================
// MC6809 Condition Code Register (CC) Flag Bits
// ============================================================================

namespace Flags {
    constexpr uint8_t C  = 0x01;  // Carry
    constexpr uint8_t V  = 0x02;  // Overflow
    constexpr uint8_t Z  = 0x04;  // Zero
    constexpr uint8_t N  = 0x08;  // Negative
    constexpr uint8_t I  = 0x10;  // IRQ mask
    constexpr uint8_t H  = 0x20;  // Half carry (BCD)
    constexpr uint8_t F  = 0x40;  // FIRQ mask
    constexpr uint8_t E  = 0x80;  // Entire state saved on stack
} // namespace Flags

// ============================================================================
// MC6809 Transfer/Exchange Register Encoding (TFR/EXG post-byte)
// ============================================================================
// The 4-bit register code used in TFR and EXG instructions.
// Upper nibble = source, lower nibble = destination.
// Bit 3 distinguishes 16-bit (0) from 8-bit (1) registers.

namespace RegCode {
    // 16-bit registers (bit 3 = 0)
    constexpr uint8_t D   = 0x00;  // A:B accumulator pair
    constexpr uint8_t X   = 0x01;  // Index register X
    constexpr uint8_t Y   = 0x02;  // Index register Y
    constexpr uint8_t U   = 0x03;  // User stack pointer
    constexpr uint8_t S   = 0x04;  // System stack pointer
    constexpr uint8_t PC  = 0x05;  // Program counter
    // HD6309 16-bit
    constexpr uint8_t W   = 0x06;  // E:F accumulator pair (HD6309)
    constexpr uint8_t V   = 0x07;  // V register (HD6309)

    // 8-bit registers (bit 3 = 1)
    constexpr uint8_t A   = 0x08;  // Accumulator A (high byte of D)
    constexpr uint8_t B   = 0x09;  // Accumulator B (low byte of D)
    constexpr uint8_t CC  = 0x0A;  // Condition codes
    constexpr uint8_t DP  = 0x0B;  // Direct page register
    // HD6309 8-bit
    constexpr uint8_t E   = 0x0E;  // Accumulator E (high byte of W, HD6309)
    constexpr uint8_t F_REG = 0x0F; // Accumulator F (low byte of W, HD6309)
    constexpr uint8_t ZERO = 0x0C; // Zero register (HD6309, always reads 0)
} // namespace RegCode

// ============================================================================
// MC6809 Indexed Addressing Mode Post-Byte Encoding
// ============================================================================

namespace IndexMode {
    // Post-byte bit 7 = 0: 5-bit constant offset (-16 to +15) from R
    // Post-byte bit 7 = 1: indexed mode type in bits 0-4

    // Register field (bits 5-6 of post-byte)
    constexpr uint8_t REG_MASK  = 0x60;
    constexpr uint8_t REG_SHIFT = 5;
    // 0=X, 1=Y, 2=U, 3=S

    // Mode field when bit 7 = 1 (bits 0-4)
    constexpr uint8_t AUTO_INC1     = 0x00;  // ,R+
    constexpr uint8_t AUTO_INC2     = 0x01;  // ,R++
    constexpr uint8_t AUTO_DEC1     = 0x02;  // ,-R
    constexpr uint8_t AUTO_DEC2     = 0x03;  // ,--R
    constexpr uint8_t NO_OFFSET     = 0x04;  // ,R (zero offset)
    constexpr uint8_t ACCB_OFFSET   = 0x05;  // B,R
    constexpr uint8_t ACCA_OFFSET   = 0x06;  // A,R
    constexpr uint8_t OFFSET_8      = 0x08;  // n8,R
    constexpr uint8_t OFFSET_16     = 0x09;  // n16,R
    constexpr uint8_t ACCD_OFFSET   = 0x0B;  // D,R
    constexpr uint8_t PC_OFFSET_8   = 0x0C;  // n8,PCR
    constexpr uint8_t PC_OFFSET_16  = 0x0D;  // n16,PCR
    // HD6309 additions
    constexpr uint8_t ACCW_OFFSET   = 0x0E;  // W,R (HD6309)
    constexpr uint8_t ACCE_OFFSET   = 0x07;  // E,R (HD6309)
    constexpr uint8_t ACCF_OFFSET   = 0x0A;  // F,R (HD6309)

    // Indirect bit (bit 4 of post-byte when bit 7 = 1)
    constexpr uint8_t INDIRECT      = 0x10;

    // Extended indirect: post-byte = $9F
    constexpr uint8_t EXTENDED_IND  = 0x1F;
} // namespace IndexMode

// ============================================================================
// MC6809 Push/Pull Register Bit Mask (PSH/PUL post-byte)
// ============================================================================

namespace StackBit {
    constexpr uint8_t CC  = 0x01;
    constexpr uint8_t A   = 0x02;
    constexpr uint8_t B   = 0x04;
    constexpr uint8_t DP  = 0x08;
    constexpr uint8_t X   = 0x10;
    constexpr uint8_t Y   = 0x20;
    constexpr uint8_t U_S = 0x40;  // U for PSHS/PULS, S for PSHU/PULU
    constexpr uint8_t PC  = 0x80;
} // namespace StackBit

// ============================================================================
// Interrupt Vectors
// ============================================================================

namespace Vector {
    constexpr uint16_t RESET = 0xFFFE;
    constexpr uint16_t NMI   = 0xFFFC;
    constexpr uint16_t SWI   = 0xFFFA;
    constexpr uint16_t IRQ   = 0xFFF8;
    constexpr uint16_t FIRQ  = 0xFFF6;
    constexpr uint16_t SWI2  = 0xFFF4;
    constexpr uint16_t SWI3  = 0xFFF2;
    constexpr uint16_t RES   = 0xFFFE;  // Reserved — same as RESET
} // namespace Vector

} // namespace mc6809
