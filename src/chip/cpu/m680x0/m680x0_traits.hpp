#pragma once
/*
 * m680x0_traits.hpp — CPU Traits System for Motorola 680x0 Family
 *
 * Base types for the M680x0 CPU traits system: core feature flags,
 * M680x0Traits struct, commonly-used flag combinations, and template helpers.
 *
 * Individual CPU trait constants and type aliases live in per-CPU wrapper
 * headers (mc68000.hpp, mc68010.hpp, mc68020.hpp, etc.).
 *
 * Follows the fam65xx/z80 CPUTraits pattern — NTTP-based template dispatch
 * with constexpr feature detection.
 */

#include <cstdint>
#include <type_traits>

namespace m680x0 {

namespace detail {

// ============================================================================
// M680x0 Core Feature Flags (CPU execution behavior only)
// ============================================================================

namespace M680x0CoreFlags {

// === Data Bus Width (bits 0-1) ===
constexpr uint32_t DATA_BUS_8    = 1 << 0;   // MC68008: 8-bit external data bus
constexpr uint32_t DATA_BUS_32   = 1 << 1;   // MC68020+: 32-bit data bus (default 16 if neither set)

// === ISA Extensions (bits 2-7) ===
constexpr uint32_t HAS_VBR       = 1 << 2;   // MC68010+: Vector Base Register
constexpr uint32_t HAS_LOOP_MODE = 1 << 3;   // MC68010: loop mode optimization
constexpr uint32_t HAS_BFLD      = 1 << 4;   // MC68020+: bit field instructions (BFCHG, BFCLR, etc.)
constexpr uint32_t HAS_LONG_BRANCH = 1 << 5; // MC68020+: 32-bit branch displacements
constexpr uint32_t HAS_CAS       = 1 << 6;   // MC68020+: CAS/CAS2 compare-and-swap
constexpr uint32_t HAS_MUL64     = 1 << 7;   // MC68020+: 32×32→64 multiply, 64÷32 divide

// === Hardware Features (bits 8-15) ===
constexpr uint32_t HAS_CACHE     = 1 << 8;   // MC68020+: instruction cache
constexpr uint32_t HAS_DATA_CACHE = 1 << 9;  // MC68030+: data cache
constexpr uint32_t HAS_MMU       = 1 << 10;  // MC68030+: on-chip MMU (PMMU)
constexpr uint32_t HAS_FPU       = 1 << 11;  // MC68040: on-chip FPU
constexpr uint32_t HAS_BARREL    = 1 << 12;  // MC68020+: barrel shifter

// === Addressing Modes (bits 16-19) ===
constexpr uint32_t ADDR_32BIT    = 1 << 16;  // MC68020+: full 32-bit address bus
// (default: 24-bit address bus for 68000/68010)

// === Bus Protocol (bits 20-23) ===
constexpr uint32_t HAS_DYNAMIC_BUS = 1 << 20;  // MC68020+: dynamic bus sizing
constexpr uint32_t HAS_BURST     = 1 << 21;    // MC68040: burst bus transfers

} // namespace M680x0CoreFlags

// ============================================================================
// Complete M680x0 Trait Structure
// ============================================================================

struct M680x0Traits {
    const char* vendor;              // "Motorola", "Freescale", "NXP"
    const char* chip_id;             // "68000", "68008", "68010", "68020", "68030", "68040"
    uint32_t    core_flags;          // M680x0CoreFlags combination
    uint8_t     address_bits;        // Address bus width (20, 22, 24, 32)
    uint8_t     data_bus_bits;       // Data bus width (8, 16, 32)
    uint16_t    max_clock_mhz_x10;  // Max rated clock × 10 (e.g. 80 = 8.0 MHz)
    uint8_t     pin_count;           // Package pin count (48, 52, 64, 114, etc.)

    // === Helper Methods ===

    constexpr bool has(uint32_t flag) const { return (core_flags & flag) != 0; }

