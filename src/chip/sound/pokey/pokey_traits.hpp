#pragma once
/*
 * pokey_traits.hpp — Compile-time traits for the Atari POKEY family
 *
 * The POKEY (Pot Keyboard Integrated Circuit) was designed by Doug
 * Neubauer at Atari and debuted in 1979 as C012294.  It combines
 * 4-channel audio synthesis, keyboard scanning, serial I/O, paddle
 * (potentiometer) reading, timer/counter interrupts, and a random
 * number generator (17/9-bit LFSR) in a single 40-pin DIP.
 *
 * All known revisions share the same register map and functional core;
 * differences are process, packaging, and minor errata.
 *
 * Pattern follows fam65xx CPUTraits / Z80Traits / AYTraits — NTTP via
 * const&, inline constexpr instances, helper methods for zero-overhead
 * feature gating.
 *
 * Revisions covered:
 *   C012294   — Original 1979 NMOS (Atari 400/800, arcades)
 *   C012294B  — Minor NMOS revision
 *   C014795   — 5200 SuperSystem variant (same die)
 *
 * Concrete trait instances live in their respective variant headers:
 *   c012294.hpp, c012294b.hpp, c014795.hpp
 */

#include <cstdint>

namespace pokey {
namespace detail {

// ============================================================================
// POKEY Feature Flags
// ============================================================================

namespace PokeyFlags {

// === Process / Errata (bits 0-7) ===
constexpr uint32_t NMOS              = 1 << 0;  // NMOS process (all production POKEYs)
constexpr uint32_t TIMER_BUG_FIX     = 1 << 1;  // C012294B: timer count-off-by-one fix
constexpr uint32_t INIT_RANDOM_ZERO  = 1 << 2;  // LFSR initializes to 0 instead of 0xFFFF

// === Functional Blocks (bits 8-15) ===
constexpr uint32_t HAS_KEYBOARD      = 1 << 8;  // Keyboard scanning enabled (home computers)
constexpr uint32_t HAS_SERIAL        = 1 << 9;  // Serial I/O enabled (SIO bus)
constexpr uint32_t HAS_POT_INPUTS    = 1 << 10; // Pot (paddle) inputs active
constexpr uint32_t HAS_TIMERS        = 1 << 11; // Timer/counter interrupt system

// === Context (bits 16-19) — where this POKEY lives ===
constexpr uint32_t CONTEXT_ARCADE    = 1 << 16; // Arcade board (no keyboard/SIO)
constexpr uint32_t CONTEXT_HOME      = 1 << 17; // Home computer (full I/O suite)
constexpr uint32_t CONTEXT_5200      = 1 << 18; // 5200 console (pots, no keyboard)

} // namespace PokeyFlags

// ============================================================================
// Common Flag Combinations
// ============================================================================

namespace CoreFlags {

// All production POKEYs are NMOS
constexpr uint32_t NMOS_FULL =
    PokeyFlags::NMOS           |
    PokeyFlags::HAS_KEYBOARD   |
    PokeyFlags::HAS_SERIAL     |
    PokeyFlags::HAS_POT_INPUTS |
    PokeyFlags::HAS_TIMERS     |
    PokeyFlags::CONTEXT_HOME;

// Arcade usage: audio + timers + pots, no keyboard/serial
constexpr uint32_t NMOS_ARCADE =
    PokeyFlags::NMOS           |
    PokeyFlags::HAS_POT_INPUTS |
    PokeyFlags::HAS_TIMERS     |
    PokeyFlags::CONTEXT_ARCADE;

// 5200 SuperSystem: audio + timers + pots, limited keyboard
constexpr uint32_t NMOS_5200 =
    PokeyFlags::NMOS           |
    PokeyFlags::HAS_POT_INPUTS |
    PokeyFlags::HAS_TIMERS     |
    PokeyFlags::CONTEXT_5200;

// Revised NMOS with timer bug fix
constexpr uint32_t NMOS_B =
    NMOS_FULL | PokeyFlags::TIMER_BUG_FIX;

} // namespace CoreFlags

// ============================================================================
// POKEYTraits — compile-time descriptor for each chip variant
// ============================================================================

struct POKEYTraits {
    const char* vendor;               // "Atari"
    const char* chip_id;              // "C012294", "C012294B", etc.
    uint32_t    core_flags;           // PokeyFlags combination
    uint8_t     pin_count;            // 40 (all known variants)

    // === Helpers ===
    constexpr bool has(uint32_t flag)   const { return (core_flags & flag) != 0; }
    constexpr bool is_nmos()            const { return has(PokeyFlags::NMOS); }
    constexpr bool has_keyboard()       const { return has(PokeyFlags::HAS_KEYBOARD); }
    constexpr bool has_serial()         const { return has(PokeyFlags::HAS_SERIAL); }
    constexpr bool has_pot_inputs()     const { return has(PokeyFlags::HAS_POT_INPUTS); }
    constexpr bool has_timers()         const { return has(PokeyFlags::HAS_TIMERS); }
    constexpr bool has_timer_bug_fix()  const { return has(PokeyFlags::TIMER_BUG_FIX); }
    constexpr bool is_arcade()          const { return has(PokeyFlags::CONTEXT_ARCADE); }
    constexpr bool is_home_computer()   const { return has(PokeyFlags::CONTEXT_HOME); }
    constexpr bool is_5200()            const { return has(PokeyFlags::CONTEXT_5200); }

    constexpr const char* get_vendor()  const { return vendor; }
    constexpr const char* get_chip_id() const { return chip_id; }

    constexpr bool operator==(const POKEYTraits& other) const {
        return vendor == other.vendor && chip_id == other.chip_id &&
               core_flags == other.core_flags;
    }
    constexpr bool operator!=(const POKEYTraits& other) const { return !(*this == other); }
};

} // namespace detail

using namespace detail;

} // namespace pokey
