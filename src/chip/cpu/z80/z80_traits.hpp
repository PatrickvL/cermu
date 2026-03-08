#pragma once
/*
 * z80_traits.hpp — CPU Traits System for Zilog Z80 Family
 *
 * Base types for the Z80 CPU traits system: Z80CoreFlags, enums,
 * Z80Traits struct, commonly-used flag combinations, and template helpers.
 *
 * Individual CPU trait constants, type aliases, and pin layouts live in
 * per-CPU wrapper headers (z80.h, z80a.h, etc.).
 *
 * Follows the fam65xx CPUTraits pattern — NTTP-based template dispatch with
 * constexpr feature detection.
 */

#include <cstdint>
#include <type_traits>

namespace z80 {

namespace detail {

// ============================================================================
// Z80 Core Feature Flags (CPU execution behavior only)
// ============================================================================

namespace Z80CoreFlags {

// === Instruction Set (bits 0-7) ===
constexpr uint32_t UNDOCUMENTED_OPS  = 1 << 0;  // Undocumented opcodes (SLL, IX/IY half-reg, etc.)
constexpr uint32_t Z180_EXTENDED     = 1 << 1;  // Z180/HD64180 extended instructions (TST, MLT, IN0/OUT0)
constexpr uint32_t EZ80_EXTENDED     = 1 << 2;  // eZ80 24-bit extensions (ADL mode)
constexpr uint32_t R800_EXTENDED     = 1 << 3;  // R800 (MSX turbo R) multiply instructions
// Bits 4-7 reserved

// === Hardware Bugs / Quirks (bits 8-11) ===
constexpr uint32_t NMOS_TIMING       = 1 << 8;  // Original NMOS Z80 timing (vs CMOS variants)
constexpr uint32_t BLOCK_INT_BUG     = 1 << 9;  // NMOS: LD A,I / LD A,R reset IFF2→P/V on interrupt edge
constexpr uint32_t Q_REGISTER        = 1 << 10; // Zilog Z80 internal Q register (affects SCF/CCF flags)
// Bit 11 reserved

// === Address Bus Width (bits 12-15) ===
constexpr uint32_t ADDR_24BIT        = 1 << 12; // eZ80: 24-bit address bus (16MB)
constexpr uint32_t HAS_MMU           = 1 << 13; // Z180: built-in MMU for 20-bit physical addressing
// Bits 14-15 reserved

// === Integrated Peripherals (bits 16-23) ===
constexpr uint32_t HAS_UART          = 1 << 16; // Z180: two built-in ASCIs (async serial)
constexpr uint32_t HAS_DMA           = 1 << 17; // Z180: built-in DMA controller
constexpr uint32_t HAS_PRT           = 1 << 18; // Z180: built-in programmable reload timers
constexpr uint32_t HAS_WAIT_STATES   = 1 << 19; // Z180: programmable I/O wait states
// Bits 20-23 reserved

// === Clock (bits 24-27) ===
constexpr uint32_t CLOCK_DOUBLED     = 1 << 24; // R800: doubled internal clock (7.16 MHz effective)
// Bits 25-27 reserved

} // namespace Z80CoreFlags

// ============================================================================
// Integrated Peripheral Configuration
// ============================================================================

enum class Z80SoundChip : uint8_t {
    NONE = 0,
    // Future: integrated sound (none on standard Z80 variants)
};

enum class Z80DMAType : uint8_t {
    NONE = 0,
    Z180_DMA,     // Z180 built-in 2-channel DMA
    // Future: Z80 DMA peripheral chip (external, not in traits)
};

struct Z80PeripheralConfig {
    Z80SoundChip sound;
    Z80DMAType   dma;
    bool         has_uart;     // Z180 ASCI channels
    bool         has_timers;   // Z180 PRT

