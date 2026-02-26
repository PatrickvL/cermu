#pragma once
/*
 * fam65xx_processor_traits.hpp - CPU Traits System for MOS 65xx Family
 *
 * Base types for the 65xx CPU traits system: CPUCoreFlags, enums,
 * CPUTraits struct, commonly-used flag combinations, and template helpers.
 *
 * Individual CPU trait constants, type aliases, and pin layouts live in
 * per-CPU wrapper headers (mos6502.h, mos6510.h, etc.).
 */

#include <cstdint>
#include <type_traits>

namespace fam65xx {

// All trait-related types live in detail:: to keep them out of the public API.
// Internal fam65xx code pulls them in via `using namespace detail;`.
namespace detail {

// ============================================================================
// CPU Core Feature Flags (only CPU execution behavior)
// ============================================================================

// CPUCoreFlags namespace - provides global access to constants for operations
// files
namespace CPUCoreFlags {
// === Instruction Set (bits 0-7) ===
constexpr uint32_t ILLEGAL_OPCODES =
    1 << 0; // ✓ IMPLEMENTED: NMOS undocumented opcodes (LAX, SAX, DCP, ISC, etc.)
constexpr uint32_t CMOS_BASE =
    1 << 1; // ✓ IMPLEMENTED: CMOS instruction set (BRA, STZ, PHX/PLX, PHY/PLY, TRB/TSB, etc.)
constexpr uint32_t ROCKWELL_BITS =
    1 << 2; // ✓ IMPLEMENTED: Rockwell bit manipulation (RMB0-7, SMB0-7, BBR0-7, BBS0-7)
constexpr uint32_t WAI_STP =
    1 << 3; // ✓ IMPLEMENTED: WAI (wait for interrupt) and STP (stop processor)
constexpr uint32_t CE02_EXTENDED =
    1 << 4; // ⚠ NOT IMPLEMENTED: 65CE02 extensions (Z register, PHZ/PLZ, TAZ/TZA, BASE page, etc.)
constexpr uint32_t C816_16BIT =
    1 << 5; // ✓ IMPLEMENTED: 65C816 16-bit mode (native mode, M/X flags, 24-bit addressing)
constexpr uint32_t HUC6280_EXTENDED =
    1 << 6; // ⚠ NOT IMPLEMENTED: HuC6280 unique instructions (TII, TAM, TMA, CSH/CSL, etc.)
// Bit 7 reserved

// === Hardware Bugs (bits 8-11) ===
constexpr uint32_t JMP_INDIRECT_BUG =
    1 << 8; // ✓ IMPLEMENTED: NMOS JMP ($xxFF) wraps within page instead of crossing
constexpr uint32_t RMW_DUMMY_WRITE =
    1 << 9; // ✓ IMPLEMENTED: NMOS RMW operations write original value during dummy cycle (CMOS does dummy read)
// Bits 10-11 reserved for other quirks

// === Decimal Mode (bits 12-15) ===
constexpr uint32_t HAS_DECIMAL_MODE =
    1 << 12; // ✓ IMPLEMENTED: BCD arithmetic supported (2A03/2A07 lack this)
constexpr uint32_t BCD_NMOS_FLAGS =
    1 << 13; // ✓ IMPLEMENTED: N/V/Z flags set from binary result (NMOS) vs BCD result (CMOS)
constexpr uint32_t BCD_EXTRA_CYCLE =
    1 << 14; // ✓ IMPLEMENTED: CMOS processors take extra cycle for ADC/SBC in decimal mode
// Bit 15 reserved

// === Memory & Banking (bits 16-19) ===
constexpr uint32_t HAS_IO_PORT =
    1 << 16; // ✓ IMPLEMENTED: Memory-mapped I/O port at $00-$01 (6510, 7501, 8502)
constexpr uint32_t HAS_BANKING =
    1 << 17; // ⚠ PARTIALLY IMPLEMENTED: Banking/MMU support (defined in BankingType enum, not fully implemented)
// Bits 18-19 reserved

// === Interrupts (bits 20-23) ===
constexpr uint32_t NO_NMI_LINE =
    1 << 20; // ✓ IMPLEMENTED: NMI line disabled/missing (7501)
constexpr uint32_t NO_IRQ_LINE =
    1 << 21; // ✓ IMPLEMENTED: IRQ line disabled/missing (6507 for Atari 2600)
// Bits 22-23 reserved

// === Timing (bits 24-27) ===
constexpr uint32_t OPTIMIZED_CYCLES =
    1 << 24; // ✓ IMPLEMENTED: 65CE02/4510 eliminated dummy cycles for single-cycle operations via improved CMOS timing
constexpr uint32_t VARIABLE_CLOCK =
    1 << 25; // ⚠ NOT IMPLEMENTED: CPU can switch clock speeds (8502, HuC6280)
// Bit 26 reserved (was ACCURATE_INTERNAL_CYCLES - removed, dummy cycles now always simulated)
// Bit 27 reserved (was UPDATE_BUS_LINES - removed, bus updates now always enabled)
} // namespace CPUCoreFlags

// ============================================================================
// Banking/MMU Type (mutually exclusive)
// ============================================================================

enum class BankingType : uint8_t {
  NONE = 0, // Standard 64KB flat addressing
  MOS6509,  // ⚠ NOT IMPLEMENTED: 6509 banking (indirect-Y indexed, 20-bit addressing)
  HUC6280,  // ⚠ NOT IMPLEMENTED: HuC6280 8-bank mapper (21-bit addressing)
  CSG4510,  // ⚠ NOT IMPLEMENTED: 4510 MAP instruction (20-bit addressing with memory mapping)
            // Future: Add more as needed
};

// ============================================================================
// Peripheral Configuration (separate from CPU core)
// ============================================================================

enum class SoundChip : uint8_t {
  NONE = 0,
  RICOH_APU,   // ✓ IMPLEMENTED: 2A03/2A07 5-channel APU (NES)
  HUC6280_PSG, // ⚠ NOT IMPLEMENTED: HuC6280 6-channel PSG (PC Engine/TurboGrafx-16)
               // Future: Others if needed
};

enum class DMAController : uint8_t {
  NONE = 0,
  CSG4510_DMA,    // ⚠ NOT IMPLEMENTED: 4510 integrated DMA (Commodore 65)
  RICOH_5A22_DMA, // ⚠ NOT IMPLEMENTED: 5A22 SNES DMA/HDMA
                  // Future: Others
};

struct PeripheralConfig {
  SoundChip sound;
  DMAController dma;
  bool has_timer; // Simple flag for integrated timer

