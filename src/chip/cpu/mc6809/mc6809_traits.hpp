#pragma once
/*
 * mc6809_traits.hpp — CPU Traits System for Motorola 6809 Family
 *
 * Base types for the MC6809 CPU traits system: MC6809CoreFlags, enums,
 * MC6809Traits struct, commonly-used flag combinations, and template helpers.
 *
 * Individual CPU trait constants, type aliases, and pin layouts live in
 * per-CPU wrapper headers (motorola_mc6809.hpp, hitachi_hd6309.hpp, etc.).
 *
 * Follows the fam65xx/Z80 CPUTraits pattern — NTTP-based template dispatch
 * with constexpr feature detection.
 */

#include <cstdint>
#include <type_traits>

namespace mc6809 {

namespace detail {

// ============================================================================
// MC6809 Core Feature Flags (CPU execution behavior only)
// ============================================================================

namespace MC6809CoreFlags {

// === Instruction Set (bits 0-7) ===
constexpr uint32_t BASE_6809         = 1 << 0;  // Standard MC6809 instruction set
constexpr uint32_t HD6309_EXTENDED   = 1 << 1;  // HD6309 native-mode extensions (TFM, DIVD, MULD, etc.)
constexpr uint32_t UNDOCUMENTED_OPS  = 1 << 2;  // Undocumented MC6809 opcodes (SWI behavior, etc.)
// Bits 3-7 reserved

// === Hardware Behavior / Quirks (bits 8-11) ===
constexpr uint32_t INTERNAL_CLOCK    = 1 << 8;  // MC6809: E+Q generated internally from EXTAL/XTAL
constexpr uint32_t EXTERNAL_CLOCK    = 1 << 9;  // MC6809E: E+Q are external inputs
constexpr uint32_t HD6309_DIVZERO    = 1 << 10; // HD6309: division-by-zero trap behavior
// Bit 11 reserved

// === Register Extensions (bits 12-15) ===
constexpr uint32_t HAS_W_REGISTER   = 1 << 12; // HD6309: W (E:F), V, zero register, MD
constexpr uint32_t HAS_NATIVE_MODE  = 1 << 13; // HD6309: native mode (MD bit 0)
// Bits 14-15 reserved

// === Bus Features (bits 16-19) ===
constexpr uint32_t HAS_AVMA         = 1 << 16; // Advanced Valid Memory Address
constexpr uint32_t HAS_BUSY         = 1 << 17; // BUSY output (double-byte operations)
constexpr uint32_t HAS_LIC          = 1 << 18; // Last Instruction Cycle output
// Bit 19 reserved

} // namespace MC6809CoreFlags

// ============================================================================
// Complete MC6809 Trait Structure
// ============================================================================

struct MC6809Traits {
    const char* vendor;             // "Motorola", "Hitachi"
    const char* chip_id;            // "MC6809", "MC6809E", "HD6309"
    uint32_t    core_flags;         // MC6809CoreFlags combination
    uint8_t     address_bits;       // Address bus width (always 16)
    uint8_t     max_clock_mhz_x10;  // Max rated clock × 10 (e.g. 10 = 1.0 MHz, 20 = 2.0 MHz)

    // === Helper Methods ===

    constexpr bool has(uint32_t flag) const { return (core_flags & flag) != 0; }

    constexpr bool is_6809()          const { return has(MC6809CoreFlags::BASE_6809); }
    constexpr bool is_hd6309()        const { return has(MC6809CoreFlags::HD6309_EXTENDED); }
    constexpr bool has_internal_clock() const { return has(MC6809CoreFlags::INTERNAL_CLOCK); }
    constexpr bool has_external_clock() const { return has(MC6809CoreFlags::EXTERNAL_CLOCK); }
    constexpr bool has_w_register()   const { return has(MC6809CoreFlags::HAS_W_REGISTER); }
    constexpr bool has_native_mode()  const { return has(MC6809CoreFlags::HAS_NATIVE_MODE); }
    constexpr bool has_avma()         const { return has(MC6809CoreFlags::HAS_AVMA); }
    constexpr bool has_busy()         const { return has(MC6809CoreFlags::HAS_BUSY); }
    constexpr bool has_lic()          const { return has(MC6809CoreFlags::HAS_LIC); }

    constexpr uint32_t address_mask() const { return ~(~0u << address_bits); }

    constexpr const char* get_vendor()  const { return vendor; }
    constexpr const char* get_chip_id() const { return chip_id; }

    constexpr bool operator==(const MC6809Traits& other) const {
        return vendor == other.vendor && chip_id == other.chip_id &&
               core_flags == other.core_flags &&
               address_bits == other.address_bits &&
               max_clock_mhz_x10 == other.max_clock_mhz_x10;
    }
    constexpr bool operator!=(const MC6809Traits& other) const { return !(*this == other); }
};

// ============================================================================
// Common Flag Combinations
// ============================================================================

namespace CoreFlags {

// MC6809 — internal oscillator, standard instruction set
constexpr uint32_t MC6809_BASE =
    MC6809CoreFlags::BASE_6809       |
    MC6809CoreFlags::INTERNAL_CLOCK  |
    MC6809CoreFlags::UNDOCUMENTED_OPS |
    MC6809CoreFlags::HAS_AVMA        |
    MC6809CoreFlags::HAS_BUSY        |
    MC6809CoreFlags::HAS_LIC;

// MC6809E — external clock, standard instruction set
constexpr uint32_t MC6809E_BASE =
    MC6809CoreFlags::BASE_6809       |
    MC6809CoreFlags::EXTERNAL_CLOCK  |
    MC6809CoreFlags::UNDOCUMENTED_OPS |
    MC6809CoreFlags::HAS_AVMA        |
    MC6809CoreFlags::HAS_BUSY        |
    MC6809CoreFlags::HAS_LIC;

// HD6309 — Hitachi enhanced, external clock, extended registers + instructions
constexpr uint32_t HD6309_BASE =
    MC6809CoreFlags::BASE_6809       |
    MC6809CoreFlags::HD6309_EXTENDED |
    MC6809CoreFlags::EXTERNAL_CLOCK  |
    MC6809CoreFlags::HAS_W_REGISTER  |
    MC6809CoreFlags::HAS_NATIVE_MODE |
    MC6809CoreFlags::HAS_AVMA        |
    MC6809CoreFlags::HAS_BUSY        |
    MC6809CoreFlags::HAS_LIC         |
    MC6809CoreFlags::HD6309_DIVZERO;

} // namespace CoreFlags

// ============================================================================
// Template Helpers
// ============================================================================

template <const MC6809Traits& Traits> constexpr bool is_hd6309() {
    return Traits.is_hd6309();
}

template <const MC6809Traits& Traits> constexpr bool has_w_register() {
    return Traits.has_w_register();
}

template <const MC6809Traits& Traits> constexpr bool has_native_mode() {
    return Traits.has_native_mode();
}

} // namespace detail

using namespace detail;

} // namespace mc6809