    constexpr bool has_vbr()        const { return has(M680x0CoreFlags::HAS_VBR); }
    constexpr bool has_cache()      const { return has(M680x0CoreFlags::HAS_CACHE); }
    constexpr bool has_mmu()        const { return has(M680x0CoreFlags::HAS_MMU); }
    constexpr bool has_fpu()        const { return has(M680x0CoreFlags::HAS_FPU); }
    constexpr bool has_32bit_addr() const { return has(M680x0CoreFlags::ADDR_32BIT); }
    constexpr bool has_bit_fields() const { return has(M680x0CoreFlags::HAS_BFLD); }
    constexpr bool has_long_branch()const { return has(M680x0CoreFlags::HAS_LONG_BRANCH); }
    constexpr bool has_mul64()      const { return has(M680x0CoreFlags::HAS_MUL64); }

    constexpr uint32_t address_mask() const {
        return (address_bits >= 32) ? 0xFFFFFFFF : ~(~0u << address_bits);
    }

    constexpr const char* get_vendor()  const { return vendor; }
    constexpr const char* get_chip_id() const { return chip_id; }

    constexpr bool operator==(const M680x0Traits& other) const {
        return vendor == other.vendor && chip_id == other.chip_id &&
               core_flags == other.core_flags &&
               address_bits == other.address_bits &&
               max_clock_mhz_x10 == other.max_clock_mhz_x10;
    }
    constexpr bool operator!=(const M680x0Traits& other) const { return !(*this == other); }
};

// ============================================================================
// Common Flag Combinations
// ============================================================================

namespace CoreFlags {

// MC68000 (1979) — original 16-bit CPU
constexpr uint32_t MC68000_FLAGS = 0;  // No extended features — baseline

// MC68008 (1982) — 8-bit data bus version
constexpr uint32_t MC68008_FLAGS =
    M680x0CoreFlags::DATA_BUS_8;

// MC68010 (1982) — virtual memory support, loop mode
constexpr uint32_t MC68010_FLAGS =
    M680x0CoreFlags::HAS_VBR |
    M680x0CoreFlags::HAS_LOOP_MODE;

// MC68020 (1984) — full 32-bit, cache, extended ISA
constexpr uint32_t MC68020_FLAGS =
    M680x0CoreFlags::DATA_BUS_32 |
    M680x0CoreFlags::ADDR_32BIT  |
    M680x0CoreFlags::HAS_VBR     |
    M680x0CoreFlags::HAS_BFLD    |
    M680x0CoreFlags::HAS_LONG_BRANCH |
    M680x0CoreFlags::HAS_CAS    |
    M680x0CoreFlags::HAS_MUL64  |
    M680x0CoreFlags::HAS_CACHE  |
    M680x0CoreFlags::HAS_BARREL |
    M680x0CoreFlags::HAS_DYNAMIC_BUS;

// MC68030 (1987) — on-chip MMU + data cache
constexpr uint32_t MC68030_FLAGS =
    MC68020_FLAGS |
    M680x0CoreFlags::HAS_DATA_CACHE |
    M680x0CoreFlags::HAS_MMU;

// MC68040 (1990) — on-chip FPU + burst transfers
constexpr uint32_t MC68040_FLAGS =
    MC68030_FLAGS |
    M680x0CoreFlags::HAS_FPU  |
    M680x0CoreFlags::HAS_BURST;

} // namespace CoreFlags

// ============================================================================
// Template Helpers
// ============================================================================

template <const M680x0Traits& Traits> constexpr bool has_vbr() {
    return Traits.has_vbr();
}

template <const M680x0Traits& Traits> constexpr bool has_32bit_addr() {
    return Traits.has_32bit_addr();
}

template <const M680x0Traits& Traits> constexpr bool has_cache() {
    return Traits.has_cache();
}

template <const M680x0Traits& Traits> constexpr bool has_mmu() {
    return Traits.has_mmu();
}

template <const M680x0Traits& Traits> constexpr bool has_fpu() {
    return Traits.has_fpu();
}

} // namespace detail

using namespace detail;

} // namespace m680x0