  constexpr bool has_sound() const { return sound != SoundChip::NONE; }
  constexpr bool has_dma() const { return dma != DMAController::NONE; }
};

// ============================================================================
// Complete CPU Trait Structure
// ============================================================================

struct CPUTraits {
  const char *vendor;          // Manufacturer name (e.g., "MOS Technology", "Ricoh", "WDC")
  const char *chip_id;         // Chip identifier (e.g., "6502", "6510", "2A03")
  uint32_t core_flags;         // CPUCoreFlags combination
  uint8_t address_bits;        // Address bus width (13, 16, 20, 21, 24)
  uint8_t io_port_mask;        // Bitmask of available I/O pins
  BankingType banking;         // Type of banking/MMU
  PeripheralConfig peripheral; // Integrated peripherals

  // === Helper Methods ===

  constexpr bool has(uint32_t flag) const { return (core_flags & flag) != 0; }

  constexpr bool is_nmos() const { return !has(CPUCoreFlags::CMOS_BASE); }

  constexpr bool is_cmos() const { return has(CPUCoreFlags::CMOS_BASE); }

  constexpr bool has_extended_instructions() const {
    return (core_flags & 0x7C) != 0; // Bits 2-6
  }

  constexpr bool has_io_port() const { return io_port_mask != 0; }

  constexpr bool io_pin_exists(uint8_t pin) const {
    return (io_port_mask & (1 << pin)) != 0;
  }

  constexpr bool has_banking() const { return banking != BankingType::NONE; }

  constexpr uint32_t address_mask() const { return ~(~0ull << address_bits); }

  // Feature detection methods (for template compatibility)
  constexpr bool has_illegal_opcodes() const {
    return has(CPUCoreFlags::ILLEGAL_OPCODES);
  }