    constexpr bool has_sound() const { return sound != Z80SoundChip::NONE; }
    constexpr bool has_dma()   const { return dma   != Z80DMAType::NONE; }
};

// ============================================================================
// Complete Z80 Trait Structure
// ============================================================================

struct Z80Traits {
    const char* vendor;            // "Zilog", "NEC", "Sharp", "Toshiba", "SGS-Thomson"
    const char* chip_id;           // "Z80", "Z80A", "Z180", "R800"
    uint32_t    core_flags;        // Z80CoreFlags combination
    uint8_t     address_bits;      // Address bus width (16, 20, 24)
    uint8_t     max_clock_mhz_x10; // Max rated clock × 10 (e.g. 40 = 4.0 MHz, 60 = 6.0 MHz)
    Z80PeripheralConfig peripheral;// Integrated peripherals

    // === Helper Methods ===

    constexpr bool has(uint32_t flag) const { return (core_flags & flag) != 0; }

    constexpr bool is_nmos() const { return has(Z80CoreFlags::NMOS_TIMING); }
    constexpr bool is_cmos() const { return !is_nmos(); }

    constexpr bool has_undocumented_ops() const { return has(Z80CoreFlags::UNDOCUMENTED_OPS); }
    constexpr bool has_block_int_bug()    const { return has(Z80CoreFlags::BLOCK_INT_BUG); }
    constexpr bool has_q_register()       const { return has(Z80CoreFlags::Q_REGISTER); }
    constexpr bool has_mmu()              const { return has(Z80CoreFlags::HAS_MMU); }

    constexpr uint32_t address_mask() const { return ~(~0u << address_bits); }

    constexpr bool has_24bit_addr() const { return has(Z80CoreFlags::ADDR_24BIT); }

    constexpr const char* get_vendor()  const { return vendor; }
    constexpr const char* get_chip_id() const { return chip_id; }

    // Equality (pointer comparison for string literals)
    constexpr bool operator==(const Z80Traits& other) const {
        return vendor == other.vendor && chip_id == other.chip_id &&
               core_flags == other.core_flags &&
               address_bits == other.address_bits &&
               max_clock_mhz_x10 == other.max_clock_mhz_x10;
    }
    constexpr bool operator!=(const Z80Traits& other) const { return !(*this == other); }
};

// ============================================================================
// Common Flag Combinations
// ============================================================================

namespace CoreFlags {

// Original NMOS Z80 (Zilog, NEC, Sharp, etc.)
constexpr uint32_t NMOS_Z80 =
    Z80CoreFlags::UNDOCUMENTED_OPS |
    Z80CoreFlags::NMOS_TIMING      |
    Z80CoreFlags::BLOCK_INT_BUG    |
    Z80CoreFlags::Q_REGISTER;

// CMOS Z80 (Z84C00 series) — no NMOS bugs, same instruction set
constexpr uint32_t CMOS_Z80 =
    Z80CoreFlags::UNDOCUMENTED_OPS |
    Z80CoreFlags::Q_REGISTER;

// Z180/HD64180 — CMOS with extended instructions + MMU
constexpr uint32_t Z180_BASE =
    Z80CoreFlags::Z180_EXTENDED    |
    Z80CoreFlags::HAS_MMU          |
    Z80CoreFlags::HAS_UART         |
    Z80CoreFlags::HAS_DMA          |
    Z80CoreFlags::HAS_PRT          |
    Z80CoreFlags::HAS_WAIT_STATES;

} // namespace CoreFlags

// ============================================================================
// Template Helpers
// ============================================================================

template <const Z80Traits& Traits> constexpr bool has_undocumented_ops() {
    return Traits.has_undocumented_ops();
}

template <const Z80Traits& Traits> constexpr bool has_block_int_bug() {
    return Traits.has_block_int_bug();
}

template <const Z80Traits& Traits> constexpr bool has_mmu() {
    return Traits.has_mmu();
}

template <const Z80Traits& Traits> constexpr bool is_nmos() {
    return Traits.is_nmos();
}

} // namespace detail

using namespace detail;

} // namespace z80