  // String identification methods
  constexpr const char *get_vendor() const { return vendor; }

  constexpr const char *get_chip_id() const { return chip_id; }

  constexpr bool has_cmos_enhancements() const {
    return has(CPUCoreFlags::CMOS_BASE);
  }

  constexpr bool has_wide_registers() const {
    return has(CPUCoreFlags::C816_16BIT);
  }

  constexpr bool has_bcd() const { return has(CPUCoreFlags::HAS_DECIMAL_MODE); }

  constexpr bool has_nmos_bugs() const {
    return has(CPUCoreFlags::JMP_INDIRECT_BUG) ||
           has(CPUCoreFlags::RMW_DUMMY_WRITE);
  }

  constexpr bool has_apu() const {
    return peripheral.sound == SoundChip::RICOH_APU;
  }

  constexpr bool has_bit_manipulation() const {
    return has(CPUCoreFlags::ROCKWELL_BITS);
  }


  // Equality operator for constexpr comparisons
  // Since CPUTraits instances use string literals, we can use pointer
  // comparison
  constexpr bool operator==(const CPUTraits &other) const {
    return vendor == other.vendor && chip_id == other.chip_id &&
           core_flags == other.core_flags &&
           address_bits == other.address_bits &&
           io_port_mask == other.io_port_mask && banking == other.banking &&
           peripheral.sound == other.peripheral.sound &&
           peripheral.dma == other.peripheral.dma &&
           peripheral.has_timer == other.peripheral.has_timer;
  }

  constexpr bool operator!=(const CPUTraits &other) const {
    return !(*this == other);
  }
};

// ============================================================================
// Common Flag Combinations (DRY principle)
// ============================================================================

namespace CoreFlags {
// NMOS common flags
constexpr uint32_t NMOS_BASE =
    CPUCoreFlags::ILLEGAL_OPCODES | CPUCoreFlags::JMP_INDIRECT_BUG |
    CPUCoreFlags::RMW_DUMMY_WRITE | CPUCoreFlags::HAS_DECIMAL_MODE |
    CPUCoreFlags::BCD_NMOS_FLAGS;

// CMOS common flags
constexpr uint32_t CMOS_BASE_FLAGS =
    CPUCoreFlags::CMOS_BASE | CPUCoreFlags::HAS_DECIMAL_MODE |
    CPUCoreFlags::BCD_EXTRA_CYCLE;

// CMOS with Rockwell extensions
constexpr uint32_t ROCKWELL_BASE =
    CMOS_BASE_FLAGS | CPUCoreFlags::ROCKWELL_BITS;

// Modern WDC (Rockwell + WAI/STP)
constexpr uint32_t WDC_MODERN = ROCKWELL_BASE | CPUCoreFlags::WAI_STP;
} // namespace CoreFlags

// ============================================================================
// Template Helper Functions (for compatibility with existing code)
// ============================================================================

// Helper functions that can be used with CPUTraits instances
template <const CPUTraits &Traits> constexpr bool has_io_port() {
  return Traits.has_io_port();
}

template <const CPUTraits &Traits> constexpr bool has_bcd() {
  return Traits.has_bcd();
}

template <const CPUTraits &Traits> constexpr bool has_illegal_opcodes() {
  return Traits.has_illegal_opcodes();
}

template <const CPUTraits &Traits> constexpr bool has_cmos_enhancements() {
  return Traits.has_cmos_enhancements();
}

template <const CPUTraits &Traits> constexpr bool has_bit_manipulation() {
  return Traits.has_bit_manipulation();
}

template <const CPUTraits &Traits> constexpr bool has_wide_registers() {
  return Traits.has_wide_registers();
}

template <const CPUTraits &Traits> constexpr bool has_nmos_bugs() {
  return Traits.has_nmos_bugs();
}

template <const CPUTraits &Traits> constexpr bool has_apu() {
  return Traits.has_apu();
}

} // namespace detail

// Pull detail:: types into fam65xx scope so internal headers can use
// CPUTraits, CPUCoreFlags, CoreFlags, etc. unqualified.
using namespace detail;

} // namespace fam65xx